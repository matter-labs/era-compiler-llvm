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
#include "llvm/CodeGen/MachineModuleInfo.h"
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
  Opcode = getWithInsNotSwapped(Opcode);
  // TODO: CPR-1355 Mappings for Select.
  // Select is not a part of a mapping, so have to handle it manually.
  const DenseSet<unsigned> In0R = {
      EraVM::SELrrr, EraVM::SELrir,        EraVM::SELrcr,
      EraVM::SELrsr, EraVM::FATPTR_SELrrr, EraVM::SELrrs,
      EraVM::SELris, EraVM::SELrcs,        EraVM::SELrss};
  const DenseSet<unsigned> In0I = {EraVM::SELirr, EraVM::SELiir, EraVM::SELicr,
                                   EraVM::SELisr, EraVM::SELirs, EraVM::SELiis,
                                   EraVM::SELics, EraVM::SELiss};
  const DenseSet<unsigned> In0C = {EraVM::SELcrr, EraVM::SELcir, EraVM::SELccr,
                                   EraVM::SELcsr, EraVM::SELcrs, EraVM::SELcis,
                                   EraVM::SELccs, EraVM::SELcss};
  const DenseSet<unsigned> In0S = {EraVM::SELsrr, EraVM::SELsir, EraVM::SELscr,
                                   EraVM::SELssr, EraVM::SELsrs, EraVM::SELsis,
                                   EraVM::SELscs, EraVM::SELsss};
  const DenseSet<unsigned> In1R = {
      EraVM::SELrrr, EraVM::SELirr,        EraVM::SELcrr,
      EraVM::SELsrr, EraVM::FATPTR_SELrrr, EraVM::SELrrs,
      EraVM::SELirs, EraVM::SELcrs,        EraVM::SELsrs};
  const DenseSet<unsigned> In1I = {EraVM::SELrir, EraVM::SELiir, EraVM::SELcir,
                                   EraVM::SELsir, EraVM::SELris, EraVM::SELiis,
                                   EraVM::SELcis, EraVM::SELsis};
  const DenseSet<unsigned> In1C = {EraVM::SELrcr, EraVM::SELicr, EraVM::SELccr,
                                   EraVM::SELscr, EraVM::SELrcs, EraVM::SELics,
                                   EraVM::SELccs, EraVM::SELscs};
  const DenseSet<unsigned> In1S = {EraVM::SELrsr, EraVM::SELisr, EraVM::SELcsr,
                                   EraVM::SELssr, EraVM::SELrss, EraVM::SELiss,
                                   EraVM::SELcss, EraVM::SELsss};
  if (Kind == ArgumentKind::Out1) {
    // TODO: CPR-986 Support stack output for Select.
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
  if (hasSROutAddressingMode(MI))
    return in1Iterator(MI) + argumentSize(ArgumentKind::In1, MI);
  return MI.operands_begin();
}

MachineInstr::mop_iterator out1Iterator(MachineInstr &MI) {
  return MI.operands_begin() + MI.getNumExplicitDefs() - 1;
}

MachineInstr::mop_iterator ccIterator(MachineInstr &MI) {
  return MI.operands_begin() + (MI.getNumExplicitOperands() - 1);
}

int getWithRRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAddrMode_OpndRR); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAddrMode_OpndRR); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAddrMode_OpndRR); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAddrMode_OpndRR);
}

int getWithIRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAddrMode_OpndIR); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAddrMode_OpndIR); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAddrMode_OpndIR); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAddrMode_OpndIR);
}

int getWithCRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAddrMode_OpndCR); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAddrMode_OpndCR); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAddrMode_OpndCR); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAddrMode_OpndCR);
}

int getWithSRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAddrMode_OpndSR); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAddrMode_OpndSR); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAddrMode_OpndSR); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAddrMode_OpndSR);
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
  return (unsigned)mapRRInputTo(Opcode, OperandAddrMode_OpndRR) == Opcode;
}

bool hasIRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapIRInputTo(Opcode, OperandAddrMode_OpndIR) == Opcode;
}

bool hasCRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapCRInputTo(Opcode, OperandAddrMode_OpndCR) == Opcode;
}

bool hasSRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapSRInputTo(Opcode, OperandAddrMode_OpndSR) == Opcode;
}

bool hasAnyInAddressingMode(unsigned Opcode) {
  return hasRRInAddressingMode(Opcode) || hasIRInAddressingMode(Opcode) ||
         hasCRInAddressingMode(Opcode) || hasSRInAddressingMode(Opcode);
}

bool hasRROutAddressingMode(unsigned Opcode) {
  return withStackResult(Opcode) != -1;
}

bool hasSROutAddressingMode(unsigned Opcode) {
  return withRegisterResult(Opcode) != -1;
}

