//===-- EraVMInstrInfo.cpp - EraVM Instruction Information ------*- C++ -*-===//
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

#include "EraVMInstrInfo.h"

#include <deque>
#include <optional>

#include "EraVMMachineFunctionInfo.h"
#include "EraVMTargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#define GET_INSTRMAP_INFO
#include "EraVMGenInstrInfo.inc"

namespace llvm {
namespace EraVM {

ArgumentType argumentType(ArgumentKind Kind, unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  // TODO: Mappings for Select.
  // Select is not a part of a mapping, so have to handle it manually.
  const DenseSet<unsigned> In0R = {EraVM::SELrrr, EraVM::SELrir, EraVM::SELrcr,
                                   EraVM::SELrsr, EraVM::FATPTR_SELrrr};
  const DenseSet<unsigned> In0I = {EraVM::SELirr, EraVM::SELiir, EraVM::SELicr,
                                   EraVM::SELisr};
  const DenseSet<unsigned> In0C = {EraVM::SELcrr, EraVM::SELcir, EraVM::SELccr,
                                   EraVM::SELcsr};
  const DenseSet<unsigned> In0S = {EraVM::SELsrr, EraVM::SELsir, EraVM::SELscr,
                                   EraVM::SELssr};
  const DenseSet<unsigned> In1R = {EraVM::SELrrr, EraVM::SELirr, EraVM::SELcrr,
                                   EraVM::SELsrr, EraVM::FATPTR_SELrrr};
  const DenseSet<unsigned> In1I = {EraVM::SELrir, EraVM::SELiir, EraVM::SELcir,
                                   EraVM::SELsir};
  const DenseSet<unsigned> In1C = {EraVM::SELrcr, EraVM::SELicr, EraVM::SELccr,
                                   EraVM::SELscr};
  const DenseSet<unsigned> In1S = {EraVM::SELrsr, EraVM::SELisr, EraVM::SELcsr,
                                   EraVM::SELssr};
  if (Kind == ArgumentKind::Out1) {
    // TODO: Support stack output for Select.
    return ArgumentType::Register;
  }
  if (Kind == ArgumentKind::In1) {
    if (In1R.count(Opcode))
      return ArgumentType::Register;
    if (In1I.count(Opcode))
      return ArgumentType::Immediate;
    if (In1C.count(Opcode))
      return ArgumentType::Code;
    if (In1S.count(Opcode))
      return ArgumentType::Stack;
    return ArgumentType::Register;
  }
  if (Kind == ArgumentKind::Out0) {
    if (hasSROutAddressingMode(Opcode))
      return ArgumentType::Stack;
    return ArgumentType::Register;
  }
  assert(Kind == ArgumentKind::In0);
  if (In0R.count(Opcode))
    return ArgumentType::Register;
  if (In0I.count(Opcode))
    return ArgumentType::Immediate;
  if (In0C.count(Opcode))
    return ArgumentType::Code;
  if (In0S.count(Opcode))
    return ArgumentType::Stack;
  if (hasRRInAddressingMode(Opcode))
    return ArgumentType::Register;
  if (hasIRInAddressingMode(Opcode))
    return ArgumentType::Immediate;
  if (hasCRInAddressingMode(Opcode))
    return ArgumentType::Code;
  return ArgumentType::Stack;
}

MachineInstr::mop_iterator in0Iterator(MachineInstr &MI) {
  return MI.operands_begin() + MI.getNumExplicitDefs();
}

MachineInstr::mop_iterator in1Iterator(MachineInstr &MI) {
  return in0Iterator(MI) + argumentSize(ArgumentKind::In0, MI);
}

MachineInstr::mop_iterator out0Iterator(MachineInstr &MI) {
  auto Begin = MI.operands_begin();
  if (hasRROutAddressingMode(MI) || isSelect(MI))
    return Begin;
  return in1Iterator(MI) + argumentSize(ArgumentKind::In1, MI);
}

MachineInstr::mop_iterator out1Iterator(MachineInstr &MI) {
  return MI.operands_begin() + MI.getNumExplicitDefs() - 1;
}

int getWithRRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_0); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_0); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_0); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_0);
}

int getWithIRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_1); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_1); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_1); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_1);
}

int getWithCRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_2); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_2); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_2); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_2);
}

int getWithSRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_3); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_3); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_3); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_3);
}

