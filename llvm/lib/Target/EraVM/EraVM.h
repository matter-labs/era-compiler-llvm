//===-- EraVM.h - Top-level interface for EraVM representation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in
// the LLVM EraVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVM_H
#define LLVM_LIB_TARGET_ERAVM_ERAVM_H

#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Target/TargetMachine.h"

namespace EraVMCC {

// EraVM-specific condition codes - shared between hand-written C++ and
// tablegen-erated code. The values of COND_* constants are their binary
// encodings, except for COND_INVALID which is not encodable.
#define GET_CondCodes_DECL
#include "EraVMGenSearchableTables.inc"

} // namespace EraVMCC

namespace EraVMAS {

// EraVM address spaces
#define GET_AddressSpaces_DECL
#include "EraVMGenSearchableTables.inc"

} // namespace EraVMAS

/// This namespace holds all of the target specific flags that instruction info
/// tracks.
namespace EraVMII {
/// Target Operand Flags enum.
enum TargetOperandFlags {
  MO_NO_FLAG,

  /// Immediate that represents stack slot index.
  MO_STACK_SLOT_IDX,

  /// Represents return address symbol.
  MO_SYM_RET_ADDRESS,
}; // enum TargetOperandFlags
} // namespace EraVMII

inline unsigned getImmOrCImm(const llvm::MachineOperand &MO) {
  return MO.isImm() ? MO.getImm() : MO.getCImm()->getZExtValue();
}

namespace llvm {
class EraVMTargetMachine;
class FunctionPass;
class ModulePass;
class PassRegistry;

FunctionPass *createEraVMISelDag(EraVMTargetMachine &TM,
                                 CodeGenOpt::Level OptLevel);
ModulePass *createEraVMLowerIntrinsicsPass();
ModulePass *createEraVMLinkRuntimePass(bool);
FunctionPass *createEraVMAllocaHoistingPass();
FunctionPass *createEraVMBytesToCellsPass();
FunctionPass *createEraVMCodegenPreparePass();
FunctionPass *createEraVMExpandPseudoPass();
FunctionPass *createEraVMExpandSelectPass();
FunctionPass *createEraVMPropagateGenericPointersPass();
FunctionPass *createEraVMStackAddressConstantPropagationPass();
FunctionPass *createEraVMCombineFlagSettingPass();
FunctionPass *createEraVMCombineAddressingModePass();
FunctionPass *createEraVMCombineToIndexedMemopsPass();
FunctionPass *createEraVMOptimizeStdLibCallsPass();
ImmutablePass *createEraVMAAWrapperPass();
ImmutablePass *createEraVMExternalAAWrapperPass();
ModulePass *createEraVMAlwaysInlinePass();
FunctionPass *createEraVMSHA3ConstFoldingPass();
FunctionPass *createEraVMOptimizeSelectPostRAPass();
Pass *createEraVMIndexedMemOpsPreparePass();
FunctionPass *createEraVMTieSelectOperandsPass();
FunctionPass *createEraVMHoistFlagSettingPass();
FunctionPass *createEraVMOptimizeSelectPreRAPass();
FunctionPass *createEraVMCSEPass();
FunctionPass *createEraVMFoldSimilarInstructionsPass();
FunctionPass *createEraVMPostCodegenPreparePass();
FunctionPass *createEraVMDeadRegisterDefinitionsPass();
FunctionPass *createEraVMConditionOptimizerPass();

void initializeEraVMLowerIntrinsicsPass(PassRegistry &);
void initializeEraVMAllocaHoistingPass(PassRegistry &);
void initializeEraVMBytesToCellsPass(PassRegistry &);
void initializeEraVMLinkRuntimePass(PassRegistry &);
void initializeEraVMCodegenPreparePass(PassRegistry &);
void initializeEraVMExpandPseudoPass(PassRegistry &);
void initializeEraVMExpandSelectPass(PassRegistry &);
void initializeEraVMPropagateGenericPointersPass(PassRegistry &);
void initializeEraVMStackAddressConstantPropagationPass(PassRegistry &);
void initializeEraVMDAGToDAGISelPass(PassRegistry &);
void initializeEraVMCombineFlagSettingPass(PassRegistry &);
void initializeEraVMCombineAddressingModePass(PassRegistry &);
void initializeEraVMCombineToIndexedMemopsPass(PassRegistry &);
void initializeEraVMOptimizeStdLibCallsPass(PassRegistry &);
void initializeEraVMAAWrapperPassPass(PassRegistry &);
void initializeEraVMExternalAAWrapperPass(PassRegistry &);
void initializeEraVMAlwaysInlinePass(PassRegistry &);
void initializeEraVMSHA3ConstFoldingPass(PassRegistry &);
void initializeEraVMOptimizeSelectPostRAPass(PassRegistry &);
void initializeEraVMIndexedMemOpsPreparePass(PassRegistry &);
void initializeEraVMTieSelectOperandsPass(PassRegistry &);
void initializeEraVMHoistFlagSettingPass(PassRegistry &);
void initializeEraVMOptimizeSelectPreRAPass(PassRegistry &);
void initializeEraVMCSELegacyPassPass(PassRegistry &);
void initializeEraVMFoldSimilarInstructionsPass(PassRegistry &);
void initializeEraVMPostCodegenPreparePass(PassRegistry &);
void initializeEraVMDeadRegisterDefinitionsPass(PassRegistry &);
void initializeEraVMConditionOptimizerPass(PassRegistry &);

struct EraVMLinkRuntimePass : PassInfoMixin<EraVMLinkRuntimePass> {
  explicit EraVMLinkRuntimePass(OptimizationLevel Level) : Level(Level) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  OptimizationLevel Level;
};

struct EraVMOptimizeStdLibCallsPass
    : PassInfoMixin<EraVMOptimizeStdLibCallsPass> {
  EraVMOptimizeStdLibCallsPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

struct EraVMAlwaysInlinePass : PassInfoMixin<EraVMAlwaysInlinePass> {
  EraVMAlwaysInlinePass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

struct EraVMSHA3ConstFoldingPass : PassInfoMixin<EraVMSHA3ConstFoldingPass> {
  EraVMSHA3ConstFoldingPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

struct EraVMCSEPass : PassInfoMixin<EraVMCSEPass> {
  EraVMCSEPass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

struct EraVMLowerIntrinsicsPass : PassInfoMixin<EraVMLowerIntrinsicsPass> {
  EraVMLowerIntrinsicsPass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

struct EraVMPostCodegenPreparePass
    : PassInfoMixin<EraVMPostCodegenPreparePass> {
  EraVMPostCodegenPreparePass() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
