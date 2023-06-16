//===-- EraVMInstrInfo.h - EraVM Instruction Information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EraVM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMINSTRINFO_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMINSTRINFO_H

#include "EraVM.h"
#include "EraVMRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "EraVMGenInstrInfo.inc"

namespace llvm {

namespace EraVM {

int getPseudoMapOpcode(uint16_t);
int getFlagSettingOpcode(uint16_t);
int getNonFlagSettingOpcode(uint16_t);

/// Return opcode of the instruction with RR input addressing mode otherwise
/// identical to \p Opcode.
int getWithRRInAddrMode(uint16_t Opcode);
/// Return opcode of the instruction with IR input addressing mode otherwise
/// identical to \p Opcode. For a fat pointer instruction return an opcode with
/// operands swapped (i.e. XR).
int getWithIRInAddrMode(uint16_t Opcode);
/// Return opcode of the instruction with CR input addressing mode otherwise
/// identical to \p Opcode. For a fat pointer instruction return an opcode with
/// operands swapped (i.e. YR).
int getWithCRInAddrMode(uint16_t Opcode);
/// Return opcode of the instruction with SR input addressing mode otherwise
/// identical to \p Opcode.
int getWithSRInAddrMode(uint16_t Opcode);
/// Return opcode of the instruction with RR otput addressing mode otherwise
/// identical to \p Opcode.
int getWithRROutAddrMode(uint16_t Opcode);
/// Return opcode of the instruction with SR otput addressing mode otherwise
/// identical to \p Opcode.
int getWithSROutAddrMode(uint16_t Opcode);
/// Return opcode of the instruction with operands not swapped
/// otherwise identical to \p Opcode.
int getWithInsNotSwapped(uint16_t Opcode);
/// Return opcode of the instruction with operands swapped otherwise identical
/// to \p Opcode.
int getWithInsSwapped(uint16_t Opcode);

/// \defgroup AMProperties Addressing mode properties
/// Functions to identify in and out addressing mode of an instruction
/// @{
bool hasRRInAddressingMode(unsigned Opcode);
bool hasIRInAddressingMode(unsigned Opcode);
bool hasCRInAddressingMode(unsigned Opcode);
bool hasSRInAddressingMode(unsigned Opcode);
bool hasRROutAddressingMode(unsigned Opcode);
bool hasSROutAddressingMode(unsigned Opcode);
inline bool hasRRInAddressingMode(const MachineInstr &MI) {
  return hasRRInAddressingMode(MI.getOpcode());
}
inline bool hasIRInAddressingMode(const MachineInstr &MI) {
  return hasIRInAddressingMode(MI.getOpcode());
}
inline bool hasCRInAddressingMode(const MachineInstr &MI) {
  return hasCRInAddressingMode(MI.getOpcode());
}
inline bool hasSRInAddressingMode(const MachineInstr &MI) {
  return hasSRInAddressingMode(MI.getOpcode());
}
inline bool hasRROutAddressingMode(const MachineInstr &MI) {
  return hasRROutAddressingMode(MI.getOpcode());
}
inline bool hasSROutAddressingMode(const MachineInstr &MI) {
  return hasSROutAddressingMode(MI.getOpcode());
}
/// @}

bool isSelect(unsigned Opcode);
inline bool isSelect(const MachineInstr &MI) {
  return isSelect(MI.getOpcode());
}

enum class ArgumentKind { In0, In1, Out0, Out1 };

enum class ArgumentType { Register, Immediate, Code, Stack };

/// Return argument addressing mode for a specified argument.
ArgumentType argumentType(ArgumentKind Kind, unsigned Opcode);
inline ArgumentType argumentType(ArgumentKind Kind, const MachineInstr &MI) {
  return argumentType(Kind, MI.getOpcode());
}

/// Return number of Machine Operands that represent an ISA operand of \p Type.
inline unsigned argumentSize(ArgumentType Type) {
  switch (Type) {
  case ArgumentType::Register:
  case ArgumentType::Immediate:
    return 1;
  case ArgumentType::Code:
    return 2;
  case ArgumentType::Stack:
    return 3;
  }
  llvm_unreachable("Unexpected argument type");
}

/// Return number of Machine Operands that represent an ISA operand.
inline unsigned argumentSize(ArgumentKind Kind, const MachineInstr &MI) {
  return argumentSize(argumentType(Kind, MI));
}

/// \defgroup ISAOperandIt MOP iterators to instruction operands as per ISA.
/// Return machine operand iterator to the beginning of an ISA operand.
/// @{
MachineInstr::mop_iterator in0Iterator(MachineInstr &MI);
MachineInstr::mop_iterator in1Iterator(MachineInstr &MI);
MachineInstr::mop_iterator out0Iterator(MachineInstr &MI);
MachineInstr::mop_iterator out1Iterator(MachineInstr &MI);
/// @}

/// \defgroup ISAOperandRange MOP ranges to instruction operands as per ISA.
/// Return machine operand range of an ISA operand.
/// @{
inline auto in0Range(MachineInstr &MI) {
  auto It = in0Iterator(MI);
  return make_range(It, It + argumentSize(ArgumentKind::In0, MI));
}
inline auto in1Range(MachineInstr &MI) {
  auto It = in1Iterator(MI);
  return make_range(It, It + argumentSize(ArgumentKind::In1, MI));
}
inline auto out0Range(MachineInstr &MI) {
  auto It = out0Iterator(MI);
  return make_range(It, It + argumentSize(ArgumentKind::Out0, MI));
}
inline auto out1Range(MachineInstr &MI) {
  auto It = out1Iterator(MI);
  return make_range(It, It + argumentSize(ArgumentKind::Out1, MI));
}
/// @}

/// Copy \p OpRange to \p Inst.
inline void copyOperands(MachineInstrBuilder &Inst,
                         iterator_range<MachineInstr::mop_iterator> OpRange) {
  for (MachineOperand &MO : OpRange)
    Inst.add(MO);
}

/// Copy \p OpRange to \p Inst.
inline void copyOperands(MachineInstrBuilder &Inst,
                         MachineInstr::mop_iterator OpBegin,
                         MachineInstr::mop_iterator OpEnd) {
  for (MachineOperand &MO : make_range(OpBegin, OpEnd))
    Inst.add(MO);
}

} // namespace EraVM

class EraVMInstrInfo : public EraVMGenInstrInfo {
  const EraVMRegisterInfo RI;
  virtual void anchor();

public:
  explicit EraVMInstrInfo();