bool hasAnyOutAddressingMode(unsigned Opcode) {
  return hasRROutAddressingMode(Opcode) || hasSROutAddressingMode(Opcode);
}

// TODO: CPR-1355 Implement in via td.
bool isSelect(unsigned Opcode) {
  DenseSet<unsigned> Members = {
      EraVM::SELrrr,       EraVM::SELrir, EraVM::SELrcr, EraVM::SELrsr,
      EraVM::SELirr,       EraVM::SELiir, EraVM::SELicr, EraVM::SELisr,
      EraVM::SELcrr,       EraVM::SELcir, EraVM::SELccr, EraVM::SELcsr,
      EraVM::SELsrr,       EraVM::SELsir, EraVM::SELscr, EraVM::SELssr,
      EraVM::SELrrs,       EraVM::SELris, EraVM::SELrcs, EraVM::SELrss,
      EraVM::SELirs,       EraVM::SELiis, EraVM::SELics, EraVM::SELiss,
      EraVM::SELcrs,       EraVM::SELcis, EraVM::SELccs, EraVM::SELcss,
      EraVM::SELsrs,       EraVM::SELsis, EraVM::SELscs, EraVM::SELsss,
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

unsigned int
EraVMInstrInfo::getTailDuplicateSize(CodeGenOpt::Level OptLevel) const {
  // After benchmarking tail duplication sizes of 2, 3, 4, 6, and 10, it was
  // determined that a size of 3 provides the best trade-off between code size
  // and performance at optimization level -O3.
  return OptLevel >= CodeGenOpt::Aggressive ? 3 : 2;
}

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

  auto CC = static_cast<EraVMCC::CondCodes>(getImmOrCImm(Cond[0]));

  auto NewCC = getReversedCondition(CC);
  if (!NewCC)
    return true;

  Cond.pop_back();
  Cond.push_back(MachineOperand::CreateImm(*NewCC));
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
    if (!I->isTerminator())
      break;

    // A terminator that isn't a branch can't easily be handled
    // by this analysis.
    if (!I->isBranch())
      return true;

    // We can't analyze indirect branches, since the jump target
    // is unknown.
    if (I->isIndirectBranch())
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
  assert(Cond.size() <= 1 && "EraVM branch conditions have one component!");
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
  BuildMI(&MBB, DL, get(EraVM::JCl)).addMBB(TBB).addImm(cond_code);
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(&MBB, DL, get(EraVM::J)).addMBB(FBB);
    ++Count;
  }
  return Count;
}

bool EraVMInstrInfo::analyzeCompare(const MachineInstr &MI, Register &SrcReg,
                                    Register &SrcReg2, int64_t &CmpMask,
                                    int64_t &CmpValue) const {
  return isSub(MI) && isFlagSettingInstruction(MI);
}

