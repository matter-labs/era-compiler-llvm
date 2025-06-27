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
  // TSF flag to check if this is a PUSH instruction.
  IsPushPos = 1,
  IsPushMask = 0x1,
};

} // namespace EVMII

class EVMInstrInfo final : public EVMGenInstrInfo {
  const EVMRegisterInfo RI;

public:
  explicit EVMInstrInfo();

  enum BranchType : uint8_t {
    BT_None,      // Couldn't analyze branch.
    BT_NoBranch,  // No branches found.
    BT_Uncond,    // One unconditional branch.
    BT_Cond,      // One conditional branch.
    BT_CondUncond // A conditional branch followed by an unconditional branch.
  };

  const EVMRegisterInfo &getRegisterInfo() const { return RI; }

  bool isReallyTriviallyReMaterializable(const MachineInstr &MI) const override;

  bool shouldBreakCriticalEdgeToSink(MachineInstr &MI) const override {
    return true;
  }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, MCRegister DestReg, MCRegister SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  BranchType analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                           MachineBasicBlock *&FBB,
                           SmallVectorImpl<MachineOperand> &Cond,
                           bool AllowModify,
                           SmallVectorImpl<MachineInstr *> &BranchInstrs) const;

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

  static bool isPush(const MachineInstr *MI) {
    return getTSFlag(MI, EVMII::IsPushPos, EVMII::IsPushMask);
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H