int getWithRROutAddrMode(uint16_t Opcode) {
  if (int Result = withRegisterResult(Opcode); Result != -1)
    return Result;
  return Opcode;
}

int getWithSROutAddrMode(uint16_t Opcode) {
  if (int Result = withStackResult(Opcode); Result != -1)
    return Result;
  return Opcode;
}

int getWithInsNotSwapped(uint16_t Opcode) {
  if (int Result = withInsNotSwapped(Opcode); Result != -1)
    return Result;
  return Opcode;
}

int getWithInsSwapped(uint16_t Opcode) {
  if (int Result = withInsSwapped(Opcode); Result != -1)
    return Result;
  return Opcode;
}

bool hasRRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapRRInputTo(Opcode, OperandAM_0) == Opcode;
}

bool hasIRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapIRInputTo(Opcode, OperandAM_1) == Opcode;
}

bool hasCRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapCRInputTo(Opcode, OperandAM_2) == Opcode;
}

bool hasSRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapSRInputTo(Opcode, OperandAM_3) == Opcode;
}

bool hasRROutAddressingMode(unsigned Opcode) {
  return withStackResult(Opcode) != -1;
}

bool hasSROutAddressingMode(unsigned Opcode) {
  return withRegisterResult(Opcode) != -1;
}

// TODO: Implement in via td.
bool isSelect(unsigned Opcode) {
  DenseSet<unsigned> Members = {
      EraVM::SELrrr,       EraVM::SELrir, EraVM::SELrcr, EraVM::SELrsr,
      EraVM::SELirr,       EraVM::SELiir, EraVM::SELicr, EraVM::SELisr,
      EraVM::SELcrr,       EraVM::SELcir, EraVM::SELccr, EraVM::SELcsr,
      EraVM::SELsrr,       EraVM::SELsir, EraVM::SELscr, EraVM::SELssr,
      EraVM::FATPTR_SELrrr};
  return Members.count(Opcode);
}

bool hasInvalidRelativeStackAccess(MachineInstr::const_mop_iterator Op) {
  auto SA = EraVM::classifyStackAccess(Op);
  if (SA == EraVM::StackAccess::Invalid)
    return true;

  if (SA == EraVM::StackAccess::Relative && !(Op + 2)->isImm())
    return true;
  return false;
}

} // namespace EraVM
} // namespace llvm

// Pin the vtable to this file.
void EraVMInstrInfo::anchor() {}

EraVMInstrInfo::EraVMInstrInfo()
    : EraVMGenInstrInfo(EraVM::ADJCALLSTACKDOWN, EraVM::ADJCALLSTACKUP), RI() {}

unsigned EraVMInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");

  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;

  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!I->isBranch())
      break;
    // Remove the branch.
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return Count;
}

bool EraVMInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Xbranch condition!");

  auto CC = static_cast<EraVMCC::CondCodes>(
      Cond[0].isImm() ? Cond[0].getImm() : Cond[0].getCImm()->getZExtValue());

  switch (CC) {
  default:
    llvm_unreachable("Invalid branch condition!");
  case EraVMCC::COND_E:
    CC = EraVMCC::COND_NE;
    break;
  case EraVMCC::COND_NE:
    CC = EraVMCC::COND_E;
    break;
  case EraVMCC::COND_LT:
    CC = EraVMCC::COND_GE;
    break;
  case EraVMCC::COND_GE:
    CC = EraVMCC::COND_LT;
    break;
  case EraVMCC::COND_LE:
    CC = EraVMCC::COND_GT;
    break;
  case EraVMCC::COND_GT:
    CC = EraVMCC::COND_LE;
    break;
  }

  Cond.pop_back();
  Cond.push_back(MachineOperand::CreateImm(CC));
  return false;
}

