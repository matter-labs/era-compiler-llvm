//===-- SyncVMTargetMachine.h - Define TargetMachine for SyncVM -*- C++ -*-===//
//
// This file declares the SyncVM specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMTARGETMACHINE_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMTARGETMACHINE_H

#include "SyncVMSubtarget.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

/// SyncVMTargetMachine
///
class SyncVMTargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  SyncVMSubtarget Subtarget;

public:
  SyncVMTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT);
  ~SyncVMTargetMachine() override;

  const SyncVMSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  TargetTransformInfo getTargetTransformInfo(const Function &F) override;

}; // SyncVMTargetMachine.

} // end namespace llvm

#endif
