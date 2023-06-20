//==-------- EVM.h - Top-level interface for EVM representation --*- C++ -*-==//
//
// This file contains the entry points for global functions defined in
// the LLVM EVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVM_H
#define LLVM_LIB_TARGET_EVM_EVM_H

#include "llvm/MC/TargetRegistry.h"

namespace llvm {
class EVMTargetMachine;
class FunctionPass;
class ModulePass;
class PassRegistry;

FunctionPass *createEVMISelDag(EVMTargetMachine &TM,
                               CodeGenOpt::Level OptLevel);
ModulePass *createEVMLowerIntrinsicsPass();

void initializeEVMLowerIntrinsicsPass(PassRegistry &);
} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVM_H
