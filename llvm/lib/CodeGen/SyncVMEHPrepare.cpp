//===------ SyncVMEHPrepare - Prepare excepton handling for SyncVM --------===//
//
// This transformation is designed for use by code generators which use
// SyncVM exception handling scheme.
// The implementation is inherited from WebAssembly.
//
// At the moment SyncVM only supports limited capacity for exception handling:
// - a throwing function can only inform that it throws without passing any
// additional info about the failure
//===----------------------------------------------------------------------===//

#include "llvm/ADT/BreadthFirstIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsSyncVM.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "syncvmehprepare"

const std::string SStoreWrapperName = "__sstore";

static cl::opt<bool> AggresivelyReplaceIntrinsicInvokes(
    "syncvm-replace-invoke-intrinsics-aggressively",
    cl::desc("If intrinsic invokation is to be replaced with calls"),
    cl::init(true), cl::Hidden);

namespace {
/// Lower exception handling for instruction selection.
/// After the pass a well-formed module should not contain any invoke or landing
/// pad instruction.
class SyncVMEHPrepare : public FunctionPass {
  Function *ThrowF = nullptr; // syncvm.throw() intrinsic

  /// Remove landing pad instructions from an exception handling basic block.
  /// After replacing invokes with calls and branches, landing pads are no
  /// longer valid, nor they are needed since SyncVM does not allow to
  /// differnetiate between types of errors when selecting the landing pad.
  bool removeLPad(Function &F);
  /// Replace invoke __cxa_throw, success_bb, unwind_bb with br unwind_bb.
  /// __cxa_trow always throws and SyncVM does not differentiate between
  /// excaptions.
  bool replaceCXAThrow(Function &F);
  /// Replace invoke function, success_bb, unwind_bb with call function + jtl
  /// unwind_bb, success_bb.
  bool replaceInvokeFunction(Function &F);
  /// Replace invoke invokable instrinsics (e.g. sstore) with corresponding
  /// logic with call and br instructions only.
  bool replaceInvokeIntrinsic(Function &F);
  /// Replace all instructions followed after a throw with unreachable.
  bool prepareThrows(Function &F);

public:
  static char ID; // Pass identification, replacement for typeid

  SyncVMEHPrepare() : FunctionPass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
  }
  /// Declare intrinsic wrappers.
  bool doInitialization(Module &M) override;
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return "SyncVM Exception handling preparation";
  }
};
} // end anonymous namespace

char SyncVMEHPrepare::ID = 0;
INITIALIZE_PASS_BEGIN(SyncVMEHPrepare, DEBUG_TYPE, "Prepare SyncVM exceptions",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(SyncVMEHPrepare, DEBUG_TYPE, "Prepare SyncVM exceptions",
                    false, false)

FunctionPass *llvm::createSyncVMEHPass() { return new SyncVMEHPrepare(); }

// Erase the specified BBs if the BB does not have any remaining predecessors,
// and also all its dead children.
template <typename Container>
static void eraseDeadBBsAndChildren(const Container &BBs, DomTreeUpdater *DTU) {
  SmallVector<BasicBlock *, 8> WL(BBs.begin(), BBs.end());
  while (!WL.empty()) {
    auto *BB = WL.pop_back_val();
    if (pred_begin(BB) != pred_end(BB))
      continue;
    WL.append(succ_begin(BB), succ_end(BB));
    DeleteDeadBlock(BB, DTU);
  }
}

bool SyncVMEHPrepare::doInitialization(Module &M) {
  if (M.getFunction(SStoreWrapperName))
    return false;
  LLVMContext &C = M.getContext();
  auto *Int256Ty = Type::getInt256Ty(C);
  auto *Ty = FunctionType::get(
      Type::getVoidTy(C), {Int256Ty, Int256Ty, Int256Ty}, false /* IsVarArg */);
  // FIXME: The function declaration is removed by the time it's needed.
  M.getOrInsertFunction(SStoreWrapperName, Ty);
  return true;
}

bool SyncVMEHPrepare::runOnFunction(Function &F) {
  bool Changed = false;
  Changed |= replaceCXAThrow(F);
  return Changed;
  Changed |= prepareThrows(F);
  Changed |= replaceInvokeIntrinsic(F);
  Changed |= replaceInvokeFunction(F);
  Changed |= removeLPad(F);
  return Changed;
}

bool SyncVMEHPrepare::replaceCXAThrow(Function &F) {
  bool Changed = false;
  Module &M = *F.getParent();
  ThrowF = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_throw);
  for (auto &BB : F)
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      auto &Inst = *II;
      auto *Call = dyn_cast<CallInst>(&Inst);
      auto *Invoke = dyn_cast<InvokeInst>(&Inst);
      if (Call || Invoke) {
        auto CallSite =
            Call ? Call->getCalledOperand() : Invoke->getCalledOperand();
        auto CSGV = dyn_cast<GlobalValue>(CallSite);
        if (CSGV && CSGV->getGlobalIdentifier() == "__cxa_throw") {
          IRBuilder<> Builder(&Inst);
          if (Call) {
            auto Op = Builder.CreatePtrToInt(Call->getOperand(0),
                                             Builder.getInt256Ty());
            CallInst *ThrowFCall =
                Builder.CreateCall(ThrowF, {Op});
            Inst.replaceAllUsesWith(ThrowFCall);
          } else if (Invoke) {
            Builder.CreateBr(Invoke->getUnwindDest());
            Invoke->getUnwindDest()->front().eraseFromParent();
          }
          ++II;
          Inst.eraseFromParent();
          Changed = true;
        }
      }
    }
  return Changed;
}

