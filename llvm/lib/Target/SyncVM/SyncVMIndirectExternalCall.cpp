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
  Function *getSSTOREFunction(Module &M);
};
} // namespace

char SyncVMIndirectExternalCall::ID = 0;
static const std::string WrapperNames[4] = {"__farcall", "__delegatecall",
                                            "__callcode", "__staticcall"};

INITIALIZE_PASS(SyncVMIndirectExternalCall, "syncvm-indirect-external-call",
                "Wrap an external call into a fuction call", false, false)

bool SyncVMIndirectExternalCall::runOnModule(Module &M) {
  bool changed = false;
  std::vector<IntrinsicInst *> Calls[4];
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        if (auto *II = dyn_cast<InvokeInst>(&I)) {
          Intrinsic::ID ID = II->getIntrinsicID();
          switch (ID) {
          default:
            continue;
          case Intrinsic::syncvm_sstore: {
            // redirect the invoke to the wrapper function so we can keep the unwind label.
            Function *Replacement = getSSTOREFunction(M);
            II->setCalledFunction(Replacement);
            changed = true;
            break;
          }
          }

        }
      }

  return changed;
}

Function* SyncVMIndirectExternalCall::getSSTOREFunction(Module &M) {
  LLVMContext &C = M.getContext();
  Type *Int256Ty = Type::getInt256Ty(C);
  Type *VoidTy = Type::getVoidTy(C);

  auto name = "__sstore";

  Function *Result = M.getFunction(name);
  if (Result)
    return Result;

  M.getOrInsertFunction(name, FunctionType::get(VoidTy, {Int256Ty, Int256Ty, Int256Ty}, false));
  Result = M.getFunction(name);
  assert(Result);
  return Result;
}

ModulePass *llvm::createSyncVMIndirectExternalCallPass() {
  return new SyncVMIndirectExternalCall();
}
