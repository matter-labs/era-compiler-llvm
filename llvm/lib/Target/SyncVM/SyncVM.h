//==-- SyncVM.h - Top-level interface for SyncVM representation --*- C++ -*-==//
//
// This file contains the entry points for global functions defined in
// the LLVM SyncVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVM_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVM_H

#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace SyncVMCC {
// MSP430 specific condition code.
enum CondCodes {
  COND_NONE = 0, // unconditional
  COND_E = 1,    // aka COND_Z
  COND_LT = 2,
  COND_LE = 3,
  COND_GT = 4,
  COND_GE = 5,
  COND_NE = 6, // aka COND_NZ

  COND_INVALID = -1
};
} // namespace SyncVMCC

namespace llvm {
class SyncVMTargetMachine;

FunctionPass *createSyncVMISelDag(SyncVMTargetMachine &TM,
                                  CodeGenOpt::Level OptLevel);
ModulePass *createSyncVMLowerIntrinsicsPass();
FunctionPass *createSyncVMCodegenPreparePass();
FunctionPass *createSyncVMExpandPseudoPass();
FunctionPass *createSyncVMAllocaHoistingPass();
FunctionPass *createSyncVMDropUnusedRegistersPass();
FunctionPass *createSyncVMAdjustSPBasedOffsetsPass();
FunctionPass *createSyncVMMoveCallResultSpillPass();
FunctionPass *createSyncVMAdjustStackInCallseqPass();

void initializeSyncVMLowerIntrinsicsPass(PassRegistry &);
void initializeSyncVMCodegenPreparePass(PassRegistry &);
void initializeSyncVMExpandPseudoPass(PassRegistry &);
void initializeSyncVMAllocaHoistingPass(PassRegistry &);
void initializeSyncVMDropUnusedRegistersPass(PassRegistry &);
void initializeSyncVMAdjustSPBasedOffsetsPass(PassRegistry &);
void initializeSyncVMMoveCallResultSpillPass(PassRegistry &);
void initializeSyncVMAdjustStackInCallseqPass(PassRegistry &);

} // end namespace llvm;

#endif