bool EraVMInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  // Start from the bottom of the block and work up, examining the
  // terminator instructions.
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;

    // Working from the bottom, when we see a non-terminator
    // instruction, we're done.
    if (!isUnpredicatedTerminator(*I))
      break;

    // A terminator that isn't a branch can't easily be handled
    // by this analysis.
    if (!I->isBranch())
      return true;

    // We can't analyze if target address is on stack.
    if (I->getOpcode() == EraVM::J_s)
      return true;

    if (I->getOpcode() == EraVM::J) {
      // Handle unconditional branches.
      auto *jumpTarget = I->getOperand(0).getMBB();
      if (!AllowModify) {
        TBB = jumpTarget;
        continue;
      }

      // If the block has any instructions after a JMP, delete them.
      MBB.erase(std::next(I), MBB.end());

      Cond.clear();
      FBB = nullptr;

      // Delete the JMP if it's equivalent to a fall-through.
      if (MBB.isLayoutSuccessor(jumpTarget)) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }

      // TBB is used to indicate the unconditional destination.
      TBB = jumpTarget;
      continue;
    }

    auto BranchCode =
        static_cast<EraVMCC::CondCodes>(getImmOrCImm(I->getOperand(1)));
    if (BranchCode == EraVMCC::COND_INVALID)
      return true; // Can't handle weird stuff.

    // Working from the bottom, handle the first conditional branch.
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      LLVMContext &C = MBB.getParent()->getFunction().getContext();
      auto *CImmCC =
          ConstantInt::get(IntegerType::get(C, 256), BranchCode, false);
      Cond.push_back(MachineOperand::CreateCImm(CImmCC));
      continue;
    }

    // Handle subsequent conditional branches. Only handle the case where all
    // conditional branches branch to the same destination.
    assert(Cond.size() == 1);
    assert(TBB);

    // Only handle the case where all conditional branches branch to
    // the same destination.
    if (TBB != I->getOperand(0).getMBB())
      return true;

    auto OldBranchCode = (EraVMCC::CondCodes)Cond[0].getCImm()->getZExtValue();
    // If the conditions are the same, we can leave them alone.
    if (OldBranchCode == BranchCode)
      continue;

    return true;
  }

  return false;
}

unsigned EraVMInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() <= 1) && "EraVM branch conditions have one component!");
  assert(!BytesAdded && "code size not handled");

  if (Cond.empty()) {
    // Unconditional branch?
    assert(!FBB && "Unconditional branch with multiple successors!");
    BuildMI(&MBB, DL, get(EraVM::J)).addMBB(TBB);
    return 1;
  }
  // Conditional branch.
  unsigned Count = 0;
  auto cond_code = getImmOrCImm(Cond[0]);
  BuildMI(&MBB, DL, get(EraVM::JC)).addMBB(TBB).addImm(cond_code);
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(&MBB, DL, get(EraVM::J)).addMBB(FBB);
    ++Count;
  }
  return Count;
}

void EraVMInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  if (RC == &EraVM::GR256RegClass) {
    BuildMI(MBB, MI, DL, get(EraVM::ADDrrs_s))
        .addReg(SrcReg, getKillRegState(isKill))
        .addReg(EraVM::R0)
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addImm(0)
        .addMemOperand(MMO);
  } else if (RC == &EraVM::GRPTRRegClass) {
    BuildMI(MBB, MI, DL, get(EraVM::PTR_ADDrrs_s))
        .addReg(SrcReg, getKillRegState(isKill))
        .addReg(EraVM::R0)
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addImm(0)
        .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

void EraVMInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                          Register DestReg, int FrameIndex,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI,
                                          Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  if (RC == &EraVM::GR256RegClass) {
    BuildMI(MBB, MI, DL, get(EraVM::ADDsrr_s))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addReg(EraVM::R0)
        .addImm(0)
        .addMemOperand(MMO);
  } else if (RC == &EraVM::GRPTRRegClass) {
    BuildMI(MBB, MI, DL, get(EraVM::PTR_ADDsrr_s))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addReg(EraVM::R0)
        .addImm(0)
        .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

void EraVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator I,
                                 const DebugLoc &DL, MCRegister DestReg,
                                 MCRegister SrcReg, bool KillSrc) const {
  unsigned opcode = I->getFlag(MachineInstr::MIFlag::IsFatPtr)
                        ? EraVM::PTR_ADDrrr_s
                        : EraVM::ADDrrr_s;

  BuildMI(MBB, I, DL, get(opcode), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addReg(EraVM::R0)
      .addImm(0);
}

/// GetInstSize - Return the number of bytes of code the specified
/// instruction may be.  This returns the maximum number of bytes.
///
unsigned EraVMInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  const MCInstrDesc &Desc = MI.getDesc();
  if (Desc.getOpcode() == TargetOpcode::DBG_VALUE)
    return 0;
  return Desc.getSize();
}

#define INSTR_TESTER(NAME, OPCODE)                                             \
  bool EraVMInstrInfo::is##NAME(const MachineInstr &MI) const {                \
    StringRef Mnemonic = getName(MI.getOpcode());                              \
    return Mnemonic.starts_with(#OPCODE);                                      \
  }

INSTR_TESTER(Add, ADD)
INSTR_TESTER(Sub, SUB)
INSTR_TESTER(Mul, MUL)
INSTR_TESTER(Div, DIV)
INSTR_TESTER(And, AND)
INSTR_TESTER(Or, OR)
INSTR_TESTER(Xor, XOR)
INSTR_TESTER(Shl, SHL)
INSTR_TESTER(Shr, SHR)
INSTR_TESTER(Rol, ROL)
INSTR_TESTER(Ror, ROR)
INSTR_TESTER(Sel, SEL)
INSTR_TESTER(Load, LD)
INSTR_TESTER(Store, ST)
INSTR_TESTER(FatLoad, FATPTR_LD)
INSTR_TESTER(NOP, NOP)

bool EraVMInstrInfo::isPtr(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.starts_with("PTR");
}

bool EraVMInstrInfo::isNull(const MachineInstr &MI) const {
  return isAdd(MI) && EraVM::hasRRInAddressingMode(MI) &&
         MI.getNumDefs() == 1U && MI.getOperand(1).getReg() == EraVM::R0 &&
         MI.getOperand(2).getReg() == EraVM::R0;
}

bool EraVMInstrInfo::isSilent(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.ends_with("s");
}

/// Mark a copy to a fat pointer register or from a fat pointer register with
/// IsFatPtr MI flag. Then when COPY is lowered to a real instruction the flag
/// is used to choose between add and ptr.add.
void EraVMInstrInfo::tagFatPointerCopy(MachineInstr &MI) const {
  if (!MI.isCopy())
    return;

  const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
  Register DstReg = MI.getOperand(0).getReg();
  // If COPY doesn't have arguments use destination reg class twice in the
  // check.
  Register SrcReg = (MI.getNumOperands() > 1 && MI.getOperand(1).isReg())
                        ? MI.getOperand(1).getReg()
                        : DstReg;

  auto isFPReg = [&MRI](Register R) {
    return R.isVirtual() &&
           MRI.getRegClass(R)->getID() == EraVM::GRPTRRegClassID;
  };

  if (isFPReg(SrcReg) || isFPReg(DstReg))
    MI.setFlag(MachineInstr::MIFlag::IsFatPtr);
}

bool EraVMInstrInfo::isPredicatedInstr(const MachineInstr &MI) const {
  return MI.isBranch() || isArithmetic(MI) || isBitwise(MI) || isShift(MI) ||
         isRotate(MI) || isLoad(MI) || isFatLoad(MI) || isStore(MI) ||
         isNOP(MI) || isSel(MI) || isPtr(MI);
}

/// Returns the predicate operand
EraVMCC::CondCodes EraVMInstrInfo::getCCCode(const MachineInstr &MI) const {
  if (!isPredicatedInstr(MI))
    return EraVMCC::COND_INVALID;

  auto CC = MI.getOperand(MI.getNumExplicitOperands() - 1);

  if (CC.isImm())
    return EraVMCC::CondCodes(CC.getImm());
  if (CC.isCImm())
    return EraVMCC::CondCodes(CC.getCImm()->getZExtValue());
  return EraVMCC::COND_INVALID;
}

/// Return true if the first instruction in a function is stack advance.
static bool hasStackAdvanceInstruction(MachineFunction &MF) {
  auto EntryBB = MF.begin();
  auto FirstMI = EntryBB->getFirstNonDebugInstr();
  return FirstMI != EntryBB->end() && FirstMI->getOpcode() == EraVM::NOPSP;
}

/// Sets the offsets on instructions in MBB which use SP, so that they will be
/// valid post-outlining.
static void fixupStackOffsetPostOutline(MachineBasicBlock &MBB,
                                        int64_t FixupOffset) {
  auto AdjustDisp = [FixupOffset](MachineInstr::mop_iterator It) {
    if (EraVM::classifyStackAccess(It) != EraVM::StackAccess::Relative)
      return;

    auto DispIt = It + 2;
    assert(DispIt->isImm() && "Displacement is not immediate.");
    DispIt->setImm(DispIt->getImm() + FixupOffset);
  };

  for (MachineInstr &MI : MBB) {
    if (EraVM::hasSRInAddressingMode(MI))
      AdjustDisp(EraVM::in0Iterator(MI));
    if (EraVM::hasSROutAddressingMode(MI))
      AdjustDisp(EraVM::out0Iterator(MI));

    // Adjust sub instruction that was generated from ADDframe during
    // elimination of frame indices.
    if (MI.getOpcode() == EraVM::SUBxrr_s &&
        MI.getOperand(1).getTargetFlags() == EraVMII::MO_STACK_SLOT_IDX) {
      auto &StackSlotOP = MI.getOperand(1);
      StackSlotOP.setImm(StackSlotOP.getImm() - FixupOffset);
    }
  }
}

/// Since we are placing return address from the outlined function onto the top
/// of the stack, we need to reserve stack slot for it and to adjust
/// instructions which use SP in this function.
void EraVMInstrInfo::fixupPostOutline(MachineFunction &MF) const {
  auto *SFI = MF.getInfo<EraVMMachineFunctionInfo>();
  if (SFI->isStackAdjustedPostOutline())
    return;

  // Reserve stack slot for return address from outlined function.
  if (hasStackAdvanceInstruction(MF)) {
    auto NopIt = MF.begin()->getFirstNonDebugInstr();
    auto &StackAdvanceOp = NopIt->getOperand(0);
    StackAdvanceOp.setImm(StackAdvanceOp.getImm() + 1 /* StackSlotSize */);
  } else {
    auto EntryBB = MF.begin();
    BuildMI(*EntryBB, EntryBB->begin(), DebugLoc(), get(EraVM::NOPSP))
        .addImm(1 /* StackSlotSize */)
        .addImm(EraVMCC::COND_NONE);
  }

  // Adjust instructions which use SP in this function.
  for (MachineBasicBlock &MBB : MF)
    fixupStackOffsetPostOutline(MBB, -1 /* FixupOffset */);

  SFI->setStackAdjustedPostOutline();
}

/// Enum values indicating how an outlined call should be constructed.
enum MachineOutlinerConstructionID { MachineOutlinerDefault };

bool EraVMInstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  return MF.getFunction().hasMinSize();
}

