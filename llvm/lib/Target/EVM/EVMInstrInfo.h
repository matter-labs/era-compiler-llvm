//=---------- EVMInstrInfo.h - EVM Instruction Information -*- C++ -*--------=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EVM implementation of the
// TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H
#define LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H

#include "EVMRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "EVMGenInstrInfo.inc"

namespace llvm {

namespace EVMII {

enum {
  // TSF flag to check if this is a stack instruction.
  IsStackPos = 0,
  IsStackMask = 0x1,
};

} // namespace EVMII

class EVMInstrInfo final : public EVMGenInstrInfo {
  const EVMRegisterInfo RI;

public:
  explicit EVMInstrInfo();

  const EVMRegisterInfo &getRegisterInfo() const { return RI; }

  bool isReallyTriviallyReMaterializable(const MachineInstr &MI) const override;

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, MCRegister DestReg, MCRegister SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL, int *BytesAdded) const override;

  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  /// TSFlags extraction
  static unsigned getTSFlag(const MachineInstr *MI, unsigned Pos,
                            unsigned Mask) {
    return (MI->getDesc().TSFlags >> Pos) & Mask;
  }

  static bool isStack(const MachineInstr *MI) {
    return getTSFlag(MI, EVMII::IsStackPos, EVMII::IsStackMask);
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H
