//===-- SyncVMIRIdiomRecognition.cpp - Revert unsupported instcombine changes -===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IntrinsicsSyncVM.h"

#include "SyncVM.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-ir-idiom"

namespace llvm {
FunctionPass *createSyncVMIRIdiomRecognition();
void initializeSyncVMIRIdiomRecognitionPass(PassRegistry &);
} // namespace llvm

namespace {
struct SyncVMIRIdiomRecognition : public FunctionPass {
public:
  static char ID;
  SyncVMIRIdiomRecognition() : FunctionPass(ID) {
    initializeSyncVMIRIdiomRecognitionPass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  bool findAndReplaceMulMod(Instruction &I);

  StringRef getPassName() const override {
    return "SyncVM IR idiom recognization pass";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // namespace

char SyncVMIRIdiomRecognition::ID = 0;

INITIALIZE_PASS(SyncVMIRIdiomRecognition, "syncvm-ir-idiom",
                "Recoginze IR patterns and make replacements", false, false)

bool SyncVMIRIdiomRecognition::findAndReplaceMulMod(Instruction &I) {
  auto *CI = dyn_cast<CallInst>(&I);
  if (!CI)
    return false;

  if (CI->getCalledFunction()->getName() != "__mulmod")
    return false;

  // This pass is executed after constant folding, where it will try to fold
  // mulmod if all arguments are known. So if we happen to see the 3rd argument
  // is a constant, we know it's not folded.
  auto *C = dyn_cast<ConstantInt>(CI->getArgOperand(2));
  if (!C)
    return false;

  LLVM_DEBUG(dbgs() << "Found foldable call to `__mulmod`: " << I << "\n");

  auto *Module = CI->getModule();
  llvm::Function *NewFunction = [&]() {
    llvm::FunctionType *FuncType = CI->getFunctionType();
    return llvm::Function::Create(FuncType, llvm::Function::ExternalLinkage,
                                  "__mulmod_replaced", Module);
  }();
  std::vector<llvm::Value*> Args(CI->arg_begin(), CI->arg_end());
  llvm::CallInst *NewCall = llvm::CallInst::Create(NewFunction, Args, "", CI);
  CI->replaceAllUsesWith(NewCall);
  
  return true;
}

bool SyncVMIRIdiomRecognition::runOnFunction(Function &F) {
  std::vector<Instruction *> Replaced;

  // TODO
  for (auto &BB : F)
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      auto &I = *II;

      bool IsReplaced = findAndReplaceMulMod(I);
      if (IsReplaced)
        Replaced.push_back(&I);
    }

  for (auto *I : Replaced)
    I->eraseFromParent();

  return !Replaced.empty();
}

FunctionPass *llvm::createSyncVMIRIdiomRecognitionPass() {
  return new SyncVMIRIdiomRecognition();
}
