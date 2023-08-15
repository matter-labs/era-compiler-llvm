//===-- SyncVMTargetMachine.cpp - Define TargetMachine for SyncVM ---------===//
//
// Top-level implementation for the SyncVM target.
//
//===----------------------------------------------------------------------===//

#include "SyncVMTargetMachine.h"

#include "TargetInfo/SyncVMTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/MergeSimilarBB.h"
#include "llvm/Transforms/Utils.h"

#include "SyncVM.h"
#include "SyncVMTargetTransformInfo.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMTarget() {
  // Register the target.
  RegisterTargetMachine<SyncVMTargetMachine> X(getTheSyncVMTarget());
  // TODO: optimize switch lowering
  auto &PR = *PassRegistry::getPassRegistry();
  initializeSyncVMCodegenPreparePass(PR);
  initializeSyncVMCombineFlagSettingPass(PR);
  initializeSyncVMCombineReloadUsePass(PR);
  initializeSyncVMExpandPseudoPass(PR);
  initializeSyncVMExpandSelectPass(PR);
  initializeSyncVMFoldSelectPass(PR);
  initializeSyncVMLowerIntrinsicsPass(PR);
  initializeSyncVMLinkRuntimePass(PR);
  initializeSyncVMAllocaHoistingPass(PR);
  initializeSyncVMStackAddressConstantPropagationPass(PR);
}

static std::string computeDataLayout() {
  return "E-p:256:256-i256:256:256-S32-a:256:256";
}

static Reloc::Model getEffectiveRelocModel(Optional<Reloc::Model> RM) {
  if (!RM.hasValue())
    return Reloc::Static;
  return *RM;
}

SyncVMTargetMachine::SyncVMTargetMachine(const Target &T, const Triple &TT,
                                         StringRef CPU, StringRef FS,
                                         const TargetOptions &Options,
                                         Optional<Reloc::Model> RM,
                                         Optional<CodeModel::Model> CM,
                                         CodeGenOpt::Level OL, bool JIT)
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

SyncVMTargetMachine::~SyncVMTargetMachine() {}

TargetTransformInfo
SyncVMTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(SyncVMTTIImpl(this, F));
}

void SyncVMTargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerPipelineStartEPCallback(
      [](ModulePassManager &PM, OptimizationLevel Level) {
        PM.addPass(SyncVMLinkRuntimePass());
        PM.addPass(GlobalDCEPass());
      });

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &PM, OptimizationLevel Level) {
        if (Level.getSizeLevel() || Level.getSpeedupLevel() > 1)
          PM.addPass(MergeSimilarBB());
      });
}

namespace {
/// SyncVM Code Generator Pass Configuration Options.
class SyncVMPassConfig : public TargetPassConfig {
public:
  SyncVMPassConfig(SyncVMTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  SyncVMTargetMachine &getSyncVMTargetMachine() const {
    return getTM<SyncVMTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreRegAlloc() override;
  void addPreEmitPass() override;
  void addPreSched2() override;
};
} // namespace

TargetPassConfig *SyncVMTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new SyncVMPassConfig(*this, PM);
}

void SyncVMPassConfig::addIRPasses() {
  addPass(createSyncVMLowerIntrinsicsPass());
  addPass(createSyncVMLinkRuntimePass(true));
  addPass(createSyncVMCodegenPreparePass());
  addPass(createGlobalDCEPass());
  addPass(createSyncVMAllocaHoistingPass());
  if (TM->getOptLevel() != CodeGenOpt::None) {
    // Call SeparateConstOffsetFromGEP pass to extract constants within indices
    // and lower a GEP with multiple indices to either arithmetic operations or
    // multiple GEPs with single index.
    addPass(createSeparateConstOffsetFromGEPPass(true));
    // ReassociateGEPs exposes more opportunites for SLSR. See
    // the example in reassociate-geps-and-slsr.ll.
    addPass(createStraightLineStrengthReducePass());
    // SeparateConstOffsetFromGEP and SLSR creates common expressions which GVN
    // or EarlyCSE can reuse. GVN generates significantly better code than
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
  TargetPassConfig::addIRPasses();
}

bool SyncVMPassConfig::addInstSelector() {
  (void)TargetPassConfig::addInstSelector();
  // Install an instruction selector.
  addPass(createSyncVMISelDag(getSyncVMTargetMachine(), getOptLevel()));
  return false;
}

void SyncVMPassConfig::addPreRegAlloc() {
  addPass(createSyncVMAddConditionsPass());
  addPass(createSyncVMStackAddressConstantPropagationPass());
  addPass(createSyncVMBytesToCellsPass());
  if (TM->getOptLevel() != CodeGenOpt::None) {
    // The pass combines sub.s! 0, x, y with x definition. It assumes only one
    // usage of every definition of Flags. This is guaranteed by the selection
    // DAG. Every pass that break this assumption is expected to be sequenced
    // after SyncVMCombineFlagSetting.
    addPass(createSyncVMCombineFlagSettingPass());
    // This pass emits indexed loads and stores
    addPass(createSyncVMCombineToIndexedMemopsPass());
  }
}

void SyncVMPassConfig::addPreSched2() {
  if (getOptLevel() != CodeGenOpt::None) {
    addPass(createIfConverter([](const MachineFunction &MF) { return true; }));
  }
}

void SyncVMPassConfig::addPreEmitPass() {
  addPass(createSyncVMCombineReloadUsePass());
  addPass(createSyncVMExpandSelectPass());
  addPass(createSyncVMExpandPseudoPass());
}
