//===---- EVMTargetMachine.h - Define TargetMachine for EVM -*- C++ -*-----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the EVM specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

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
                   std::optional<Reloc::Model> RM,
                   std::optional<CodeModel::Model> CM, CodeGenOpt::Level OL,
                   bool JIT);

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  const EVMSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  yaml::MachineFunctionInfo *createDefaultFuncInfoYAML() const override;
  yaml::MachineFunctionInfo *
  convertFuncInfoToYAML(const MachineFunction &MF) const override;
  bool parseMachineFunctionInfo(const yaml::MachineFunctionInfo &,
                                PerFunctionMIParsingState &PFS,
                                SMDiagnostic &Error,
                                SMRange &SourceRange) const override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  // For EVM target this should return false to avoid assertion fails in some
  // post-RA passes that require NoVreg property.
  // For example, PrologEpilogInserter.
  bool usesPhysRegsForValues() const override { return false; }

  void registerPassBuilderCallbacks(PassBuilder &PB) override;
}; // EVMTargetMachine.

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMTARGETMACHINE_H
