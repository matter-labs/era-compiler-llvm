//===-- EraVMTargetMachine.h - Define TargetMachine for EraVM ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the EraVM specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMTARGETMACHINE_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMTARGETMACHINE_H

#include "EraVMSubtarget.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

/// EraVMTargetMachine
///
class EraVMTargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  EraVMSubtarget Subtarget;

public:
  EraVMTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                     StringRef FS, const TargetOptions &Options,
                     std::optional<Reloc::Model> RM,
                     std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                     bool JIT);
  EraVMTargetMachine(const EraVMTargetMachine &) = delete;
  EraVMTargetMachine &operator=(EraVMTargetMachine &) = delete;
  EraVMTargetMachine(EraVMTargetMachine &&) = delete;
  EraVMTargetMachine &&operator=(EraVMTargetMachine &&) = delete;
  ~EraVMTargetMachine() override;

  const EraVMSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;
  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;
  void registerPassBuilderCallbacks(PassBuilder &PB) override;
  void registerDefaultAliasAnalyses(AAManager &AAM) override;

}; // EraVMTargetMachine.

} // end namespace llvm

#endif