bool EraVMInstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();

  // Can F be deduplicated by the linker? If it can, don't outline from it.
  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;

  // Don't outline from functions with section markings; the program could
  // expect that all the code is in the named section.
  if (F.hasSection())
    return false;

  // Don't outline from functions if there is non-adjustable stack relative
  // addressing instruction.
  for (MachineBasicBlock &MBB : MF) {
    if (any_of(
            instructionsWithoutDebug(MBB.begin(), MBB.end()),
            [](MachineInstr &MI) {
              if (EraVM::hasSRInAddressingMode(MI) &&
                  EraVM::hasInvalidRelativeStackAccess(EraVM::in0Iterator(MI)))
                return true;
              if (EraVM::hasSROutAddressingMode(MI) &&
                  EraVM::hasInvalidRelativeStackAccess(EraVM::out0Iterator(MI)))
                return true;
              return false;
            }))
      return false;
  }

  // It's safe to outline from MF.
  return true;
}

bool EraVMInstrInfo::isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                                            unsigned &Flags) const {
  return TargetInstrInfo::isMBBSafeToOutlineFrom(MBB, Flags);
}

outliner::InstrType
EraVMInstrInfo::getOutliningTypeImpl(MachineBasicBlock::iterator &MBBI,
                                     unsigned Flags) const {
  MachineInstr &MI = *MBBI;

  // Don't allow debug values to impact outlining type.
  if (MI.isDebugInstr() || MI.isIndirectDebugValue())
    return outliner::InstrType::Invisible;

  // Don't allow instructions which won't be materialized to impact outlining
  // analysis.
  if (MI.isMetaInstruction())
    return outliner::InstrType::Invisible;

  // Don't outline context.gas_left instruction.
  if (MI.getOpcode() == EraVM::CTXr_se &&
      getImmOrCImm(MI.getOperand(1)) == EraVMCTX::GAS_LEFT)
    return outliner::InstrType::Illegal;

  // Positions generally can't safely be outlined.
  if (MI.isPosition())
    return outliner::InstrType::Illegal;

  // Don't trust the user to write safe inline assembly.
  if (MI.isInlineAsm())
    return outliner::InstrType::Illegal;

  // We can't outline branches and return statements.
  if (MI.isTerminator() || MI.isReturn())
    return outliner::InstrType::Illegal;

  // Don't outline instructions that set or modify SP.
  if (MI.getDesc().hasImplicitDefOfPhysReg(EraVM::SP))
    return outliner::InstrType::Illegal;

  // Make sure the operands don't reference something unsafe.
  for (const auto &MO : MI.operands())
    if (MO.isMBB() || MO.isBlockAddress() || MO.isCPI() || MO.isJTI())
      return outliner::InstrType::Illegal;

  return outliner::InstrType::Legal;
}