bool SyncVMEHPrepare::replaceInvokeFunction(Function &F) {
  bool Changed = false;
  for (BasicBlock &BB : F)
    if (InvokeInst *II = dyn_cast<InvokeInst>(BB.getTerminator())) {
      SmallVector<Value *, 16> CallArgs(II->arg_begin(), II->arg_end());
      SmallVector<OperandBundleDef, 1> OpBundles;
      II->getOperandBundlesAsDefs(OpBundles);
      // Insert a normal call instruction...
      CallInst *NewCall =
          CallInst::Create(II->getFunctionType(), II->getCalledOperand(),
                           CallArgs, OpBundles, "", II);
      NewCall->takeName(II);
      NewCall->setCallingConv(II->getCallingConv());
      NewCall->setAttributes(II->getAttributes());
      NewCall->setDebugLoc(II->getDebugLoc());
      II->replaceAllUsesWith(NewCall);

      auto *Fn =
          Intrinsic::getDeclaration(F.getParent(), Intrinsic::syncvm_ltflag);
      IRBuilder<> Builder(II);
      CallInst *LTFlag = Builder.CreateCall(Fn, {});
      auto *TruncFlag = Builder.CreateTrunc(LTFlag, Builder.getInt1Ty());

      // Insert an unconditional branch to the normal destination.
      Builder.CreateCondBr(TruncFlag, II->getUnwindDest(), II->getNormalDest());

      // Remove the invoke instruction now.
      BB.getInstList().erase(II);

      Changed = true;
    }
  return Changed;
}

bool SyncVMEHPrepare::replaceInvokeIntrinsic(Function &F) {
  bool Changed = false;
  Module &M = *F.getParent();
  ThrowF = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_throw);
  for (auto &BB : F)
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      auto &Inst = *II;
      auto *Invoke = dyn_cast<InvokeInst>(&Inst);
      if (!Invoke)
        continue;
      Function *CallSite = Invoke->getCalledFunction();
      if (!CallSite->isIntrinsic())
        continue;
      if (CallSite->getIntrinsicID() != Intrinsic::syncvm_sstore)
        continue;
      IRBuilder<> Builder(&Inst);
      if (AggresivelyReplaceIntrinsicInvokes) {
        Builder.CreateCall(
            CallSite->getFunctionType(), CallSite,
            {Inst.getOperand(0), Inst.getOperand(1), Inst.getOperand(2)});
        Builder.CreateBr(Invoke->getNormalDest());
      } else {
        Module *M = F.getParent();
        Function *SStore = M->getFunction(SStoreWrapperName);
        assert(SStore && "__sstore must be declared by this point");
        Builder.CreateInvoke(
            SStore->getFunctionType(), SStore, Invoke->getNormalDest(),
            Invoke->getUnwindDest(),
            {Inst.getOperand(0), Inst.getOperand(1), Inst.getOperand(2)});
      }
      ++II;
      Invoke->eraseFromParent();
      Changed = true;
    }
  return Changed;
}

bool SyncVMEHPrepare::prepareThrows(Function &F) {
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  DomTreeUpdater DTU(&DT, /*PostDominatorTree*/ nullptr,
                     DomTreeUpdater::UpdateStrategy::Eager);
  Module &M = *F.getParent();
  IRBuilder<> IRB(F.getContext());
  bool Changed = false;

  // syncvm.throw() intinsic, which will be lowered to syncvm 'throw'
  // instruction.
  ThrowF = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_throw);
  // Insert an unreachable instruction after a call to @llvm.syncvm.throw and
  // delete all following instructions within the BB, and delete all the dead
  // children of the BB as well.
  for (User *U : ThrowF->users()) {
    // A call to @llvm.syncvm.throw() can not be an InvokeInst.
    auto *ThrowI = cast<CallInst>(U);
    if (ThrowI->getFunction() != &F)
      continue;
    Changed = true;
    auto *BB = ThrowI->getParent();
    SmallVector<BasicBlock *, 4> Succs(succ_begin(BB), succ_end(BB));
    auto &InstList = BB->getInstList();
    InstList.erase(std::next(BasicBlock::iterator(ThrowI)), InstList.end());
    IRB.SetInsertPoint(BB);
    IRB.CreateUnreachable();
    eraseDeadBBsAndChildren(Succs, &DTU);
  }

  return Changed;
}

bool SyncVMEHPrepare::removeLPad(Function &F) {
  std::vector<Instruction *> EraseInst;
  for (auto &BB : F) {
    if (BB.empty())
      continue;
    auto &Inst = BB.front();
    if (isa<LandingPadInst>(Inst))
      EraseInst.push_back(&Inst);
  }
  for (auto *EI : EraseInst)
    EI->eraseFromParent();
  return !EraseInst.empty();
}
