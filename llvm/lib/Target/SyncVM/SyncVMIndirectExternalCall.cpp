//===-- SyncVMIndirectExternalCall.cpp - Wrap an EC into a function call --===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsSyncVM.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "SyncVM.h"

using namespace llvm;

namespace llvm {
ModulePass *createIndirectExternallCallPass();
void initializeSyncVMIndirectExternalCallPass(PassRegistry &);
} // namespace llvm

namespace {
/// Extract patterns like ltflag(external_call) to a function which returns
/// 0 iff ltflag is not set.
/// The pass is to reduce code size and prevent further optimizations from
/// meddling.
/// TODO: The overall exception handling design shall be redone, and the pass
/// is to removed after that point.
struct SyncVMIndirectExternalCall : public ModulePass {
public:
  static char ID;
  SyncVMIndirectExternalCall() : ModulePass(ID) {
    initializeSyncVMIndirectExternalCallPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Wrap an external call and its result check into a function call";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
  Function *getOrCreateIntrinsicWrapper(unsigned ID, Module &M);
};
} // namespace

char SyncVMIndirectExternalCall::ID = 0;
static const std::string WrapperNames[4] = {"__farcall", "__delegatecall",
                                            "__callcode", "__staticcall"};

INITIALIZE_PASS(SyncVMIndirectExternalCall, "syncvm-indirect-external-call",
                "Wrap an external call into a fuction call", false, false)

bool SyncVMIndirectExternalCall::runOnModule(Module &M) {
  std::vector<Instruction *> Replaced;
  std::vector<IntrinsicInst *> Calls[4];
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        if (auto *Call = dyn_cast<IntrinsicInst>(&I)) {

          Intrinsic::ID ID = Call->getIntrinsicID();
          switch (ID) {
          default:
            continue;
          case Intrinsic::syncvm_farcall_rc:
          case Intrinsic::syncvm_delegatecall_rc:
          case Intrinsic::syncvm_callcode_rc:
          case Intrinsic::syncvm_staticcall_rc: {
            Function *Replacement = getOrCreateIntrinsicWrapper(ID, M);
            IRBuilder<> Builder(&I);
            Value *Result = Builder.CreateCall(Replacement, {I.getOperand(0)});
            I.replaceAllUsesWith(Result);
            Replaced.push_back(&I);
          }
          }
        }
      }
  for (Instruction *I : Replaced)
    I->eraseFromParent();

  return !Replaced.empty();
}

Function *SyncVMIndirectExternalCall::getOrCreateIntrinsicWrapper(unsigned ID,
                                                                  Module &M) {
#if 0
  LLVMContext &C = M.getContext();
  Type *Int256Ty = Type::getInt256Ty(C);
  auto getOrCreateFunction = [&M, &C, Int256Ty](const std::string &Name,
                                                unsigned IntrinsicID) {
    Function *Result = M.getFunction(Name);
    if (Result)
      return Result;
    M.getOrInsertFunction(Name, FunctionType::get(Int256Ty, {Int256Ty}, false));
    Result = M.getFunction(Name);
    auto *Entry = BasicBlock::Create(C, "entry", Result);
    auto *SuccessBB = BasicBlock::Create(C, "success-bb", Result);
    auto *EHBB = BasicBlock::Create(C, "eh-bb", Result);
    IRBuilder<> Builder(Entry);
    auto *FarCallFn = Intrinsic::getDeclaration(&M, IntrinsicID);
    auto *LTFlagFn = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_ltflag);
    Builder.CreateCall(FarCallFn, {Result->getArg(0)});
    Value *ErrorCode = Builder.CreateCall(LTFlagFn, {});
    ErrorCode = Builder.CreateTrunc(ErrorCode, Builder.getInt1Ty());
    Builder.CreateCondBr(ErrorCode, EHBB, SuccessBB);
    Builder.SetInsertPoint(EHBB);
    Builder.CreateRet(Builder.getInt(APInt(256, 0, false)));
    Builder.SetInsertPoint(SuccessBB);
    Builder.CreateRet(Builder.getInt(APInt(256, 1, false)));
    return Result;
  };
  Function *Result = nullptr;
  switch (ID) {
  default:
    llvm_unreachable("Unrecognized intrinsic");
    break;
  case Intrinsic::syncvm_farcall_rc:
    Result = getOrCreateFunction(WrapperNames[0], Intrinsic::syncvm_farcall);
    break;
  case Intrinsic::syncvm_delegatecall_rc:
    Result =
        getOrCreateFunction(WrapperNames[1], Intrinsic::syncvm_delegatecall);
    break;
  case Intrinsic::syncvm_callcode_rc:
    Result = getOrCreateFunction(WrapperNames[2], Intrinsic::syncvm_callcode);
    break;
  case Intrinsic::syncvm_staticcall_rc:
    Result = getOrCreateFunction(WrapperNames[3], Intrinsic::syncvm_staticcall);
    break;
  }
  assert(Result && "Result must be set at this point");
  return Result;
#endif
  return {};
}

ModulePass *llvm::createSyncVMIndirectExternalCallPass() {
  return new SyncVMIndirectExternalCall();
}