bool EraVMInstrInfo::optimizeCompareInstr(
    MachineInstr &CmpInstr, Register SrcReg, Register SrcReg2, int64_t CmpMask,
    int64_t CmpValue, const MachineRegisterInfo *MRI) const {
  assert(CmpInstr.getParent() && "CmpInstr must be in a basic block");
  assert(MRI && "MachineRegisterInfo is required");

  // If Flags register is not used, convert this compare instruction (sub
  // instruction that sets flags) to a sub that doesn't set flags.
  int DeadFlagsIdx = CmpInstr.findRegisterDefOperandIdx(EraVM::Flags, true);
  if (DeadFlagsIdx != -1) {
    int NewOpc = EraVM::getNonFlagSettingOpcode(CmpInstr.getOpcode());
    assert(NewOpc != -1 && "Invalid opcode for compare instruction");
    CmpInstr.setDesc(get(NewOpc));
    CmpInstr.removeOperand(DeadFlagsIdx);
    return true;
  }

  // TODO: #621 Support removing compare instruction which output is used if the
  // nearest flag setting instruction is identical.
  if (!EraVM::hasRROutAddressingMode(CmpInstr) ||
      !MRI->use_nodbg_empty(CmpInstr.getOperand(0).getReg()))
    return false;

  // In some cases, we can have the following sequence:
  //   Out1 = SUB_v In1, In2, CondCC1, implicit-def $flags
  //   ...
  //   Out2 = SUB_v In3, In4, CondCC2, implicit-def dead $flags
  //   ...
  //   Out3 = SUB_v In1, In2, CondCC1, implicit-def $flags
  //
  // In this case, `Out2 = SUB_v` prevents MachineCSE to remove
  // `Out3 = SUB_v` in favor of `Out1 = SUB_v`, just because of
  // `implicit-def dead $flags`.
  // Since in this function we will convert `Out2 = SUB_v` to
  // `Out2 = SUB_s` and implicit dead $flags will be dropped,
  // we can try to find identical compare instruction and remove it here.
  //
  // Search backwards in order to find the nearest flag setting instruction that
  // can be identical to this compare instruction.
  auto From = std::next(MachineBasicBlock::reverse_iterator(CmpInstr));
  auto FlagSettingInst = std::find_if(
      From, CmpInstr.getParent()->rend(), [](const MachineInstr &MI) {
        return any_of(MI.implicit_operands(), [](const MachineOperand &MO) {
          return MO.isReg() && MO.isDef() && MO.getReg() == EraVM::Flags;
        });
      });

  // Bail out if we didn't find identical compare instruction.
  if (FlagSettingInst == CmpInstr.getParent()->rend() ||
      !isFlagSettingInstruction(*FlagSettingInst) ||
      !FlagSettingInst->isIdenticalTo(CmpInstr, MachineInstr::IgnoreVRegDefs))
    return false;

  CmpInstr.eraseFromParent();
  return true;
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
        .addImm(EraVMCC::COND_NONE)
        .addMemOperand(MMO);
  } else if (RC == &EraVM::GRPTRRegClass) {
    BuildMI(MBB, MI, DL, get(EraVM::PTR_ADDrrs_s))
        .addReg(SrcReg, getKillRegState(isKill))
        .addReg(EraVM::R0)
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addImm(EraVMCC::COND_NONE)
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
        .addImm(EraVMCC::COND_NONE)
        .addMemOperand(MMO);
  } else if (RC == &EraVM::GRPTRRegClass) {
    BuildMI(MBB, MI, DL, get(EraVM::PTR_ADDsrr_s))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addReg(EraVM::R0)
        .addImm(EraVMCC::COND_NONE)
        .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

MachineInstr *EraVMInstrInfo::insertIncSP(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                          const DebugLoc &DL,
                                          unsigned Increment) const {
  assert(Increment == (uint16_t)Increment &&
         "Increment does not fit into uint16_t. Is it negative integer?");
  return BuildMI(MBB, MI, DL, get(EraVM::NOPrrs))
      .addReg(EraVM::R0)           // rs0
      .addReg(EraVM::R0)           // rs1
      .addReg(EraVM::R0)           // dst0: marker
      .addReg(EraVM::R0)           // dst0: base
      .addImm(Increment)           // dst0: addend
      .addImm(EraVMCC::COND_NONE); // pred
}

MachineInstr *EraVMInstrInfo::insertDecSP(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                          const DebugLoc &DL,
                                          unsigned Decrement) const {
  assert(Decrement == (uint16_t)Decrement &&
         "Decrement does not fit into uint16_t. Is it negative integer?");
  return BuildMI(MBB, MI, DL, get(EraVM::NOPsrr))
      .addReg(EraVM::R0)           // rd0
      .addReg(EraVM::R0)           // src0: marker
      .addReg(EraVM::R0)           // src0: base
      .addImm(Decrement)           // src0: addend
      .addReg(EraVM::R0)           // rs1
      .addImm(EraVMCC::COND_NONE); // pred
}

void EraVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator I,
                                 const DebugLoc &DL, MCRegister DestReg,
                                 MCRegister SrcReg, bool KillSrc,
                                 bool RenamableDest, bool RenamableSrc) const {
  unsigned opcode = I->getFlag(MachineInstr::MIFlag::IsFatPtr)
                        ? EraVM::PTR_ADDrrr_s
                        : EraVM::ADDrrr_s;

  BuildMI(MBB, I, DL, get(opcode), DestReg)
      .addReg(SrcReg,
              getKillRegState(KillSrc) | getRenamableRegState(RenamableSrc))
      .addReg(EraVM::R0)
      .addImm(EraVMCC::COND_NONE);
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

MachineInstr *EraVMInstrInfo::createPHIDestinationCopy(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsPt,
    const DebugLoc &DL, Register Src, Register Dst) const {
  auto *Copy =
      TargetInstrInfo::createPHIDestinationCopy(MBB, InsPt, DL, Src, Dst);
  tagFatPointerCopy(*Copy);
  return Copy;
}

MachineInstr *EraVMInstrInfo::createPHISourceCopy(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsPt,
    const DebugLoc &DL, Register Src, unsigned SrcSubReg, Register Dst) const {
  auto *Copy =
      TargetInstrInfo::createPHISourceCopy(MBB, InsPt, DL, Src, SrcSubReg, Dst);
  tagFatPointerCopy(*Copy);
  return Copy;
}

bool EraVMInstrInfo::isPredicatedInstr(const MachineInstr &MI) const {
  if (MI.isBranch())
    return !MI.isUnconditionalBranch();
  return isArithmetic(MI) || isBitwise(MI) || isShift(MI) || isRotate(MI) ||
         isLoad(MI) || isFatLoad(MI) || isStore(MI) || isNOP(MI) || isSel(MI) ||
         isPtr(MI);
}

/// Returns the predicate operand
EraVMCC::CondCodes EraVMInstrInfo::getCCCode(const MachineInstr &MI) const {
  if (!EraVMInstrInfo::isPredicatedInstr(MI))
    return EraVMCC::COND_INVALID;

  // predicated CC is the last operand of the instruction
  auto CC = *EraVM::ccIterator(MI);

  if (CC.isImm())
    return EraVMCC::CondCodes(CC.getImm());
  if (CC.isCImm())
    return EraVMCC::CondCodes(CC.getCImm()->getZExtValue());
  return EraVMCC::COND_INVALID;
}

std::optional<DestSourcePair>
EraVMInstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.getOpcode() == EraVM::ADDrrr_s &&
      MI.getOperand(2).getReg() == EraVM::R0 &&
      getCCCode(MI) == EraVMCC::COND_NONE) {
    return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
  }
  return std::nullopt;
}

