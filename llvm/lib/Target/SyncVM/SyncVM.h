//==-- SyncVM.h - Top-level interface for SyncVM representation --*- C++ -*-==//
//
// This file contains the entry points for global functions defined in
// the LLVM SyncVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVM_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVM_H

#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
  class SyncVMTargetMachine;

  FunctionPass *createSyncVMISelDag(SyncVMTargetMachine &TM,
                                    CodeGenOpt::Level OptLevel);

} // end namespace llvm;

#endif
