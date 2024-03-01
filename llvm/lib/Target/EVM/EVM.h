//==-------- EVM.h - Top-level interface for EVM representation --*- C++ -*-==//
//
// This file contains the entry points for global functions defined in
// the LLVM EVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVM_H
#define LLVM_LIB_TARGET_EVM_EVM_H

#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"

namespace llvm {
class EVMTargetMachine;
class FunctionPass;
class ModulePass;
class PassRegistry;

namespace EVMAS {
// EVM address spaces
enum AddressSpaces {
  AS_STACK = 0,
  AS_HEAP = 1,
  AS_CALL_DATA = 2,
  AS_RETURN_DATA = 3,
  AS_CODE = 4,
  AS_STORAGE = 5
};
} // namespace EVMAS

// LLVM IR passes.
ModulePass *createEVMLowerIntrinsicsPass();
FunctionPass *createEVMCodegenPreparePass();

// ISel and immediate followup passes.
FunctionPass *createEVMISelDag(EVMTargetMachine &TM,
                               CodeGenOpt::Level OptLevel);
FunctionPass *createEVMArgumentMove();
FunctionPass *createEVMAllocaHoistingPass();
ModulePass *createEVMLinkRuntimePass();

// Late passes.
FunctionPass *createEVMOptimizeLiveIntervals();
FunctionPass *createEVMRegColoring();
FunctionPass *createEVMSingleUseExpression();
FunctionPass *createEVMStackify();

// PassRegistry initialization declarations.
void initializeEVMCodegenPreparePass(PassRegistry &);
void initializeEVMAllocaHoistingPass(PassRegistry &);
void initializeEVMLowerIntrinsicsPass(PassRegistry &);
void initializeEVMArgumentMovePass(PassRegistry &);
void initializeEVMLinkRuntimePass(PassRegistry &);
void initializeEVMOptimizeLiveIntervalsPass(PassRegistry &);
void initializeEVMRegColoringPass(PassRegistry &);
void initializeEVMSingleUseExpressionPass(PassRegistry &);
void initializeEVMStackifyPass(PassRegistry &);

struct EVMLinkRuntimePass : PassInfoMixin<EVMLinkRuntimePass> {
  EVMLinkRuntimePass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

struct EVMAllocaHoistingPass : PassInfoMixin<EVMAllocaHoistingPass> {
  EVMAllocaHoistingPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVM_H
