//==-------- EVM.h - Top-level interface for EVM representation --*- C++ -*-==//
//
// This file contains the entry points for global functions defined in
// the LLVM EVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVM_H
#define LLVM_LIB_TARGET_EVM_EVM_H

#include "llvm/Target/TargetMachine.h"

namespace llvm {
class EVMTargetMachine;
class FunctionPass;

FunctionPass *createEVMISelDag(EVMTargetMachine &TM,
                               CodeGenOpt::Level OptLevel);
} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVM_H