/// Return whether outlining candidate ends with a tail call. This is true only
/// if it is a terminator that ends function, call to outlined function that
/// ends with a tail call, or call to a no return function.
static bool isOutliningCandidateTailCall(const MachineInstr &MI) {
  const MachineBasicBlock *MBB = MI.getParent();
  if (MI.isTerminator()) {
    assert(MBB->succ_empty() && "Terminator is not ending a function.");
    return true;
  }

  auto GetFunction = [](const MachineOperand &MO) -> const Function * {
    if (!MO.isGlobal())
      return nullptr;
    return dyn_cast<Function>(MO.getGlobal());
  };

  // Check if outlined function ends with tail call.
  if (MI.getOpcode() == EraVM::JCALL) {
    const Function *Callee = GetFunction(MI.getOperand(0));
    if (!Callee)
      return false;

    MachineFunction *CalleeMF =
        MBB->getParent()->getMMI().getMachineFunction(*Callee);
    return CalleeMF &&
           CalleeMF->getInfo<EraVMMachineFunctionInfo>()->isOutlined() &&
           CalleeMF->getInfo<EraVMMachineFunctionInfo>()->isTailCall();
  }

  // Check if we have a call to a no return function.
  if (MI.getOpcode() != EraVM::NEAR_CALL)
    return false;

  const Function *Callee = GetFunction(MI.getOperand(1));
  if (!Callee)
    return false;

  if (Callee->doesNotReturn())
    return true;

  // Sometimes, special functions are not marked as noreturn, so check
  // if we are calling one, and do additional checks if it is a last
  // instruction in an ending MBB.
  return std::next(MI.getIterator()) == MBB->end() && MBB->succ_empty() &&
         (Callee->getName() == "__exit_return" ||
          Callee->getName() == "__exit_revert");
}

/// Return true if the first instruction in a function is stack advance
/// instruction accepting an immediate operand.
static bool hasTrivialStackAdvanceInstruction(MachineFunction &MF,
                                              MachineOperand *&StackAdvanceOp) {
  auto EntryBB = MF.begin();
  auto FirstMI = EntryBB->getFirstNonDebugInstr();

  StackAdvanceOp = nullptr;
  if (FirstMI == EntryBB->end())
    return false;

  if (FirstMI->getOpcode() != EraVM::NOPrrs)
    return false;

  // Expected stack+=[r0 + imm]
  MachineOperand &MarkerOp = FirstMI->getOperand(2);
  MachineOperand &BaseOp = FirstMI->getOperand(3);
  MachineOperand &AddendOp = FirstMI->getOperand(4);

  if (!MarkerOp.isReg() || MarkerOp.getReg() != EraVM::R0)
    return false;
  if (!BaseOp.isReg() || BaseOp.getReg() != EraVM::R0)
    return false;

  StackAdvanceOp = &AddendOp;
  return true;
}