std::optional<outliner::OutlinedFunction>
EraVMInstrInfo::getOutliningCandidateInfo(
    std::vector<outliner::Candidate> &RepeatedSequenceLocs) const {
  unsigned SequenceSize =
      std::accumulate(RepeatedSequenceLocs[0].begin(),
                      RepeatedSequenceLocs[0].end(), 0,
                      [this](unsigned Sum, const MachineInstr &MI) {
                        return Sum + getInstSizeInBytes(MI);
                      });

  SmallPtrSet<MachineFunction *, 4> NoStackAdvance;
  unsigned SaveRetSize = get(EraVM::ADDcrs_s).getSize();
  unsigned JumpSize = get(EraVM::JCALL).getSize();
  unsigned CallOverhead = SaveRetSize + JumpSize;
  for (auto &C : RepeatedSequenceLocs) {
    unsigned Overhead = CallOverhead;
    MachineFunction *MF = C.getMF();

    // If function doesn't have stack advance instruction, add overhead only
    // once for each function.
    if (!hasStackAdvanceInstruction(*MF) && NoStackAdvance.insert(MF).second)
      Overhead += get(EraVM::NOPSP).getSize();

    C.setCallInfo(MachineOutlinerDefault, Overhead);
  }

  unsigned JumpSPSize = get(EraVM::J_s).getSize();
  unsigned FrameOverhead = JumpSPSize;
  return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize,
                                    FrameOverhead, MachineOutlinerDefault);
}

void EraVMInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  // Perform stack fixup only if we didn't do it in the original function from
  // which we are outlining. Machine outliner algorithm is outlining from
  // the first candidate, so take original function from it.
  MachineFunction *OriginalMF = OF.Candidates.front().getMF();
  auto *SFI = OriginalMF->getInfo<EraVMMachineFunctionInfo>();
  if (!SFI->isStackAdjustedPostOutline())
    fixupStackOffsetPostOutline(MBB, -1 /* FixupOffset */);

  DebugLoc DL;
  if (!MBB.empty())
    DL = MBB.back().getDebugLoc();

  // Add jump instruction to the end of the outlined frame.
  MBB.insert(MBB.end(), BuildMI(MF, DL, get(EraVM::J_s))
                            .addReg(EraVM::SP)
                            .addImm(0 /* AMBase2 */)
                            .addImm(-1 /* StackOffset */));
}

MachineBasicBlock::iterator EraVMInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &OutlinedMF, outliner::Candidate &C) const {
  MachineFunction &MF = *C.getMF();
  fixupPostOutline(MF);

  DebugLoc DL;
  if (It != MBB.end())
    DL = It->getDebugLoc();

  // Add jump instruction to the outlined function at the given location.
  It = MBB.insert(It,
                  BuildMI(MF, DL, get(EraVM::JCALL))
                      .addGlobalAddress(M.getNamedValue(OutlinedMF.getName())));

  // Add symbol just after the jump that will be used as a return address
  // from the outlined function.
  MCSymbol *RetSym =
      MF.getContext().createTempSymbol("OUTLINED_FUNCTION_RET", true);
  It->setPostInstrSymbol(MF, RetSym);

  // Add instruction to store return address onto the top of the stack.
  BuildMI(MBB, It, DL, get(EraVM::ADDcrs_s))
      .addSym(RetSym)
      .addImm(0 /* RetSymOffset */)
      .addReg(EraVM::R0)
      .addReg(EraVM::SP)
      .addImm(0 /* AMBase2 */)
      .addImm(-1 /* StackOffset */)
      .addImm(EraVMCC::COND_NONE);

  return It;
}
