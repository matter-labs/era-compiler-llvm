//==-- EraVM.h - Top-level interface for EraVM representation ----*- C++ -*-==//
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
// EraVM specific condition code.
enum CondCodes {
  COND_NONE = 0, /// unconditional
  COND_E = 2,    /// EQ flag is set
  COND_LT = 4,   /// LT flag is set
  COND_GT = 8,   /// GT flag is set
  COND_NE,
  COND_LE,
  COND_GE,
  COND_OF, /// Overflow flag, cannot be reversed in optimization.
           /// Used in conjunction with arithmetic overflow check.
  COND_INVALID = -1
};
} // namespace EraVMCC

namespace EraVMAS {
// EraVM address spaces
enum AddressSpaces {
  AS_STACK = 0,
  AS_HEAP = 1,
  AS_HEAP_AUX = 2,
  AS_GENERIC = 3,
  AS_CODE = 4,
  AS_STORAGE = 5,
  MAX_ADDRESS = AS_STORAGE,
};
} // namespace EraVMAS

namespace EraVMCTX {
// EraVM context operands
enum Context {
  THIS = 0,
  CALLER = 1,
  CODE_SOURCE = 2,
  META = 3,
  TX_ORIGIN = 4,
  COINBASE = 5,
  GAS_LEFT = 6,
  SP = 7,
  GET_U128 = 8,
  SET_U128 = 9,
  INC_CTX = 10,
  SET_PUBDATAPRICE = 11,
};
} // namespace EraVMCTX

/// This namespace holds all of the target specific flags that instruction info
// tracks.
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
FunctionPass *createEraVMAddConditionsPass();
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
FunctionPass *createEraVMOptimizeSelectPass();
Pass *createEraVMIndexedMemOpsPreparePass();

void initializeEraVMLowerIntrinsicsPass(PassRegistry &);
void initializeEraVMAddConditionsPass(PassRegistry &);
void initializeEraVMAllocaHoistingPass(PassRegistry &);
void initializeEraVMBytesToCellsPass(PassRegistry &);
void initializeEraVMLinkRuntimePass(PassRegistry &);
void initializeEraVMCodegenPreparePass(PassRegistry &);
void initializeEraVMExpandPseudoPass(PassRegistry &);
void initializeEraVMExpandSelectPass(PassRegistry &);
void initializeEraVMPropagateGenericPointersPass(PassRegistry &);
void initializeEraVMStackAddressConstantPropagationPass(PassRegistry &);
void initializeEraVMCombineFlagSettingPass(PassRegistry &);
void initializeEraVMCombineAddressingModePass(PassRegistry &);
void initializeEraVMCombineToIndexedMemopsPass(PassRegistry &);
void initializeEraVMOptimizeStdLibCallsPass(PassRegistry &);
void initializeEraVMAAWrapperPassPass(PassRegistry &);
void initializeEraVMExternalAAWrapperPass(PassRegistry &);
void initializeEraVMAlwaysInlinePass(PassRegistry &);
void initializeEraVMSHA3ConstFoldingPass(PassRegistry &);
void initializeEraVMOptimizeSelectPass(PassRegistry &);
void initializeEraVMIndexedMemOpsPreparePass(PassRegistry &);

struct EraVMLinkRuntimePass : PassInfoMixin<EraVMLinkRuntimePass> {
  EraVMLinkRuntimePass(OptimizationLevel Level) : Level(Level) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  OptimizationLevel Level;
};

struct EraVMOptimizeStdLibCallsPass
    : PassInfoMixin<EraVMOptimizeStdLibCallsPass> {
  EraVMOptimizeStdLibCallsPass() {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

struct EraVMAlwaysInlinePass : PassInfoMixin<EraVMAlwaysInlinePass> {
  EraVMAlwaysInlinePass() {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

struct EraVMSHA3ConstFoldingPass : PassInfoMixin<EraVMSHA3ConstFoldingPass> {
  EraVMSHA3ConstFoldingPass() {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
