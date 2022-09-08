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
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils.h"

#include "SyncVM.h"
#include "SyncVMTargetTransformInfo.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMTarget() {
  // Register the target.
  RegisterTargetMachine<SyncVMTargetMachine> X(getTheSyncVMTarget());
  // TODO: optimize switch lowering
  auto &PR = *PassRegistry::getPassRegistry();
  initializeLowerSwitchLegacyPassPass(PR);
  initializeSyncVMExpandUMAPass(PR);
  initializeSyncVMIndirectUMAPass(PR);
  initializeSyncVMIndirectExternalCallPass(PR);
  initializeSyncVMCodegenPreparePass(PR);
  initializeSyncVMExpandPseudoPass(PR);
  initializeSyncVMExpandSelectPass(PR);
  initializeSyncVMLowerIntrinsicsPass(PR);
  initializeSyncVMLinkRuntimePass(PR);
  initializeSyncVMAllocaHoistingPass(PR);
  initializeSyncVMMoveCallResultSpillPass(PR);
  initializeSyncVMPeepholePass(PR);
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
  initAsmInfo();
}

SyncVMTargetMachine::~SyncVMTargetMachine() {}

TargetTransformInfo
SyncVMTargetMachine::getTargetTransformInfo(const Function &F) {
  return TargetTransformInfo(SyncVMTTIImpl(this, F));
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
};
} // namespace

TargetPassConfig *SyncVMTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new SyncVMPassConfig(*this, PM);
}

void SyncVMPassConfig::addIRPasses() {
  addPass(createSyncVMLowerIntrinsicsPass());
  addPass(createSyncVMLinkRuntimePass());
  addPass(createSyncVMCodegenPreparePass());
  addPass(createGlobalDCEPass());
  addPass(createSyncVMAllocaHoistingPass());
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
}

void SyncVMPassConfig::addPreEmitPass() {
  addPass(createSyncVMMoveCallResultSpillPass());
  addPass(createSyncVMExpandSelectPass());
  addPass(createSyncVMExpandPseudoPass());
  // TODO: The pass combines store with MULxxrr, DIVxxrr regardles of whether
  // the destination register has one use. Should be fixed and reenabled.
  // addPass(createSyncVMPeepholePass());
}
