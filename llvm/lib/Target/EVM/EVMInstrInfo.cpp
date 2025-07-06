//===------------------ EVMInstrInfo.cpp - EVM Instruction Information ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EVM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "EVMInstrInfo.h"
#include "EVMMachineFunctionInfo.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
using namespace llvm;

#define DEBUG_TYPE "evm-instr-info"

#define GET_INSTRINFO_CTOR_DTOR
#include "EVMGenInstrInfo.inc"

EVMInstrInfo::EVMInstrInfo()
    : EVMGenInstrInfo(EVM::ADJCALLSTACKDOWN, EVM::ADJCALLSTACKUP), RI() {}

bool EVMInstrInfo::isReallyTriviallyReMaterializable(
    const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case EVM::ADDRESS:
  case EVM::CALLER:
  case EVM::CALLVALUE:
  case EVM::CALLDATASIZE:
  case EVM::CODESIZE:
  case EVM::CONST_I256:
    return true;
  default:
    return false;
  }
}

void EVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, MCRegister DestReg,
                               MCRegister SrcReg, bool KillSrc,
                               bool RenamableDest, bool RenamableSrc) const {
  // This method is called by post-RA expansion, which expects only
  // phys registers to exist. However we expect only virtual here.
  assert(Register::isVirtualRegister(DestReg) &&
         "Unexpected physical register");

#ifndef NDEBUG
  auto &MRI = MBB.getParent()->getRegInfo();
  assert((MRI.getRegClass(DestReg) == &EVM::GPRRegClass) &&
         "Unexpected register class");
#endif // NDEBUG

  BuildMI(MBB, I, DL, get(EVM::COPY_I256), DestReg)
      .addReg(SrcReg, KillSrc ? RegState::Kill : 0);
}

// Branch analysis.
bool EVMInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                 MachineBasicBlock *&TBB,
                                 MachineBasicBlock *&FBB,
                                 SmallVectorImpl<MachineOperand> &Cond,
                                 bool AllowModify) const {
  SmallVector<MachineInstr *, 2> BranchInstrs;
  BranchType BT = analyzeBranch(MBB, TBB, FBB, Cond, AllowModify, BranchInstrs);
  return BT == BT_None;
}

EVMInstrInfo::BranchType EVMInstrInfo::analyzeBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *&TBB, MachineBasicBlock *&FBB,
    SmallVectorImpl<MachineOperand> &Cond, bool AllowModify,
    SmallVectorImpl<MachineInstr *> &BranchInstrs) const {

  LLVM_DEBUG(dbgs() << "Analyzing branches of " << printMBBReference(MBB)
                    << '\n');

  // Assume this is a fall-through block and update that information later.
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  const bool IsStackified =
      MBB.getParent()->getInfo<EVMMachineFunctionInfo>()->getIsStackified();

  MachineBasicBlock::reverse_iterator I = MBB.rbegin(), REnd = MBB.rend();

  // Skip all the debug instructions.
  while (I != REnd && I->isDebugInstr())
    ++I;

  if (I == REnd || !isUnpredicatedTerminator(*I)) {
    // This block ends with no branches (it just falls through to its succ).
    // Leave TBB/FBB null.
    return BT_NoBranch;
  }
  if (!I->isBranch()) {
    // Not a branch terminator.
    return BT_None;
  }

  MachineInstr *LastInst = &*I++;
  MachineInstr *SecondLastInst = nullptr;

  // Skip any debug instruction to see if the second last is a branch.
  while (I != REnd && I->isDebugInstr())
    ++I;

  if (I != REnd && I->isBranch())
    SecondLastInst = &*I++;

  // Check that there are no unaccounted terminators left.
  assert(I == REnd ||
         std::none_of(I, REnd, [](MachineBasicBlock::reverse_iterator I) {
           return I->isTerminator();
         }));

  if (LastInst->isUnconditionalBranch()) {
    TBB = LastInst->getOperand(0).getMBB();

    // Clean things up, if we're allowed to.
    if (AllowModify) {
      // There should be no instructions after the unconditional branch.
      assert(LastInst == MBB.rbegin());

      // Delete the branch itself, if its target is the fall-through block.
      if (MBB.isLayoutSuccessor(TBB)) {
        LLVM_DEBUG(dbgs() << "Removing fall-through branch: ";
                   LastInst->dump());
        LastInst->eraseFromParent();
        TBB = nullptr; // Fall-through case.
      }
    }
    if (TBB)
      BranchInstrs.push_back(LastInst);

    if (!SecondLastInst)
      return TBB ? BT_Uncond : BT_NoBranch;
  }

  MachineInstr *CondBr = SecondLastInst ? SecondLastInst : LastInst;
  assert(CondBr->isConditionalBranch());
  BranchInstrs.push_back(CondBr);

  // Set FBB to the destination of the previously encountered unconditional
  // branch (if there was any).
  FBB = TBB;
  TBB = CondBr->getOperand(0).getMBB();

  // Set from which instruction this condition comes from. It is needed for
  // reversing and inserting of branches.
  Cond.push_back(
      MachineOperand::CreateImm(CondBr->getOpcode() == EVM::JUMPI ||
                                CondBr->getOpcode() == EVM::PseudoJUMPI));
  if (!IsStackified) {
    // Put the "use" of the condition into Cond[1].
    const MachineOperand &UseMO = CondBr->getOperand(1);
    Cond.push_back(UseMO);
  }

  if (!SecondLastInst)
    return BT_Cond;

  return BT_CondUncond;
}

