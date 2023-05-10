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
                               CodeGenOptLevel OptLevel);
FunctionPass *createEVMArgumentMove();
ModulePass *createEVMLinkRuntimePass();

// PassRegistry initialization declarations.
void initializeEVMCodegenPreparePass(PassRegistry &);
void initializeEVMLowerIntrinsicsPass(PassRegistry &);
void initializeEVMArgumentMovePass(PassRegistry &);
void initializeEVMLinkRuntimePass(PassRegistry &);

struct EVMLinkRuntimePass : PassInfoMixin<EVMLinkRuntimePass> {
  EVMLinkRuntimePass() {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVM_H
