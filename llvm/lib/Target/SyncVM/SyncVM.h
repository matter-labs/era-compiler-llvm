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

namespace llvm {
class SyncVMTargetMachine;

FunctionPass *createSyncVMISelDag(SyncVMTargetMachine &TM,
                                  CodeGenOpt::Level OptLevel);
ModulePass   *createSyncVMExpandUMAPass();
ModulePass   *createSyncVMIndirectUMAPass();
ModulePass   *createSyncVMIndirectExternalCallPass();
ModulePass   *createSyncVMLowerIntrinsicsPass();
ModulePass   *createSyncVMLinkRuntimePass();
FunctionPass *createSyncVMAdjustStackInCallseqPass();
FunctionPass *createSyncVMAdjustSPBasedOffsetsPass();
FunctionPass *createSyncVMAllocaHoistingPass();
FunctionPass *createSyncVMCodegenPreparePass();
FunctionPass *createSyncVMExpandPseudoPass();
FunctionPass *createSyncVMDropUnusedRegistersPass();
FunctionPass *createSyncVMMoveCallResultSpillPass();
FunctionPass *createSyncVMPeepholePass();

void initializeSyncVMExpandUMAPass(PassRegistry &);
void initializeSyncVMIndirectUMAPass(PassRegistry &);
void initializeSyncVMIndirectExternalCallPass(PassRegistry &);
void initializeSyncVMLowerIntrinsicsPass(PassRegistry &);
void initializeSyncVMLinkRuntimePass(PassRegistry &);
void initializeSyncVMCodegenPreparePass(PassRegistry &);
void initializeSyncVMExpandPseudoPass(PassRegistry &);
void initializeSyncVMAllocaHoistingPass(PassRegistry &);
void initializeSyncVMDropUnusedRegistersPass(PassRegistry &);
void initializeSyncVMAdjustSPBasedOffsetsPass(PassRegistry &);
void initializeSyncVMMoveCallResultSpillPass(PassRegistry &);
void initializeSyncVMAdjustStackInCallseqPass(PassRegistry &);
void initializeSyncVMPeepholePass(PassRegistry &);

} // end namespace llvm;

#endif
