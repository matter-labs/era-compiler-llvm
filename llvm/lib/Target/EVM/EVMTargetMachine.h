//===---- EVMTargetMachine.h - Define TargetMachine for EVM -*- C++ -*----===//
//
// This file declares the EVM specific subclass of TargetMachine.
//
//===---------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMTARGETMACHINE_H
#define LLVM_LIB_TARGET_EVM_EVMTARGETMACHINE_H

#include "EVMSubtarget.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

/// EVMTargetMachine
///
class EVMTargetMachine final : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  EVMSubtarget Subtarget;

public:
  EVMTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                   StringRef FS, const TargetOptions &Options,
                   Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                   CodeGenOpt::Level OL, bool JIT);

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  const EVMSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  // For EVM target this should return false to avoid assertion fails in some
  // post-RA passes that require NoVreg property.
  // For example, PrologEpilogInserter.
  bool usesPhysRegsForValues() const override { return false; }
}; // EVMTargetMachine.

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMTARGETMACHINE_H