unsigned EVMInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  assert(!BytesRemoved && "Code size not handled");

  LLVM_DEBUG(dbgs() << "Removing branches out of " << printMBBReference(MBB)
                    << '\n');

  unsigned Count = 0;
  // Only remove branches from the end of the MBB.
  for (auto I = MBB.rbegin(); I != MBB.rend() && I->isBranch(); ++Count) {

#ifndef NDEBUG
    if (I->isUnconditionalBranch())
      assert(!Count && "Malformed basic block: unconditional branch is not the "
                       "last instruction in the block");
#endif // NDEBUG

    LLVM_DEBUG(dbgs() << "Removing branch: "; I->dump());
    MBB.erase(&MBB.back());
    I = MBB.rbegin();
  }
  return Count;
}

unsigned EVMInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL, int *BytesAdded) const {
  assert(!BytesAdded && "Code size not handled");

  // The number of instructions inserted.
  unsigned InstrCount = 0;
  const bool IsStackified =
      MBB.getParent()->getInfo<EVMMachineFunctionInfo>()->getIsStackified();
  unsigned UncondOpc = !IsStackified ? EVM::JUMP : EVM::PseudoJUMP;

  // Insert a branch to the "true" destination.
  assert(TBB && "A branch must have a destination");
  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(UncondOpc)).addMBB(TBB);
  } else {
    // Destinguish between stackified and non-stackified instructions.
    unsigned CondOpc = 0;
    if (!IsStackified)
      CondOpc = Cond[0].getImm() ? EVM::JUMPI : EVM::JUMP_UNLESS;
    else
      CondOpc = Cond[0].getImm() ? EVM::PseudoJUMPI : EVM::PseudoJUMP_UNLESS;

    auto NewMI = BuildMI(&MBB, DL, get(CondOpc)).addMBB(TBB);

    // Add a condition operand, if we are not in stackified form.
    if (!IsStackified) {
      assert(
          Cond.size() == 2 &&
          "Unexpected number of conditional operands in non-stackified code");
      NewMI.add(Cond[1]);
    }
  }

  ++InstrCount;

  // If there is also a "false" destination, insert another branch.
  if (FBB) {
    assert(!Cond.empty() && "Unconditional branch can't have two destinations");
    BuildMI(&MBB, DL, get(UncondOpc)).addMBB(FBB);
    ++InstrCount;
  }

  return InstrCount;
}

bool EVMInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert((Cond.size() == 1 || Cond.size() == 2) &&
         "Unexpected number of conditional operands");
  assert(Cond[0].isImm() && "Unexpected condition type");
  Cond.front() = MachineOperand::CreateImm(!Cond.front().getImm());
  return false;
}