/// Sets the offsets on instructions in [Start, End) which use SP, so that they
/// will be valid post-outlining.
static void fixupStackAccessOffsetPostOutline(MachineBasicBlock::iterator Start,
                                              MachineBasicBlock::iterator End,
                                              int64_t FixupOffset) {
  auto AdjustDisp = [FixupOffset](MachineInstr::mop_iterator It) {
    if (EraVM::classifyStackAccess(It) != EraVM::StackAccess::Relative)
      return;

    auto *DispIt = It + 2;
    assert(DispIt->isImm() && "Displacement is not immediate.");
    DispIt->setImm(DispIt->getImm() + FixupOffset);
  };

  for (MachineInstr &MI : make_range(Start, End)) {
    // Skip adjusting instruction that is used to set return address
    // from outlining function.
    if (any_of(MI.operands(), [](const MachineOperand &MO) {
          return MO.getTargetFlags() == EraVMII::MO_SYM_RET_ADDRESS;
        }))
      continue;

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
void EraVMInstrInfo::fixupStackPostOutline(MachineFunction &MF) const {
  auto *SFI = MF.getInfo<EraVMMachineFunctionInfo>();
  if (SFI->isStackAdjustedPostOutline())
    return;

  // Reserve stack slot for return address from outlined function.
  MachineOperand *StackAdvanceOp = nullptr;
  if (hasTrivialStackAdvanceInstruction(MF, StackAdvanceOp)) {
    StackAdvanceOp->setImm(StackAdvanceOp->getImm() + 1 /* StackSlotSize */);
  } else {
    auto EntryBB = MF.begin();
    insertIncSP(*EntryBB, EntryBB->begin(), DebugLoc(), 1 /* StackSlotSize */);
  }

  // Adjust instructions which use SP in this function.
  for (MachineBasicBlock &MBB : MF)
    fixupStackAccessOffsetPostOutline(MBB.begin(), MBB.end(),
                                      -1 /* FixupOffset */);

  SFI->setStackAdjustedPostOutline();
}

/// Constants defining how certain sequences should be outlined.
/// This encompasses how an outlined function should be called, and what kind of
/// frame should be emitted for that outlined function.
///
/// \p MachineOutlinerDefault implies that the function should be called with
/// a save return address onto TOS and jump back from address on TOS.
///
/// That is,
///
/// I1     Save RET ADDR onto TOS     OUTLINED_FUNCTION:
/// I2 --> J OUTLINED_FUNCTION        I1
/// I3     RET ADDR SYM               I2
///                                   I3
///                                   J TOS
///
/// * Call construction overhead: 2 (save + J) + 1? (SP advance)
/// * Frame construction overhead: 1 (J)
/// * Requires stack fixups? Yes
///
/// \p MachineOutlinerTailCall implies that the function is being created from
/// a sequence of instructions ending in a return.
///
/// That is,
///
/// I1                             OUTLINED_FUNCTION:
/// I2 --> J OUTLINED_FUNCTION     I1
/// RET                            I2
///                                RET
///
/// * Call construction overhead: 1 (J)
/// * Frame construction overhead: 0 (Return included in sequence)
/// * Requires stack fixups? No, if all callers don't need to do stack fixup
///
enum MachineOutlinerConstructionID {
  MachineOutlinerDefault, /// Emit save, jump and jump to return.
  MachineOutlinerTailCall /// Only emit a jump.
};

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

  // Don't outline function that was previously outlined.
  if (MF.getInfo<EraVMMachineFunctionInfo>()->isOutlined())
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

  // Is this a terminator for a basic block?
  if (MI.isTerminator()) {

    // Is this the end of a function?
    if (MI.getParent()->succ_empty())
      return outliner::InstrType::Legal;

    // It's not, so don't outline it.
    return outliner::InstrType::Illegal;
  }

  // Don't outline jump instruction that is used to call non-tail outlined
  // function.
  if (MI.getOpcode() == EraVM::JCALL) {
    const MachineOperand &MO = MI.getOperand(0);

    // If it's not a global, we can outline it.
    if (!MO.isGlobal())
      return outliner::InstrType::Legal;

    const auto *Callee = dyn_cast<Function>(MO.getGlobal());

    // Only check for functions.
    if (!Callee)
      return outliner::InstrType::Legal;

    MachineFunction *CalleeMF =
        MI.getMF()->getMMI().getMachineFunction(*Callee);

    // We don't know what's going on with the callee at all. Don't touch it.
    if (!CalleeMF)
      return outliner::InstrType::Legal;

    auto *SFI = CalleeMF->getInfo<EraVMMachineFunctionInfo>();

    // We can safely outline calls to non-outlined or tail outlined functions.
    if (!SFI->isOutlined() || SFI->isTailCall())
      return outliner::InstrType::Legal;

    // Don't outline this, as it is paired with instruction that adds return
    // address onto TOS.
    return outliner::InstrType::Illegal;
  }

  // Don't outline context.gas_left instruction.
  if (MI.getOpcode() == EraVM::CTXGasLeft)
    return outliner::InstrType::Illegal;

  // Positions generally can't safely be outlined.
  if (MI.isPosition())
    return outliner::InstrType::Illegal;

  // Don't trust the user to write safe inline assembly.
  if (MI.isInlineAsm())
    return outliner::InstrType::Illegal;

  // Don't outline instructions that set or modify SP.
  if (MI.getDesc().hasImplicitDefOfPhysReg(EraVM::SP))
    return outliner::InstrType::Illegal;

  // Make sure the operands don't reference something unsafe.
  for (const auto &MO : MI.operands())
    if (MO.isMBB() || MO.isBlockAddress() || MO.isCPI() || MO.isJTI() ||
        MO.getTargetFlags() == EraVMII::MO_SYM_RET_ADDRESS)
      return outliner::InstrType::Illegal;

  return outliner::InstrType::Legal;
}

std::optional<outliner::OutlinedFunction>
EraVMInstrInfo::getOutliningCandidateInfo(
    std::vector<outliner::Candidate> &RepeatedSequenceLocs) const {
  unsigned SequenceSize =
      std::accumulate(RepeatedSequenceLocs[0].front(),
                      std::next(RepeatedSequenceLocs[0].back()), 0,
                      [this](unsigned Sum, const MachineInstr &MI) {
                        return Sum + getInstSizeInBytes(MI);
                      });

  unsigned FrameID = MachineOutlinerDefault;
  unsigned SaveRetSize = get(EraVM::ADDcrs_s).getSize();
  unsigned JumpSize = get(EraVM::JCALL).getSize();
  unsigned JumpSPSize = get(EraVM::J_s).getSize();

  // For tail calls, we are just using jump.
  if (isOutliningCandidateTailCall(*RepeatedSequenceLocs[0].back())) {
    FrameID = MachineOutlinerTailCall;
    SaveRetSize = 0;
    JumpSPSize = 0;
  }

  SmallPtrSet<MachineFunction *, 4> NoStackAdvance;
  unsigned CallOverhead = SaveRetSize + JumpSize;
  for (auto &C : RepeatedSequenceLocs) {
    unsigned Overhead = CallOverhead;
    MachineFunction *MF = C.getMF();

    // If function doesn't have stack advance instruction, add overhead only
    // once for each function.
    // Add this overhead also for tail calls, as we can end up adjusting stack
    // in caller just to align stack accesses with other callers.
    MachineOperand *UnusedOp = nullptr;
    if (!hasTrivialStackAdvanceInstruction(*MF, UnusedOp) &&
        NoStackAdvance.insert(MF).second)
      Overhead += get(EraVM::NOPrrs).getSize();

    C.setCallInfo(FrameID, Overhead);
  }

  unsigned FrameOverhead = JumpSPSize;
  return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize,
                                    FrameOverhead, FrameID);
}

void EraVMInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  bool IsTailCall = OF.FrameConstructionID == MachineOutlinerTailCall;

  // Add jump instruction to the end of the outlined frame.
  if (!IsTailCall) {
    DebugLoc DL;
    if (!MBB.empty())
      DL = MBB.back().getDebugLoc();

    MBB.insert(MBB.end(), BuildMI(MF, DL, get(EraVM::J_s))
                              .addReg(EraVM::SP)
                              .addImm(0 /* AMBase2 */)
                              .addImm(-1 /* StackOffset */));
  }

  auto *SFI = MF.getInfo<EraVMMachineFunctionInfo>();
  SFI->setIsOutlined();
  SFI->setIsTailCall(IsTailCall);
}

MachineBasicBlock::iterator EraVMInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &OutlinedMF, outliner::Candidate &C) const {
  MachineFunction &MF = *C.getMF();
  DebugLoc DL;
  if (It != MBB.end())
    DL = It->getDebugLoc();

  // Add jump instruction to the outlined function at the given location.
  It = MBB.insert(It,
                  BuildMI(MF, DL, get(EraVM::JCALL))
                      .addGlobalAddress(M.getNamedValue(OutlinedMF.getName())));

  if (C.CallConstructionID != MachineOutlinerTailCall) {
    // Add symbol just after the jump that will be used as a return address
    // from the outlined function.
    MCSymbol *RetSym =
        MF.getContext().createTempSymbol("OUTLINED_FUNCTION_RET", true);
    It->setPostInstrSymbol(MF, RetSym);

    // Add instruction to store return address onto the top of the stack.
    BuildMI(MBB, It, DL, get(EraVM::ADDirs_s))
        .addSym(RetSym, EraVMII::MO_SYM_RET_ADDRESS)
        .addReg(EraVM::R0)
        .addReg(EraVM::SP)
        .addImm(0 /* AMBase2 */)
        .addImm(-1 /* StackOffset */)
        .addImm(EraVMCC::COND_NONE);
  }

  return It;
}

