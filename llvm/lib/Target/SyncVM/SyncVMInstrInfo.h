//===-- SyncVMInstrInfo.h - SyncVM Instruction Information ------*- C++ -*-===//
//
// This file contains the SyncVM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMINSTRINFO_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMINSTRINFO_H

#include "SyncVM.h"
#include "SyncVMRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "SyncVMGenInstrInfo.inc"

namespace llvm {

// instruction mapping declarations
namespace SyncVM {
int getPseudoMapOpcode(uint16_t Opcode);
int getFlagSettingOpcode(uint16_t Opcode);
int getNonFlagSettingOpcode(uint16_t Opcode);
int getStackSettingOpcode(uint16_t Opcode);
int getSROperandAddressingModeOpcode(uint16_t Opcode);
int getReversedOperandOpcode(uint16_t Opcode);
} // namespace SyncVM

class SyncVMInstrInfo : public SyncVMGenInstrInfo {
  const SyncVMRegisterInfo RI;
  virtual void anchor();

public:
  enum GenericInstruction {
    Unsupported = 0,
    ADD,
    SUB,
    MUL,
    DIV,
  };

  explicit SyncVMInstrInfo();

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
                           const TargetRegisterInfo *TRI) const override;
  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI, Register DestReg,
                            int FrameIdx, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI) const override;

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
  bool isFarCall(const MachineInstr &MI) const;
  bool hasRROperandAddressingMode(const MachineInstr &MI) const;
  bool hasRIOperandAddressingMode(const MachineInstr &MI) const;
  bool hasRXOperandAddressingMode(const MachineInstr &MI) const;
  bool hasRSOperandAddressingMode(const MachineInstr &MI) const;
  bool hasTwoOuts(const MachineInstr &MI) const;
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

  bool isRotate(const MachineInstr &MI) const {
    return isRol(MI) || isRor(MI);
  }

  bool isLoad(const MachineInstr &MI) const;
  bool isFatLoad(const MachineInstr &MI) const;
  bool isStore(const MachineInstr &MI) const;
  bool isNOP(const MachineInstr &MI) const;

  bool isSilent(const MachineInstr &MI) const;
  GenericInstruction genericInstructionFor(const MachineInstr &MI) const;

  void tagFatPointerCopy(MachineInstr &) const override;

  bool isFlagSettingInstruction(uint16_t opcode) const {
    // a reverse lookup: for an `_v` instruction, it should return values
    // other than -1
    return SyncVM::getNonFlagSettingOpcode(opcode) != -1;
  }

  bool isPredicatedInstr(const MachineInstr &MI) const;
  SyncVMCC::CondCodes getCCCode(const MachineInstr &MI) const;
  bool isUnconditionalNonTerminator(const MachineInstr &MI) const;
  static bool useRegIsFirstSourceOperand(const MachineInstr&MI, Register reg);
};

} // namespace llvm

#endif