  /// getRegisterInfo - TargetInstrInfo is a superset of MRegister info.  As
  /// such, whenever a client has an instance of instruction info, it should
  /// always be able to get register info as well (through this method).
  ///
  const TargetRegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, MCRegister DestReg, MCRegister SrcReg,
                   bool KillSrc) const override;

  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI, Register SrcReg,
                           bool isKill, int FrameIndex,
                           const TargetRegisterClass *RC,
                           const TargetRegisterInfo *TRI,
                           Register VReg) const override;

  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI, Register DestReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI,
                            Register VReg) const override;

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  // Branch folding goodness
  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;

  int64_t getFramePoppedByCallee(const MachineInstr &I) const { return 0; }

  // Properties and mappings
  bool isAdd(const MachineInstr &MI) const;
  bool isSub(const MachineInstr &MI) const;
  bool isMul(const MachineInstr &MI) const;
  bool isDiv(const MachineInstr &MI) const;
  /// ptr.add, ptr.sub or ptr.pack
  bool isPtr(const MachineInstr &MI) const;
  /// A zero materializing add
  bool isNull(const MachineInstr &MI) const;

  // bitwise
  bool isAnd(const MachineInstr &MI) const;
  bool isOr(const MachineInstr &MI) const;
  bool isXor(const MachineInstr &MI) const;
  bool isShl(const MachineInstr &MI) const;
  bool isShr(const MachineInstr &MI) const;
  bool isRol(const MachineInstr &MI) const;
  bool isRor(const MachineInstr &MI) const;
  bool isSel(const MachineInstr &MI) const;

  bool isArithmetic(const MachineInstr &MI) const {
    return isAdd(MI) || isSub(MI) || isMul(MI) || isDiv(MI);
  }

  bool isBitwise(const MachineInstr &MI) const {
    return isAnd(MI) || isOr(MI) || isXor(MI) || isShl(MI) || isShr(MI) ||
           isRol(MI) || isRor(MI);
  }

  bool isShift(const MachineInstr &MI) const {
    return isShl(MI) || isShr(MI) || isRol(MI) || isRor(MI);
  }

  bool isRotate(const MachineInstr &MI) const { return isRol(MI) || isRor(MI); }

  bool isLoad(const MachineInstr &MI) const;
  bool isFatLoad(const MachineInstr &MI) const;
  bool isStore(const MachineInstr &MI) const;
  bool isNOP(const MachineInstr &MI) const;

  bool isSilent(const MachineInstr &MI) const;

  void tagFatPointerCopy(MachineInstr &) const override;

  bool isFlagSettingInstruction(uint16_t opcode) const {
    // a reverse lookup: for an `_v` instruction, it should return values
    // other than -1
    return EraVM::getNonFlagSettingOpcode(opcode) != -1;
  }

  bool isPredicatedInstr(const MachineInstr &MI) const;
  EraVMCC::CondCodes getCCCode(const MachineInstr &MI) const;
};

} // namespace llvm

#endif