void EraVMInstrInfo::fixupPostOutlining(
    std::vector<std::pair<MachineFunction *, std::vector<MachineFunction *>>>
        &FixupFunctions) const {
  // First, adjust all outlined functions with MachineOutlinerDefault strategy.
  for (auto [Outlined, Callers] : FixupFunctions) {
    if (Outlined->getInfo<EraVMMachineFunctionInfo>()->isTailCall())
      continue;

    // Skip adjusting last instruction which is jump that loads return address
    // from the stack.
    auto &OutlinedMBB = Outlined->front();
    fixupStackAccessOffsetPostOutline(OutlinedMBB.begin(),
                                      std::prev(OutlinedMBB.end()),
                                      -1 /* FixupOffset */);
    for (auto *Caller : Callers)
      fixupStackPostOutline(*Caller);
  }

  // Remove all functions that we don't need to process. These are outlined
  // functions with MachineOutlinerDefault strategy, and tail call outlined
  // functions that don't have stack relative access instructions.
  erase_if(FixupFunctions, [](auto &Funcs) {
    auto Outlined = Funcs.first;
    if (!Outlined->template getInfo<EraVMMachineFunctionInfo>()->isTailCall())
      return true;
    if (none_of(Outlined->front(), [](MachineInstr &MI) {
          return (EraVM::hasSRInAddressingMode(MI) &&
                  EraVM::classifyStackAccess(EraVM::in0Iterator(MI)) ==
                      EraVM::StackAccess::Relative) ||
                 (EraVM::hasSROutAddressingMode(MI) &&
                  EraVM::classifyStackAccess(EraVM::out0Iterator(MI)) ==
                      EraVM::StackAccess::Relative) ||
                 (MI.getOpcode() == EraVM::SUBxrr_s &&
                  MI.getOperand(1).getTargetFlags() ==
                      EraVMII::MO_STACK_SLOT_IDX);
        }))
      return true;
    return false;
  });

  // After that, adjust all outlined functions with MachineOutlinerTailCall
  // strategy. Repeat this iteration as long as we are changing something,
  // because it can happen that we first decide to not adjust some function
  // because callers weren't adjusted, but in later iterations we adjust one
  // of the callers. See this example in machine-outliner-tail.mir.
  bool Changed = false;
  do {
    Changed = false;
    for (auto [Outlined, Callers] : FixupFunctions) {
      auto *SFI = Outlined->getInfo<EraVMMachineFunctionInfo>();

      // Skip outlined functions that we already adjusted.
      if (SFI->isStackAdjustedPostOutline())
        continue;

      // Skip if there is no caller that adjusted sp.
      if (none_of(Callers, [](MachineFunction *MF) {
            return MF->getInfo<EraVMMachineFunctionInfo>()
                ->isStackAdjustedPostOutline();
          }))
        continue;

      auto &OutlinedMBB = Outlined->front();
      fixupStackAccessOffsetPostOutline(OutlinedMBB.begin(), OutlinedMBB.end(),
                                        -1 /* FixupOffset */);
      for (auto *Caller : Callers)
        fixupStackPostOutline(*Caller);

      // Set that we adjusted this outlined function, so we can skip it.
      SFI->setStackAdjustedPostOutline();
      Changed = true;
    }
  } while (Changed);
}

bool EraVMInstrInfo::PredicateInstruction(MachineInstr &MI,
                                          ArrayRef<MachineOperand> Pred) const {
  assert(Pred.size() == 1);
  auto CC = static_cast<EraVMCC::CondCodes>(getImmOrCImm(Pred[0]));

  if (!EraVMInstrInfo::updateCCCode(MI, CC))
    return false;

  // Add implicit Flags register if instruction doesn't have it.
  if (none_of(MI.implicit_operands(), [](const MachineOperand &MO) {
        return MO.isReg() && MO.isUse() && MO.getReg() == EraVM::Flags;
      }))
    MachineInstrBuilder(*MI.getParent()->getParent(), MI)
        .addReg(EraVM::Flags, RegState::Implicit);

  return true;
}

std::optional<EraVMCC::CondCodes>
EraVMInstrInfo::getReversedCondition(EraVMCC::CondCodes CC) {
  const std::unordered_map<EraVMCC::CondCodes, EraVMCC::CondCodes> ReverseMap =
      {
          {EraVMCC::COND_E, EraVMCC::COND_NE},
          {EraVMCC::COND_NE, EraVMCC::COND_E},
          {EraVMCC::COND_LT, EraVMCC::COND_GE},
          {EraVMCC::COND_GE, EraVMCC::COND_LT},
          {EraVMCC::COND_LE, EraVMCC::COND_GT},
          {EraVMCC::COND_GT, EraVMCC::COND_LE},
      };

  auto it = ReverseMap.find(CC);
  return (it == ReverseMap.end()) ? std::optional<EraVMCC::CondCodes>()
                                  : it->second;
}

bool EraVMInstrInfo::isPredicated(const MachineInstr &MI) const {
  if (!isPredicatedInstr(MI))
    return false;

  EraVMCC::CondCodes CC = getCCCode(MI);
  return CC != EraVMCC::COND_INVALID && CC != EraVMCC::COND_NONE;
}

bool EraVMInstrInfo::isPredicable(const MachineInstr &MI) const {
  if (!isPredicatedInstr(MI))
    return false;

  // Do not predicate gas_left instruction
  if (MI.getOpcode() == EraVM::CTXGasLeft)
    return false;

  // we cannot make a flag setting instruction predicated execute
  if (EraVMInstrInfo::isFlagSettingInstruction(MI.getOpcode()))
    return false;

  // CPR-1241: We temporarily disable ld/st instructions from being predicated
  // until assembler is fixed to support predicated ld/st instructions.
  if (isLoad(MI) || isFatLoad(MI) || isStore(MI))
    return false;

  // check condition code validity.
  EraVMCC::CondCodes CC = getCCCode(MI);
  return CC != EraVMCC::COND_INVALID;
}

