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
#include "EraVM.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMTargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "EraVMGenInstrInfo.inc"

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

    // Handle unconditional branches.
    if (getImmOrCImm(I->getOperand(1)) == 0) {
      // TBB is used to indicate the unconditional destination.
      TBB = I->getOperand(0).getMBB();

      if (AllowModify) {
        // If the block has any instructions after a JMP, delete them.
        while (std::next(I) != MBB.end())
          std::next(I)->eraseFromParent();
        Cond.clear();
        FBB = nullptr;

        // Delete the JMP if it's equivalent to a fall-through.
        if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
          TBB = nullptr;
          I->eraseFromParent();
          I = MBB.end();
          continue;
        }
      }

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
    BuildMI(&MBB, DL, get(EraVM::J)).addMBB(TBB).addImm(EraVMCC::COND_NONE);
    return 1;
  }
  // Conditional branch.
  unsigned Count = 0;
  auto cond_code = getImmOrCImm(Cond[0]);
  BuildMI(&MBB, DL, get(EraVM::J)).addMBB(TBB).addImm(cond_code);
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(&MBB, DL, get(EraVM::J)).addMBB(FBB).addImm(EraVMCC::COND_NONE);
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

  if (RC == &EraVM::GR256RegClass)
    BuildMI(MBB, MI, DL, get(EraVM::ADDrrs_s))
        .addReg(SrcReg, getKillRegState(isKill))
        .addReg(EraVM::R0)
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addImm(0)
        .addMemOperand(MMO);
  else
    llvm_unreachable("Cannot store this register to stack slot!");
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

  if (RC == &EraVM::GR256RegClass)
    BuildMI(MBB, MI, DL, get(EraVM::ADDsrr_s))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIndex)
        .addImm(32)
        .addImm(0)
        .addImm(0)
        .addImm(0)
        .addMemOperand(MMO);
  else
    llvm_unreachable("Cannot store this register to stack slot!");
}

void EraVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator I,
                                 const DebugLoc &DL, MCRegister DestReg,
                                 MCRegister SrcReg, bool KillSrc) const {
  BuildMI(MBB, I, DL, get(EraVM::ADDrrr_s), DestReg)
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
