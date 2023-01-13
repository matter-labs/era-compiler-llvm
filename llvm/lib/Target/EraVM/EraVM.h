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
  ERGS_LEFT = 6,
  SP = 7,
  GET_U128 = 8,
  SET_U128 = 9,
  INC_CTX = 10,
  SET_PUBDATAPRICE = 11,
};
} // namespace EraVMCTX

inline unsigned getImmOrCImm(const llvm::MachineOperand &MO) {
  return MO.isImm() ? MO.getImm() : MO.getCImm()->getZExtValue();
}

namespace llvm {
class EraVMTargetMachine;
class FunctionPass;
class ModulePass;
class PassRegistry;

FunctionPass *createEraVMISelDag(EraVMTargetMachine &TM,
                                 CodeGenOptLevel OptLevel);
ModulePass *createEraVMLowerIntrinsicsPass();
ModulePass *createEraVMLinkRuntimePass();
FunctionPass *createEraVMAddConditionsPass();
FunctionPass *createEraVMAllocaHoistingPass();
FunctionPass *createEraVMBytesToCellsPass();
FunctionPass *createEraVMCodegenPreparePass();
FunctionPass *createEraVMExpandPseudoPass();
FunctionPass *createEraVMExpandSelectPass();
FunctionPass *createEraVMPropagateGenericPointersPass();
FunctionPass *createEraVMStackAddressConstantPropagationPass();
FunctionPass *createEraVMCombineFlagSettingPass();

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
void initializeEraVMDAGToDAGISelLegacyPass(PassRegistry &);
void initializeEraVMCombineFlagSettingPass(PassRegistry &);

} // namespace llvm

#endif
