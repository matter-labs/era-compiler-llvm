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
#include "llvm/MC/MCContext.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#define GET_INSTRMAP_INFO
#include "EraVMGenInstrInfo.inc"

namespace llvm {
namespace EraVM {

ArgumentType argumentType(ArgumentKind Kind, unsigned Opcode) {
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
  return (unsigned)mapRRInputTo(Opcode, OperandAM_0) == Opcode;
}

bool hasIRInAddressingMode(unsigned Opcode) {
  return (unsigned)mapIRInputTo(Opcode, OperandAM_1) == Opcode;
}

bool hasCRInAddressingMode(unsigned Opcode) {
  return (unsigned)mapCRInputTo(Opcode, OperandAM_2) == Opcode;
}

bool hasSRInAddressingMode(unsigned Opcode) {
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
    return Mnemonic.startswith(#OPCODE);                                       \
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
  return Mnemonic.startswith("PTR");
}

bool EraVMInstrInfo::isNull(const MachineInstr &MI) const {
  return isAdd(MI) && EraVM::hasRRInAddressingMode(MI) &&
         MI.getNumDefs() == 1U && MI.getOperand(1).getReg() == EraVM::R0 &&
         MI.getOperand(2).getReg() == EraVM::R0;
}

bool EraVMInstrInfo::isSilent(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.endswith("s");
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
         isNOP(MI) || isSel(MI);
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

enum FlagsUsage { FlagsUseBeforeDef = 0, FlagsDef, NoFlags };

/// Return how Flags register is used in [Start, End).
static FlagsUsage getFlagsUsage(MachineBasicBlock::iterator Start,
                                MachineBasicBlock::iterator End) {
  for (MachineInstr &I : instructionsWithoutDebug(Start, End)) {
    if (I.getDesc().hasImplicitDefOfPhysReg(EraVM::Flags))
      return FlagsDef;

    if (I.readsRegister(EraVM::Flags))
      return FlagsUseBeforeDef;
  }

  return NoFlags;
}

/// Enum values indicating how an outlined call should be constructed.
enum MachineOutlinerConstructionID { MachineOutlinerDefault };

enum MachineOutlinerMBBFlags {
  FlagsRegDead = 1 << 0,
  OutlinerMBBFlagsMask = (1 << 1) - 1
};

// TODO: Enable this.
// bool EraVMInstrInfo::shouldOutlineFromFunctionByDefault(
//     MachineFunction &MF) const {
//   return MF.getFunction().hasOptSize();
// }

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

  // It's safe to outline from MF.
  return true;
}

bool EraVMInstrInfo::isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                                            unsigned &Flags) const {
  if (!TargetInstrInfo::isMBBSafeToOutlineFrom(MBB, Flags))
    return false;

  bool SeenDef = false;
  bool SeenUse = false;
  bool HasFlagsInst = false;

  // Check how Flags register is used in the MBB. We can't outline this MBB if
  // we find use before def, or def without use.
  for (MachineInstr &I : MBB) {
    if (I.isCall()) {
      SeenDef = false;
    } else if (I.getDesc().hasImplicitDefOfPhysReg(EraVM::Flags)) {
      SeenDef = true;
      SeenUse = false;
      HasFlagsInst = true;
    } else if (I.readsRegister(EraVM::Flags)) {
      // Return false if we find use before def.
      if (!SeenDef)
        return false;
      SeenUse = true;
      HasFlagsInst = true;
    }
  }

  // Return false if we find def without use.
  if (SeenDef && !SeenUse)
    return false;

  // If Flags register is not used in this MBB, we know we don't have to check
  // it later.
  if (!HasFlagsInst)
    Flags |= MachineOutlinerMBBFlags::FlagsRegDead;

  // Don't check for successors if this MBB has no predecessors
  // and no Flags instructions.
  if (MBB.pred_empty() && !HasFlagsInst)
    return true;

  SmallVector<MachineBasicBlock *, 4> Worklist{MBB.successors()};
  SmallPtrSet<MachineBasicBlock *, 4> Visited{&MBB};

  // Check if Flags register is propagated across MBBs.
  while (!Worklist.empty()) {
    MachineBasicBlock *SuccMBB = Worklist.pop_back_val();
    if (!Visited.insert(SuccMBB).second)
      continue;

    auto Usage = getFlagsUsage(SuccMBB->begin(), SuccMBB->end());

    // If Flags register is propagated across MBBs, we know outlining is
    // unsafe.
    if (Usage == FlagsUseBeforeDef)
      return false;

    // If there are no Flags usage in this MBB, we need to look further.
    if (Usage == NoFlags)
      append_range(Worklist, SuccMBB->successors());
  }

  return true;
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

  // TODO: Remove this once issue with heap growth is fixed in VM.
  switch (MI.getOpcode()) {
  case EraVM::LD1:
  case EraVM::LD1Inc:
  case EraVM::LD1i:
  case EraVM::LD2:
  case EraVM::LD2Inc:
  case EraVM::LD2i:
  case EraVM::ST1:
  case EraVM::ST1Inc:
  case EraVM::ST1i:
  case EraVM::ST2:
  case EraVM::ST2Inc:
  case EraVM::ST2i:
    return outliner::InstrType::Illegal;
  }

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

  // Since we don't outline branches, don't outline instruction that defines
  // Flags register that is used by conditional branch.
  if (!MI.isCall() && MI.getDesc().hasImplicitDefOfPhysReg(EraVM::Flags)) {
    auto TermIt = MI.getParent()->getFirstTerminator();

    // If there is a conditional branch and there are no more instructions
    // that define Flags register, don't outline this instruction.
    if (TermIt != MI.getParent()->end() && TermIt->isConditionalBranch() &&
        !any_of(instructionsWithoutDebug(std::next(MBBI), TermIt),
                [](const MachineInstr &I) {
                  return I.getDesc().hasImplicitDefOfPhysReg(EraVM::Flags);
                }))
      return outliner::InstrType::Illegal;
  }

  return outliner::InstrType::Legal;
}

std::optional<outliner::OutlinedFunction>
EraVMInstrInfo::getOutliningCandidateInfo(
    std::vector<outliner::Candidate> &RepeatedSequenceLocs) const {
  constexpr unsigned MinCandidatesToOutline = 2;
  unsigned SequenceSize =
      std::accumulate(RepeatedSequenceLocs[0].front(),
                      std::next(RepeatedSequenceLocs[0].back()), 0,
                      [this](unsigned Sum, const MachineInstr &MI) {
                        return Sum + getInstSizeInBytes(MI);
                      });

  // Properties about candidate MBBs that hold for all of them.
  unsigned FlagsSetInAll = OutlinerMBBFlagsMask;

  // Compute liveness information for each candidate, and set FlagsSetInAll.
  for (outliner::Candidate &C : RepeatedSequenceLocs)
    FlagsSetInAll &= C.Flags;

  // Are there any candidates where Flags register is live?
  if (!(FlagsSetInAll & FlagsRegDead)) {
    // Since Flags register is cleared on entry/exit from a function call,
    // we can't outline any sequence of instructions where this register
    // is live into/across it.
    auto CantGuaranteeValueAcrossCall = [](outliner::Candidate &C) {
      // If the unsafe register in this block is dead, then we don't need
      // to compute liveness here.
      if (C.Flags & FlagsRegDead)
        return false;

      // Check if Flags register is defined outside, but used in a sequence.
      if (getFlagsUsage(C.front(), std::next(C.back())) == FlagsUseBeforeDef)
        return true;

      // Check if Flags register is propagated across a sequence.
      if (getFlagsUsage(std::next(C.back()), C.getMBB()->end()) ==
          FlagsUseBeforeDef)
        return true;

      return false;
    };

    // Erase every candidate that violates the restriction above.
    llvm::erase_if(RepeatedSequenceLocs, CantGuaranteeValueAcrossCall);

    // If the sequence doesn't have enough candidates left, then we're done.
    if (RepeatedSequenceLocs.size() < MinCandidatesToOutline)
      return {};
  }

  unsigned CallOverhead = get(EraVM::NEAR_CALL).getSize();
  for (auto &C : RepeatedSequenceLocs)
    C.setCallInfo(MachineOutlinerDefault, CallOverhead);

  unsigned FrameOverhead = get(EraVM::RET).getSize();
  return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize,
                                    FrameOverhead, MachineOutlinerDefault);
}

void EraVMInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  DebugLoc DL;
  if (!MBB.empty())
    DL = MBB.back().getDebugLoc();

  // Add return instruction to the end of the outlined frame.
  MBB.insert(MBB.end(), BuildMI(MF, DL, get(EraVM::RET)));
}

MachineBasicBlock::iterator EraVMInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, outliner::Candidate &C) const {
  DebugLoc DL;
  if (It != MBB.end())
    DL = It->getDebugLoc();

  // Add call instruction to the outlined function at the given location.
  It = MBB.insert(It, BuildMI(MF, DL, get(EraVM::NEAR_CALL))
                          .addReg(EraVM::R0)
                          .addGlobalAddress(M.getNamedValue(MF.getName()))
                          .addExternalSymbol("DEFAULT_UNWIND"));
  return It;
}
