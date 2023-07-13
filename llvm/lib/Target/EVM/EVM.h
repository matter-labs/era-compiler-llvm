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

namespace EVMAS {
// EVM address spaces
enum AddressSpaces {
  AS_STACK = 0,
  AS_HEAP = 1,
  AS_CALL_DATA = 2,
  AS_RETURN_DATA = 3,
  AS_CODE = 4,
};
} // namespace EVMAS

// LLVM IR passes.
ModulePass *createEVMLowerIntrinsicsPass();

// ISel and immediate followup passes.
FunctionPass *createEVMISelDag(EVMTargetMachine &TM,
                               CodeGenOpt::Level OptLevel);
FunctionPass *createEVMArgumentMove();

// PassRegistry initialization declarations.
void initializeEVMLowerIntrinsicsPass(PassRegistry &);
void initializeEVMArgumentMovePass(PassRegistry &);
} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVM_H
