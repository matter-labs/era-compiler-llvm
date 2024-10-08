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
  case EVM::CONST_I256:
    return true;
  default:
    return false;
  }
}

void EVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator I,
                               const DebugLoc &DL, MCRegister DestReg,
                               MCRegister SrcReg, bool KillSrc) const {
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
  LLVM_DEBUG(dbgs() << "Analyzing branches of " << printMBBReference(MBB)
                    << '\n');

  // Assume this is a fall-through block and update that information later.
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  const auto *MFI = MBB.getParent()->getInfo<EVMMachineFunctionInfo>();
  if (MFI->getIsStackified()) {
    LLVM_DEBUG(dbgs() << "Can't analyze terminators in stackified code");
    return true;
  }

  // Iterate backwards and analyze all terminators.
  MachineBasicBlock::reverse_iterator I = MBB.rbegin(), E = MBB.rend();
  while (I != E) {
    if (I->isUnconditionalBranch()) {
      // There should be no other branches after the unconditional branch.
      assert(!TBB && !FBB && Cond.empty() && "Unreachable branch found");
      TBB = I->getOperand(0).getMBB();

      // Clean things up, if we're allowed to.
      if (AllowModify) {
        // There should be no instructions after the unconditional branch.
        assert(I == MBB.rbegin());

        // Delete the branch itself, if its target is the fall-through block.
        if (MBB.isLayoutSuccessor(TBB)) {
          LLVM_DEBUG(dbgs() << "Removing fall-through branch: "; I->dump());
          I->eraseFromParent();
          I = MBB.rbegin();
          TBB = nullptr; // Fall-through case.
          continue;
        }
      }
    } else if (I->isConditionalBranch()) {
      // There can't be several conditional branches in a single block just now.
      assert(Cond.empty() && "Several conditional branches?");

      // Set FBB to the destination of the previously encountered unconditional
      // branch (if there was any).
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();

      // Put the "use" of the condition into Cond[0].
      const MachineOperand &UseMO = I->getOperand(1);
      Cond.push_back(UseMO);

      // reverseBranch needs the instruction which feeds the branch, but only
      // supports comparisons. See if we can find one.
      for (MachineBasicBlock::reverse_iterator CI = I; CI != E; ++CI) {
        // If it is the right comparison, put its result into Cond[1].
        // TODO: This info is required for branch reversing, but this
        // is not yet implemented.
        if (CI->isCompare()) {
          const MachineOperand &DefMO = CI->getOperand(0);
          if (DefMO.getReg() == UseMO.getReg())
            Cond.push_back(DefMO);
          // Only give it one shot, this should be enough.
          break;
        }
      }
    } else if (I->isTerminator()) {
      // Return, indirect branch, fall-through, or some other unrecognized
      // terminator. Give up.
      LLVM_DEBUG(dbgs() << "Unrecognized terminator: "; I->dump());
      return true;
    } else if (!I->isDebugValue()) {
      // This is an ordinary instruction, meaning there are no terminators left
      // to process. Finish the analysis.
      break;
    }

    ++I;
  }

  // Check that there are no unaccounted terminators left.
  assert(std::none_of(I, E, [](MachineBasicBlock::reverse_iterator I) {
    return I->isTerminator();
  }));

  // If we didn't bail out earlier, the analysis was successful.
  return false;
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

  const bool IsUncondBranch = Cond.empty();
  const bool IsCondBranch =
      (Cond.size() == 1 && Cond[0].isReg()) ||
      (Cond.size() == 2 && Cond[0].isReg() && Cond[1].isReg());

  // Insert a branch to the "true" destination.
  assert(TBB && "A branch must have a destination");
  if (IsUncondBranch)
    BuildMI(&MBB, DL, get(EVM::JUMP)).addMBB(TBB);
  else if (IsCondBranch)
    BuildMI(&MBB, DL, get(EVM::JUMPI)).addMBB(TBB).add(Cond[0]);
  else
    llvm_unreachable("Unexpected branching condition");

  ++InstrCount;

  // If there is also a "false" destination, insert another branch.
  if (FBB) {
    assert(!Cond.empty() && "Unconditional branch can't have two destinations");
    BuildMI(&MBB, DL, get(EVM::JUMP)).addMBB(FBB);
    ++InstrCount;
  }

  return InstrCount;
}

bool EVMInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  // TODO: CPR-1557. Try to add support for branch reversing. The main problem
  // is that it may require insertion of additional instructions in the BB.
  // For example,
  //
  //   NE $3, $2, $1
  //
  // should be transformed into
  //
  //   EQ $3, $2, $1
  //   ISZERO $4, $3

  return true;
}
