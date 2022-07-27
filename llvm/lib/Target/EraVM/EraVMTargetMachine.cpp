//===-- EraVMTargetMachine.cpp - Define TargetMachine for EraVM -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#include "EraVMTargetMachine.h"

#include "TargetInfo/EraVMTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils.h"

#include "EraVM.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMTargetTransformInfo.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMTarget() {
  // Register the target.
  RegisterTargetMachine<EraVMTargetMachine> X(getTheEraVMTarget());
  // TODO: optimize switch lowering
  auto &PR = *PassRegistry::getPassRegistry();
  initializeLowerSwitchLegacyPassPass(PR);
  initializeEraVMCodegenPreparePass(PR);
  initializeEraVMExpandPseudoPass(PR);
  initializeEraVMExpandSelectPass(PR);
  initializeEraVMLowerIntrinsicsPass(PR);
  initializeEraVMLinkRuntimePass(PR);
  initializeEraVMAllocaHoistingPass(PR);
  initializeEraVMMoveCallResultSpillPass(PR);
  initializeEraVMDAGToDAGISelLegacyPass(PR);
  initializeEraVMStackAddressConstantPropagationPass(PR);
}

static std::string computeDataLayout() {
  return "E-p:256:256-i256:256:256-S32-a:256:256";
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  if (!RM)
    return Reloc::Static;
  return *RM;
}

EraVMTargetMachine::EraVMTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(), TT, CPU, FS, Options,
                        getEffectiveRelocModel(RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, std::string(CPU), std::string(FS), *this) {
  setRequiresStructuredCFG(true);
  initAsmInfo();
}

EraVMTargetMachine::~EraVMTargetMachine() = default;

TargetTransformInfo
EraVMTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(EraVMTTIImpl(this, F));
}

namespace {
/// EraVM Code Generator Pass Configuration Options.
class EraVMPassConfig : public TargetPassConfig {
public:
  EraVMPassConfig(EraVMTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  EraVMTargetMachine &getEraVMTargetMachine() const {
    return getTM<EraVMTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreRegAlloc() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *EraVMTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new EraVMPassConfig(*this, PM);
}

MachineFunctionInfo *EraVMTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return EraVMMachineFunctionInfo::create<EraVMMachineFunctionInfo>(Allocator,
                                                                    F, STI);
}

void EraVMPassConfig::addIRPasses() {
  addPass(createEraVMLowerIntrinsicsPass());
  addPass(createEraVMLinkRuntimePass());
  addPass(createEraVMCodegenPreparePass());
  addPass(createGlobalDCEPass());
  addPass(createEraVMAllocaHoistingPass());
  TargetPassConfig::addIRPasses();
}

bool EraVMPassConfig::addInstSelector() {
  (void)TargetPassConfig::addInstSelector();
  // Install an instruction selector.
  addPass(createEraVMISelDag(getEraVMTargetMachine(), getOptLevel()));
  return false;
}

void EraVMPassConfig::addPreRegAlloc() {
  addPass(createEraVMAddConditionsPass());
  addPass(createEraVMStackAddressConstantPropagationPass());
  addPass(createEraVMBytesToCellsPass());
}

void EraVMPassConfig::addPreEmitPass() {
  addPass(createEraVMMoveCallResultSpillPass());
  addPass(createEraVMExpandSelectPass());
  addPass(createEraVMExpandPseudoPass());
}
