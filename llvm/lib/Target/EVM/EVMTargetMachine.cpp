//===------ EVMTargetMachine.cpp - Define TargetMachine for EVM -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Top-level implementation for the EVM target.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"

#include "EVMMachineFunctionInfo.h"
#include "EVMTargetMachine.h"
#include "EVMTargetObjectFile.h"
#include "EVMTargetTransformInfo.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Utils.h"

using namespace llvm;

// Disable stackification pass and keep a register form
// form of instructions. Is only used for debug purposes
// when assembly printing.
cl::opt<bool>
    EVMKeepRegisters("evm-keep-registers", cl::Hidden,
                      cl::desc("EVM: output stack registers in"
                               " instruction output for test purposes only."),
                      cl::init(false));

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMTarget() {
  // Register the target.
  const RegisterTargetMachine<EVMTargetMachine> X(getTheEVMTarget());
  auto &PR = *PassRegistry::getPassRegistry();
  initializeEVMCodegenPreparePass(PR);
  initializeEVMAllocaHoistingPass(PR);
  initializeEVMLinkRuntimePass(PR);
  initializeEVMLowerIntrinsicsPass(PR);
  initializeEVMOptimizeLiveIntervalsPass(PR);
  initializeEVMRegColoringPass(PR);
  initializeEVMSingleUseExpressionPass(PR);
  initializeEVMStackifyPass(PR);
}

static std::string computeDataLayout() {
  return "E-p:256:256-i256:256:256-S256-a:256:256";
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  if (!RM)
    return Reloc::Static;
  return *RM;
}

EVMTargetMachine::EVMTargetMachine(const Target &T, const Triple &TT,
                                   StringRef CPU, StringRef FS,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM,
                                   std::optional<CodeModel::Model> CM,
                                   CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(), TT, CPU, FS, Options,
                        getEffectiveRelocModel(RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<EVMELFTargetObjectFile>()),
      Subtarget(TT, std::string(CPU), std::string(FS), *this) {
  setRequiresStructuredCFG(true);
  initAsmInfo();
}

TargetTransformInfo
EVMTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(EVMTTIImpl(this, F));
}

MachineFunctionInfo *EVMTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return EVMFunctionInfo::create<EVMFunctionInfo>(Allocator, F, STI);
}

void EVMTargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerPipelineStartEPCallback(
      [](ModulePassManager &PM, OptimizationLevel Level) {
        PM.addPass(EVMLinkRuntimePass());
        PM.addPass(GlobalDCEPass());
        PM.addPass(createModuleToFunctionPassAdaptor(EVMAllocaHoistingPass()));
      });
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

  void addCodeGenPrepare() override;
  void addIRPasses() override;
  bool addGCPasses() override { return false; }
  bool addInstSelector() override;
  void addPostRegAlloc() override;
  void addPreEmitPass() override;
};
} // namespace

void EVMPassConfig::addIRPasses() {
  addPass(createEVMLowerIntrinsicsPass());
  TargetPassConfig::addIRPasses();
}

void EVMPassConfig::addCodeGenPrepare() {
  addPass(createEVMCodegenPreparePass());
  TargetPassConfig::addCodeGenPrepare();
}

bool EVMPassConfig::addInstSelector() {
  (void)TargetPassConfig::addInstSelector();
  // Install an instruction selector.
  addPass(createEVMISelDag(getEVMTargetMachine(), getOptLevel()));
  // Run the argument-move pass immediately after the ScheduleDAG scheduler
  // so that we can fix up the ARGUMENT instructions before anything else
  // sees them in the wrong place.
  addPass(createEVMArgumentMove());
  return false;
}

void EVMPassConfig::addPostRegAlloc() {
  // TODO: The following CodeGen passes don't currently support code containing
  // virtual registers. Consider removing their restrictions and re-enabling
  // them. These functions all require the NoVRegs property.
  disablePass(&MachineLateInstrsCleanupID);
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

void EVMPassConfig::addPreEmitPass() {
  TargetPassConfig::addPreEmitPass();

  // FIXME: enable all the passes below, but the Stackify with EVMKeepRegisters.
  if (!EVMKeepRegisters) {
    addPass(createEVMOptimizeLiveIntervals());
    addPass(createEVMSingleUseExpression());
    // Run the register coloring pass to reduce the total number of registers.
    addPass(createEVMRegColoring());
    addPass(createEVMStackify());
  }
}

TargetPassConfig *EVMTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new EVMPassConfig(*this, PM);
}
