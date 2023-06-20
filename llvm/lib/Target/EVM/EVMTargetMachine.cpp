//===-------- EVMTargetMachine.cpp - Define TargetMachine for EVM ---------===//
//
// Top-level implementation for the EVM target.
//
//===----------------------------------------------------------------------===//

#include "EVMTargetMachine.h"
#include "EVM.h"
#include "EVMTargetTransformInfo.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMTarget() {
  // Register the target.
  RegisterTargetMachine<EVMTargetMachine> X(getTheEVMTarget());
  auto &PR = *PassRegistry::getPassRegistry();
  initializeEVMLowerIntrinsicsPass(PR);
}

static std::string computeDataLayout() {
  return "E-p:256:256-i256:256:256-S256-a:256:256";
}

static Reloc::Model getEffectiveRelocModel(Optional<Reloc::Model> RM) {
  if (!RM.hasValue())
    return Reloc::Static;
  return *RM;
}

EVMTargetMachine::EVMTargetMachine(const Target &T, const Triple &TT,
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
  initAsmInfo();
}

TargetTransformInfo
EVMTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(EVMTTIImpl(this, F));
}

namespace {
/// EVM Code Generator Pass Configuration Options.
class EVMPassConfig final : public TargetPassConfig {
public:
  EVMPassConfig(EVMTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  EVMTargetMachine &getEVMTargetMachine() const {
    return getTM<EVMTargetMachine>();
  }

  // EVM target - is a virtual stack machine that requires stackification
  // of the instructions instead of allocating registers for their operands.
  FunctionPass *createTargetRegisterAllocator(bool) override { return nullptr; }

  // No reg alloc
  bool addRegAssignAndRewriteFast() override { return false; }

  // No reg alloc
  bool addRegAssignAndRewriteOptimized() override { return false; }

  void addIRPasses() override;
  bool addGCPasses() override { return false; }
  bool addInstSelector() override;
  void addPostRegAlloc() override;
};
} // namespace

void EVMPassConfig::addIRPasses() {
  addPass(createEVMLowerIntrinsicsPass());
  TargetPassConfig::addIRPasses();
}

bool EVMPassConfig::addInstSelector() {
  (void)TargetPassConfig::addInstSelector();
  // Install an instruction selector.
  addPass(createEVMISelDag(getEVMTargetMachine(), getOptLevel()));
  return false;
}

void EVMPassConfig::addPostRegAlloc() {
  // TODO: The following CodeGen passes don't currently support code containing
  // virtual registers. Consider removing their restrictions and re-enabling
  // them. These functions all require the NoVRegs property.
  disablePass(&MachineCopyPropagationID);
  disablePass(&PostRAMachineSinkingID);
  disablePass(&PostRASchedulerID);
  disablePass(&FuncletLayoutID);
  disablePass(&StackMapLivenessID);
  disablePass(&LiveDebugValuesID);
  disablePass(&PatchableFunctionID);
  disablePass(&ShrinkWrapID);

  // TODO: This pass is disabled in WebAssembly, as it hurts code size because
  // it can generate irreducible control flow. Check if this also true for EVM?
  disablePass(&MachineBlockPlacementID);

  TargetPassConfig::addPostRegAlloc();
}

TargetPassConfig *EVMTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new EVMPassConfig(*this, PM);
}
