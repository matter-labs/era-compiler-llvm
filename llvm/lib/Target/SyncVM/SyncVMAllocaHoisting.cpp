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
#include "llvm/IR/Function.h"
#include "llvm/Transforms/Utils/AllocaHoisting.h"

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

  bool runOnFunction(Function &F) override;
};
} // namespace

bool SyncVMAllocaHoisting::runOnFunction(Function &F) {
  return HoistAllocaToEntry(F);
}

PreservedAnalyses AllocaHoistingPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = HoistAllocaToEntry(F);
  if (!Changed)
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  return PA;
}

char SyncVMAllocaHoisting::ID = 0;

INITIALIZE_PASS(
    SyncVMAllocaHoisting, "syncvm-alloca-hoisting",
    "Hoisting alloca instructions in non-entry blocks to the entry block",
    false, false)

FunctionPass *llvm::createSyncVMAllocaHoistingPass() {
  return new SyncVMAllocaHoisting;
}
