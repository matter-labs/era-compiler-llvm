//==-------- EVM.h - Top-level interface for EVM representation --*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in
// the LLVM EVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVM_H
#define LLVM_LIB_TARGET_EVM_EVM_H

#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Pass.h"

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
  AS_STORAGE = 5,
  AS_TSTORAGE = 6,
  MAX_ADDRESS = AS_TSTORAGE
};
} // namespace EVMAS

namespace EVMCOST {
unsigned constexpr SWAP = 3;
unsigned constexpr DUP = 3;
unsigned constexpr POP = 2;
unsigned constexpr PUSH = 3;
unsigned constexpr MLOAD = 3;
} // namespace EVMCOST

// LLVM IR passes.
ModulePass *createEVMLowerIntrinsicsPass();
FunctionPass *createEVMCodegenPreparePass();
ImmutablePass *createEVMAAWrapperPass();
ImmutablePass *createEVMExternalAAWrapperPass();

// ISel and immediate followup passes.
FunctionPass *createEVMISelDag(EVMTargetMachine &TM,
                               CodeGenOptLevel OptLevel);
FunctionPass *createEVMArgumentMove();
FunctionPass *createEVMAllocaHoistingPass();
ModulePass *createEVMLinkRuntimePass();

// Late passes.
FunctionPass *createEVMOptimizeLiveIntervals();
FunctionPass *createEVMRegColoring();
FunctionPass *createEVMSingleUseExpression();
FunctionPass *createEVMSplitCriticalEdges();
FunctionPass *createEVMStackify();
FunctionPass *createEVMBPStackification();
FunctionPass *createEVMLowerJumpUnless();
ModulePass *createEVMFinalizeStackFrames();
ModulePass *createEVMMarkRecursiveFunctionsPass();

// PassRegistry initialization declarations.
void initializeEVMCodegenPreparePass(PassRegistry &);
void initializeEVMAllocaHoistingPass(PassRegistry &);
void initializeEVMLowerIntrinsicsPass(PassRegistry &);
void initializeEVMArgumentMovePass(PassRegistry &);
void initializeEVMLinkRuntimePass(PassRegistry &);
void initializeEVMOptimizeLiveIntervalsPass(PassRegistry &);
void initializeEVMRegColoringPass(PassRegistry &);
void initializeEVMSingleUseExpressionPass(PassRegistry &);
void initializeEVMSplitCriticalEdgesPass(PassRegistry &);
void initializeEVMStackifyPass(PassRegistry &);
void initializeEVMBPStackificationPass(PassRegistry &);
void initializeEVMAAWrapperPassPass(PassRegistry &);
void initializeEVMExternalAAWrapperPass(PassRegistry &);
void initializeEVMLowerJumpUnlessPass(PassRegistry &);
void initializeEVMFinalizeStackFramesPass(PassRegistry &);
void initializeEVMMarkRecursiveFunctionsPass(PassRegistry &);

struct EVMLinkRuntimePass : PassInfoMixin<EVMLinkRuntimePass> {
  EVMLinkRuntimePass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

struct EVMAllocaHoistingPass : PassInfoMixin<EVMAllocaHoistingPass> {
  EVMAllocaHoistingPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

struct EVMSHA3ConstFoldingPass : PassInfoMixin<EVMSHA3ConstFoldingPass> {
  EVMSHA3ConstFoldingPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

struct EVMMarkRecursiveFunctionsPass
    : PassInfoMixin<EVMMarkRecursiveFunctionsPass> {
  EVMMarkRecursiveFunctionsPass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVM_H
