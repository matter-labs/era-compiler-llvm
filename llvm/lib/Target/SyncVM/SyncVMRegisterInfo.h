//===-- SyncVMRegisterInfo.h - SyncVM Register Information Impl -*- C++ -*-===//
//
// This file contains the SyncVM implementation of the MRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMREGISTERINFO_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "SyncVMGenRegisterInfo.inc"

namespace llvm {

struct SyncVMRegisterInfo : public SyncVMGenRegisterInfo {
public:
  SyncVMRegisterInfo();

  /// Code Generation virtual methods...
  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;
  const TargetRegisterClass*
  getPointerRegClass(const MachineFunction &MF,
                     unsigned Kind = 0) const override;
};

} // end namespace llvm

#endif
