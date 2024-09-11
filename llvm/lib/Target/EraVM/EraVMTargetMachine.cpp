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
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/DeadStoreElimination.h"
#include "llvm/Transforms/Scalar/MergeIdenticalBB.h"
#include "llvm/Transforms/Utils.h"

#include "EraVM.h"
#include "EraVMAliasAnalysis.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMTargetTransformInfo.h"

using namespace llvm;

// FIXME: Improve codegen tests so that the passes doesn't optimizated
// the interesting code away making the tests useless.
static cl::opt<bool> DisableStraightLineScalarOptPasses(
    "disable-eravm-scalar-opt-passes",
    cl::desc("Disable the sequence of scalar pre-ISel passes (for testing "
             "purpose only)"),
    cl::init(false), cl::Hidden);

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
  initializeEraVMDAGToDAGISelPass(PR);
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
  initializeEraVMCombineToIndexedMemopsPass(PR);
  initializeEraVMPostCodegenPreparePass(PR);
  initializeEraVMDeadRegisterDefinitionsPass(PR);
  initializeEraVMConditionOptimizerPass(PR);
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
                                       CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(), TT, CPU, FS, Options,
                        getEffectiveRelocModel(RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, std::string(CPU), std::string(FS), *this) {
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

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &PM, OptimizationLevel Level) {
        if (Level.getSizeLevel() || Level.getSpeedupLevel() > 1)
          PM.addPass(MergeIdenticalBBPass());
        if (Level != OptimizationLevel::O0)
          PM.addPass(EraVMCSEPass());
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
        if (PassName == "eravm-lower-intrinsics") {
          PM.addPass(EraVMLowerIntrinsicsPass());
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
        if (PassName == "eravm-post-codegen-prepare") {
          PM.addPass(EraVMPostCodegenPreparePass());
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
      : TargetPassConfig(TM, PM) {
    // Exceptions and StackMaps are not supported,
    // so these passes will never do anything.
    disablePass(&StackMapLivenessID);
    disablePass(&FuncletLayoutID);
    disablePass(&PatchableFunctionID);
    // Garbage collection is not supported.
    disablePass(&GCLoweringID);
    disablePass(&ShadowStackGCLoweringID);
  }

  EraVMTargetMachine &getEraVMTargetMachine() const {
    return getTM<EraVMTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  bool addILPOpts() override;
  void addPreRegAlloc() override;
  void addPreEmitPass() override;
  void addPreSched2() override;
  void addCodeGenPrepare() override;
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
  if (TM->getOptLevel() != CodeGenOpt::None) {
    if (!DisableStraightLineScalarOptPasses) {
      // Call SeparateConstOffsetFromGEP pass to extract constants within
      // indices and lower a GEP with multiple indices to either arithmetic
      // operations or multiple GEPs with single index.
      addPass(createSeparateConstOffsetFromGEPPass(true));
      // ReassociateGEPs exposes more opportunites for SLSR. See
      // the example in reassociate-geps-and-slsr.ll.
      addPass(createStraightLineStrengthReducePass());
      // SeparateConstOffsetFromGEP and SLSR creates common expressions which
      // GVN or EarlyCSE can reuse. GVN generates significantly better code than
      // EarlyCSE for some of our benchmarks.
      addPass(createNewGVNPass());
      addPass(createGVNHoistPass());
      // Run NaryReassociate after EarlyCSE/GVN to be more effective.
      addPass(createNaryReassociatePass());
      // Call EarlyCSE pass to find and remove subexpressions in the lowered
      // result.
      addPass(createEarlyCSEPass());
      // Do loop invariant code motion in case part of the lowered result is
      // invariant.
      addPass(createLICMPass());
    }

    addPass(createEraVMAAWrapperPass());
    addPass(createExternalAAWrapperPass([](Pass &P, Function &,
                                           AAResults &AAR) {
      if (auto *WrapperPass = P.getAnalysisIfAvailable<EraVMAAWrapperPass>())
        AAR.addAAResult(WrapperPass->getResult());
    }));
  }
  TargetPassConfig::addIRPasses();
}

void EraVMPassConfig::addCodeGenPrepare() {
  TargetPassConfig::addCodeGenPrepare();
  if (getOptLevel() != CodeGenOpt::None)
    addPass(createEraVMPostCodegenPreparePass());
}

bool EraVMPassConfig::addInstSelector() {
  (void)TargetPassConfig::addInstSelector();
  // Install an instruction selector.
  addPass(createEraVMISelDag(getEraVMTargetMachine(), getOptLevel()));
  return false;
}

bool EraVMPassConfig::addILPOpts() {
  addPass(createEraVMConditionOptimizerPass());
  return true;
}

void EraVMPassConfig::addPreRegAlloc() {
  addPass(createEraVMStackAddressConstantPropagationPass());
  addPass(createEraVMBytesToCellsPass());
  if (TM->getOptLevel() != CodeGenOpt::None) {
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
    addPass(createEraVMDeadRegisterDefinitionsPass());
  }
}

void EraVMPassConfig::addPreSched2() {
  addPass(createEraVMExpandPseudoPass());
  if (getOptLevel() != CodeGenOpt::None) {
    addPass(createIfConverter([](const MachineFunction &MF) { return true; }));
  }
}

void EraVMPassConfig::addPreEmitPass() {
  addPass(createEraVMCombineAddressingModePass());
  addPass(createEraVMExpandSelectPass());
  addPass(createEraVMOptimizeSelectPostRAPass());
}
