//===-- AllocaHoisting.cpp - Hoist allocas to the entry block --*- C++ -*-===//
//===----------------------------------------------------------------------===//
//
// Hoist the alloca instructions in the non-entry blocks to the entry blocks.
// Copied from NVPTX backend.
//
//===----------------------------------------------------------------------===//
// TODO: The pass is a shortcut solution. DYNAMIC_STACKALLOC suppoer is needed
// instead.

#include "SyncVM.h"

#include "llvm/CodeGen/StackProtector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {
// Hoisting the alloca instructions in the non-entry blocks to the entry
// block.
class SyncVMAllocaHoisting : public FunctionPass {
public:
  static char ID; // Pass ID
  SyncVMAllocaHoisting() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<StackProtector>();
  }

  StringRef getPassName() const override {
    return "SyncVM specific alloca hoisting";
  }

  bool runOnFunction(Function &function) override;
};
} // namespace

bool SyncVMAllocaHoisting::runOnFunction(Function &function) {
  bool functionModified = false;
  Function::iterator I = function.begin();
  Instruction *firstTerminatorInst = (I++)->getTerminator();

  for (Function::iterator E = function.end(); I != E; ++I) {
    for (BasicBlock::iterator BI = I->begin(), BE = I->end(); BI != BE;) {
      AllocaInst *allocaInst = dyn_cast<AllocaInst>(BI++);
      if (allocaInst && isa<ConstantInt>(allocaInst->getArraySize())) {
        allocaInst->moveBefore(firstTerminatorInst);
        functionModified = true;
      }
    }
  }

  return functionModified;
}

char SyncVMAllocaHoisting::ID = 0;

INITIALIZE_PASS(
    SyncVMAllocaHoisting, "syncvm-alloca-hoisting",
    "Hoisting alloca instructions in non-entry blocks to the entry block",
    false, false)

FunctionPass *llvm::createSyncVMAllocaHoistingPass() {
  return new SyncVMAllocaHoisting;
}
