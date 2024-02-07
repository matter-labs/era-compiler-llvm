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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/DeadStoreElimination.h"
#include "llvm/Transforms/Utils.h"

#include "EraVM.h"
#include "EraVMAliasAnalysis.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMTargetTransformInfo.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMTarget() {
  // Register the target.
  RegisterTargetMachine<EraVMTargetMachine> X(getTheEraVMTarget());
  auto &PR = *PassRegistry::getPassRegistry();
  initializeEraVMCodegenPreparePass(PR);
  initializeEraVMCombineFlagSettingPass(PR);
  initializeEraVMCombineAddressingModePass(PR);
  initializeEraVMCSELegacyPassPass(PR);
  initializeEraVMExpandPseudoPass(PR);
  initializeEraVMExpandSelectPass(PR);
  initializeEraVMIndexedMemOpsPreparePass(PR);
  initializeEraVMLowerIntrinsicsPass(PR);
  initializeEraVMLinkRuntimePass(PR);
  initializeEraVMAllocaHoistingPass(PR);
  initializeEraVMStackAddressConstantPropagationPass(PR);
  initializeEraVMDAGToDAGISelLegacyPass(PR);
  initializeEraVMCombineToIndexedMemopsPass(PR);
  initializeEraVMOptimizeStdLibCallsPass(PR);
  initializeEraVMAAWrapperPassPass(PR);
  initializeEraVMExternalAAWrapperPass(PR);
  initializeEraVMAlwaysInlinePass(PR);
  initializeEraVMSHA3ConstFoldingPass(PR);
  initializeEraVMOptimizeSelectPostRAPass(PR);
  initializeEraVMTieSelectOperandsPass(PR);
  initializeEraVMHoistFlagSettingPass(PR);
  initializeEraVMOptimizeSelectPreRAPass(PR);
  initializeEraVMFoldSimilarInstructionsPass(PR);
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
  setMachineOutliner(true);
  setSupportsDefaultOutlining(true);
  initAsmInfo();
}

EraVMTargetMachine::~EraVMTargetMachine() = default;

TargetTransformInfo
EraVMTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(EraVMTTIImpl(this, F));
}

void EraVMTargetMachine::registerDefaultAliasAnalyses(AAManager &AAM) {
  AAM.registerFunctionAnalysis<EraVMAA>();
}

void EraVMTargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerPipelineEarlySimplificationEPCallback([](ModulePassManager &PM,
                                                      OptimizationLevel Level) {
    if (Level != OptimizationLevel::O0)
      PM.addPass(EraVMAlwaysInlinePass());

    PM.addPass(EraVMLinkRuntimePass(Level));
    if (Level != OptimizationLevel::O0) {
      PM.addPass(
          createModuleToFunctionPassAdaptor(EraVMOptimizeStdLibCallsPass()));
      PM.addPass(GlobalDCEPass());
      PM.addPass(
          createModuleToFunctionPassAdaptor(EraVMSHA3ConstFoldingPass()));
      PM.addPass(createModuleToFunctionPassAdaptor(DSEPass()));
    } else {
      PM.addPass(GlobalDCEPass());
    }
  });

  PB.registerPreInlinerOptimizationsEPCallback(
      [](ModulePassManager &PM, OptimizationLevel Level) {
        if (Level != OptimizationLevel::O0)
          PM.addPass(createModuleToFunctionPassAdaptor(EraVMCSEPass()));
      });

  PB.registerAnalysisRegistrationCallback([](FunctionAnalysisManager &FAM) {
    FAM.registerPass([] { return EraVMAA(); });
  });

  PB.registerPipelineParsingCallback(
      [](StringRef PassName, ModulePassManager &PM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (PassName == "eravm-always-inline") {
          PM.addPass(EraVMAlwaysInlinePass());
          return true;
        }
        return false;
      });

  PB.registerPipelineParsingCallback(
      [](StringRef PassName, FunctionPassManager &PM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (PassName == "eravm-sha3-constant-folding") {
          PM.addPass(EraVMSHA3ConstFoldingPass());
          return true;
        }
        if (PassName == "eravm-cse") {
          PM.addPass(EraVMCSEPass());
          return true;
        }
        return false;
      });

  PB.registerParseAACallback([](StringRef AAName, AAManager &AAM) {
    if (AAName == "eravm-aa") {
      AAM.registerFunctionAnalysis<EraVMAA>();
      return true;
    }
    return false;
  });
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
  void addPreSched2() override;
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
  addPass(createEraVMIndexedMemOpsPreparePass());
  addPass(createEraVMLinkRuntimePass(true));
  addPass(createEraVMCodegenPreparePass());
  addPass(createGlobalDCEPass());
  addPass(createEraVMAllocaHoistingPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    addPass(createEraVMAAWrapperPass());
    addPass(createExternalAAWrapperPass([](Pass &P, Function &,
                                           AAResults &AAR) {
      if (auto *WrapperPass = P.getAnalysisIfAvailable<EraVMAAWrapperPass>())
        AAR.addAAResult(WrapperPass->getResult());
    }));
  }
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
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    // The pass combines sub.s! 0, x, y with x definition. It assumes only one
    // usage of every definition of Flags. This is guaranteed by the selection
    // DAG. Every pass that break this assumption is expected to be sequenced
    // after EraVMCombineFlagSetting.
    addPass(createEraVMCombineFlagSettingPass());
    // This pass emits indexed loads and stores
    addPass(createEraVMCombineToIndexedMemopsPass());
    addPass(createEraVMFoldSimilarInstructionsPass());
    addPass(createEraVMOptimizeSelectPreRAPass());
    addPass(createEraVMHoistFlagSettingPass());
    addPass(&LiveVariablesID);
    addPass(createEraVMTieSelectOperandsPass());
  }
}

void EraVMPassConfig::addPreSched2() {
  if (getOptLevel() != CodeGenOptLevel::None) {
    addPass(createIfConverter([](const MachineFunction &MF) { return true; }));
  }
}

void EraVMPassConfig::addPreEmitPass() {
  addPass(createEraVMExpandPseudoPass());
  addPass(createEraVMCombineAddressingModePass());
  addPass(createEraVMExpandSelectPass());
  addPass(createEraVMOptimizeSelectPostRAPass());
}
