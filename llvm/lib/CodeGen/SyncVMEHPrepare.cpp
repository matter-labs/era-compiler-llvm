//===------ SyncVMEHPrepare - Prepare excepton handling for SyncVM --------===//
//
// This transformation is designed for use by code generators which use
// SyncVM exception handling scheme.
// The implementation is inherited from WebAssembly.
//
// At the moment SyncVM only supports limited capacity for exception handling:
// - a throwing function can only inform that it throws without passing any
// additional info about the failure
// - an exception can only be propagated, not caught
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

namespace {
class SyncVMEHPrepare : public FunctionPass {
  Function *ThrowF = nullptr;       // syncvm.throw() intrinsic

  bool prepareEHPads(Function &F);
  bool prepareThrows(Function &F);

  bool IsEHPadFunctionsSetUp = false;
  void prepareEHPad(BasicBlock *BB, bool NeedPersonality, bool NeedLSDA = false,
                    unsigned Index = 0);

public:
  static char ID; // Pass identification, replacement for typeid

  SyncVMEHPrepare() : FunctionPass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool doInitialization(Module &M) override;
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return "SyncVM Exception handling preparation";
  }
};
} // end anonymous namespace

char SyncVMEHPrepare::ID = 0;
INITIALIZE_PASS_BEGIN(SyncVMEHPrepare, DEBUG_TYPE,
                      "Prepare SyncVM exceptions", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(SyncVMEHPrepare, DEBUG_TYPE,
                    "Prepare SyncVM exceptions", false, false)

FunctionPass *llvm::createSyncVMEHPass() { return new SyncVMEHPrepare(); }

void SyncVMEHPrepare::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DominatorTreeWrapperPass>();
}

bool SyncVMEHPrepare::doInitialization(Module &M) {
  return false;
}

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

bool SyncVMEHPrepare::runOnFunction(Function &F) {
  IsEHPadFunctionsSetUp = false;
  bool Changed = false;
  Changed |= prepareThrows(F);
  Changed |= prepareEHPads(F);
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

bool SyncVMEHPrepare::prepareEHPads(Function &F) {
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

      // Remove lpad instruction in the unwind BB.
      auto UnwindBB = II->getUnwindDest();
      if (!UnwindBB->empty() && isa<LandingPadInst>(UnwindBB->front()))
        UnwindBB->front().eraseFromParent();

      Changed = true;
    }
  return Changed;
}