static bool probabilityIsProfitable(unsigned TrueCycles, unsigned FalseCycles,
                                    BranchProbability Probability) {
  // opportunistically convert it:
  // We calculate the mathematical expectation of the conversion,
  // and see if the probability will satisfy that after conversion
  // the expected number of cycles are actually smaller.
  // The original expectation is calculated as:
  // (1 - P)*NumFCycles + P*(NumTCycles + jump) + (jump)
  // = (1 - P) * (FalseCycles) + P * (TrueCycles + 1) + 1
  //
  // The converted expectation is (TrueCycles + FalseCycles).
  // Note that if we conditionally execute TBB, then we eliminate one jump
  // at the end.
  //
  // so we need to check if it is profitable after conversion:
  // TrueCycles + FalseCycles < (1-P) * (FalseCycles) + P*(TrueCycles + 1) +1
  // -> P > (TrueCycles - 1) / (TrueCycles - FalseCycles + 1), and 0 <= P <= 1

  // manually handle the case where invalid branch probability will be
  // calculated.
  if (TrueCycles < 2 || FalseCycles > 2)
    return false;

  unsigned Numerator = TrueCycles - 1;
  unsigned Denominator = TrueCycles - FalseCycles + 1;
  if (!Denominator || Numerator > Denominator)
    return false;
  return Probability >= BranchProbability(Numerator, Denominator);
}

static bool isOptimizeForSize(MachineFunction &MF) {
  return MF.getFunction().hasOptSize();
}

bool EraVMInstrInfo::isProfitableToIfCvt(
    MachineBasicBlock &TMBB, unsigned NumTCycles, unsigned ExtraTCycles,
    MachineBasicBlock &FMBB, unsigned NumFCycles, unsigned ExtraFCycles,
    BranchProbability Probability) const {
  // If we are optimizing for size, it is always profitable to ifcvt,
  // because we are eliminating branchings.
  if (isOptimizeForSize(*TMBB.getParent()))
    return true;

  // profitable if execution sizes of both T and F are small enough
  if (NumTCycles <= EraVMInstrInfo::MAX_MBB_SIZE_TO_ALWAYS_IFCVT &&
      NumFCycles <= EraVMInstrInfo::MAX_MBB_SIZE_TO_ALWAYS_IFCVT)
    return true;

  // do not convert if probability is unknown
  if (Probability.isUnknown())
    return false;

  return probabilityIsProfitable(NumTCycles, NumFCycles, Probability);
}

bool EraVMInstrInfo::isProfitableToDupForIfCvt(
    MachineBasicBlock &MBB, unsigned NumInstrs,
    BranchProbability Probability) const {
  // if sensitive about size, just do not duplicate MBB:
  if (isOptimizeForSize(*MBB.getParent()))
    return false;

  // profitable if execution size is small enough
  if (NumInstrs <= EraVMInstrInfo::MAX_MBB_SIZE_TO_ALWAYS_IFCVT)
    return true;

  // do not duplicate if probability is unknown
  if (Probability.isUnknown())
    return false;

  // 1 instruction is 1 cycle
  return probabilityIsProfitable(NumInstrs, 0, Probability);
}

bool EraVMInstrInfo::isProfitableToIfCvt(MachineBasicBlock &MBB,
                                         unsigned NumCycles,
                                         unsigned ExtraPredCycles,
                                         BranchProbability Probability) const {
  // always profitable if optimizing for size
  if (isOptimizeForSize(*MBB.getParent()))
    return true;

  // profitable if execution size is small enough
  if (NumCycles <= EraVMInstrInfo::MAX_MBB_SIZE_TO_ALWAYS_IFCVT)
    return true;
  // do not convert if probability is unknown
  if (Probability.isUnknown())
    return false;

  return probabilityIsProfitable(NumCycles, 0, Probability);
}

bool EraVMInstrInfo::updateCCCode(MachineInstr &MI,
                                  EraVMCC::CondCodes CC) const {
  // cannot update CC code to invalid
  if (CC == EraVMCC::COND_INVALID)
    return false;

  if (!EraVMInstrInfo::isPredicatedInstr(MI))
    return false;

  auto &Opnd = *EraVM::ccIterator(MI);
  if (!(Opnd.isImm() || Opnd.isCImm()))
    return false;

  if (Opnd.isImm())
    Opnd.setImm(CC);
  else if (Opnd.isCImm())
    Opnd.setCImm(ConstantInt::get(Opnd.getCImm()->getType(), CC, false));
  return true;
}
