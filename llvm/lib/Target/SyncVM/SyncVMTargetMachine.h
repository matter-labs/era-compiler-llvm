//===-- SyncVMTargetMachine.h - Define TargetMachine for SyncVM -*- C++ -*-===//
//
// This file declares the SyncVM specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMTARGETMACHINE_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMTARGETMACHINE_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

/// SyncVMTargetMachine
///
class SyncVMTargetMachine : public LLVMTargetMachine {

public:
  SyncVMTargetMachine(
    const Target &T,
    const Triple &TT,
    StringRef CPU,
    StringRef FS,
    const TargetOptions &Options,
    Optional<Reloc::Model> RM,
    Optional<CodeModel::Model> CM,
    CodeGenOpt::Level OL,
    bool JIT
  );
  ~SyncVMTargetMachine() override;
  
}; // SyncVMTargetMachine.

} // end namespace llvm

#endif
