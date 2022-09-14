//==-- SyncVM.h - Top-level interface for SyncVM representation --*- C++ -*-==//
//
// This file contains the entry points for global functions defined in
// the LLVM SyncVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVM_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVM_H

#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/IR/Constants.h"
#include "llvm/Target/TargetMachine.h"

namespace SyncVMCC {
// SyncVM specific condition code.
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
} // namespace SyncVMCC

namespace SyncVMAS {
// SyncVM address spaces
enum AddressSpaces {
  AS_STACK = 0,
  AS_HEAP = 1,
  AS_HEAP_AUX = 2,
  AS_GENERIC = 3,
  AS_CODE = 4,
};
} // namespace SyncVMAS

namespace SyncVMCTX {
// SyncVM context operands
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
} // namespace SyncVMCTX

inline unsigned getImmOrCImm(const llvm::MachineOperand &MO) {
  return MO.isImm() ? MO.getImm() : MO.getCImm()->getZExtValue();
}

namespace llvm {
class SyncVMTargetMachine;

FunctionPass *createSyncVMISelDag(SyncVMTargetMachine &TM,
                                  CodeGenOpt::Level OptLevel);
ModulePass   *createSyncVMExpandUMAPass();
ModulePass   *createSyncVMIndirectUMAPass();
ModulePass   *createSyncVMIndirectExternalCallPass();
ModulePass   *createSyncVMLowerIntrinsicsPass();
ModulePass   *createSyncVMLinkRuntimePass();
FunctionPass *createSyncVMAddConditionsPass();
FunctionPass *createSyncVMAllocaHoistingPass();
FunctionPass *createSyncVMBytesToCellsPass();
FunctionPass *createSyncVMCodegenPreparePass();
FunctionPass *createSyncVMExpandPseudoPass();
FunctionPass *createSyncVMExpandSelectPass();
FunctionPass *createSyncVMPeepholePass();
FunctionPass *createSyncVMPropagateGenericPointersPass();
FunctionPass *createSyncVMStackAddressConstantPropagationPass();

void initializeSyncVMExpandUMAPass(PassRegistry &);
void initializeSyncVMIndirectUMAPass(PassRegistry &);
void initializeSyncVMIndirectExternalCallPass(PassRegistry &);
void initializeSyncVMLowerIntrinsicsPass(PassRegistry &);
void initializeSyncVMAddConditionsPass(PassRegistry &);
void initializeSyncVMAllocaHoistingPass(PassRegistry &);
void initializeSyncVMBytesToCellsPass(PassRegistry &);
void initializeSyncVMLinkRuntimePass(PassRegistry &);
void initializeSyncVMCodegenPreparePass(PassRegistry &);
void initializeSyncVMExpandPseudoPass(PassRegistry &);
void initializeSyncVMExpandSelectPass(PassRegistry &);
void initializeSyncVMPeepholePass(PassRegistry &);
void initializeSyncVMPropagateGenericPointersPass(PassRegistry &);
void initializeSyncVMStackAddressConstantPropagationPass(PassRegistry &);

} // end namespace llvm;

#endif
