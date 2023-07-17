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

namespace SyncVM {

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
inline bool hasInsSwapped(unsigned Opcode) {
  int NewOpc = getWithInsNotSwapped(Opcode);
  return NewOpc != -1 && NewOpc != static_cast<int>(Opcode);
}
inline bool hasInsSwapped(const MachineInstr &MI) {
  return hasInsSwapped(MI.getOpcode());
}

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

enum class StackAccess { Invalid, Relative, Absolute };
inline StackAccess classifyStackAccess(MachineInstr::const_mop_iterator Op) {
  constexpr int NumStackOp = 3;
  if (std::distance(Op, Op->getParent()->operands_end()) < NumStackOp)
    return StackAccess::Invalid;

  if (Op->isReg() && Op->getReg() == SyncVM::SP)
    return StackAccess::Relative;
  return StackAccess::Absolute;
}

bool hasInvalidRelativeStackAccess(MachineInstr::const_mop_iterator Op);

} // namespace SyncVM

class SyncVMInstrInfo : public SyncVMGenInstrInfo {
  const SyncVMRegisterInfo RI;
  virtual void anchor();

public:
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

  static bool isFlagSettingInstruction(uint16_t opcode) {
    // a reverse lookup: for an `_v` instruction, it should return values
    // other than -1
    return SyncVM::getNonFlagSettingOpcode(opcode) != -1;
  }

  bool isPredicatedInstr(const MachineInstr &MI) const;
  SyncVMCC::CondCodes getCCCode(const MachineInstr &MI) const;

  unsigned defaultOutlineReruns() const override { return 5; }

  bool shouldOutlineFromFunctionByDefault(MachineFunction &MF) const override;

  void fixupStackPostOutline(MachineFunction &MF) const;

  /// Return true if the function can safely be outlined from.
  bool isFunctionSafeToOutlineFrom(MachineFunction &MF,
                                   bool OutlineFromLinkOnceODRs) const override;

  /// Return true if MBB is safe to outline from, and return any target-specific
  /// information in Flags.
  bool isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                              unsigned &Flags) const override;

  /// Return if/how a given MachineInstr should be outlined.
  outliner::InstrType getOutliningType(MachineBasicBlock::iterator &MBBI,
                                       unsigned Flags) const override;

  /// Calculate target-specific information for a set of outlining candidates.
  outliner::OutlinedFunction getOutliningCandidateInfo(
      std::vector<outliner::Candidate> &RepeatedSequenceLocs) const override;

  /// Insert a custom frame for outlined functions.
  /// Since for outlined functions we don't need to follow standard calling
  /// convention ABI, in case we are returning from the outlined function we are
  /// using jump that loads return address from TOS that was added just before
  /// the call. Usually for this, we would use ret nstruction, but we would have
  /// some limitations w.r.t. propagating flags. In case we are not returning
  /// from a function, we are not adding any instruction.
  void buildOutlinedFrame(MachineBasicBlock &MBB, MachineFunction &MF,
                          const outliner::OutlinedFunction &OF) const override;

  /// Insert a call to an outlined function into a given basic block.
  /// Since for outlined functions we don't need to follow standard calling
  /// convention ABI, in case we are returning from the outlined function we are
  /// using 2 instructions to generate call to it: first to save return address
  /// onto TOS and second to jump to the outlined function. Usually for this, we
  /// would use near_call instruction, but we would have some limitations w.r.t.
  /// propagating flags. In other case where we are not returning from a
  /// function, we just use jump to it.
  MachineBasicBlock::iterator
  insertOutlinedCall(Module &M, MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator &It, MachineFunction &MF,
                     outliner::Candidate &C) const override;

  /// Do a fixup post outline.
  /// We are doing fixup in three phasses:
  ///   1. Adjust outlined functions and callers that have to put return address
  ///      onto the stack.
  ///   2. Remove functions that we already adjusted or we don't need to, so we
  ///      can simplify 3rd phase.
  ///   3. Adjust outlined functions that don't return and their callers, and
  ///      repeat it as long as we are changing something.
  ///
  /// This is done in order to detect cases where we have to adjust outlined
  /// functions that don't return, so we can align relative stack addresses in
  /// all their callers to preserve correctness. Also, doing like this, we could
  /// end up saving 1 instruction in a frameless function if we don't need to
  /// adjust it.
  void fixupPostOutline(
      std::vector<std::pair<MachineFunction *, std::vector<MachineFunction *>>>
          &FixupFunctions) const override;
};

} // namespace llvm

#endif
