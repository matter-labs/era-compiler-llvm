//===-- SyncVMInstrInfo.cpp - SyncVM Instruction Information --------------===//
//
// This file contains the SyncVM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "SyncVMInstrInfo.h"
#include "SyncVM.h"
#include "SyncVMMachineFunctionInfo.h"
#include "SyncVMTargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "SyncVMGenInstrInfo.inc"

// Pin the vtable to this file.
void SyncVMInstrInfo::anchor() {}

SyncVMInstrInfo::SyncVMInstrInfo()
    : SyncVMGenInstrInfo(SyncVM::ADJCALLSTACKDOWN, SyncVM::ADJCALLSTACKUP),
      RI() {}

unsigned SyncVMInstrInfo::removeBranch(MachineBasicBlock &MBB,
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
    assert(I->getOperand(0).getMBB() == I->getOperand(1).getMBB() &&
           "removing non-falling-through branch");
    // Remove the branch.
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return 0;
}

bool SyncVMInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Xbranch condition!");

  SyncVMCC::CondCodes CC = static_cast<SyncVMCC::CondCodes>(Cond[0].getImm());

  switch (CC) {
  default:
    llvm_unreachable("Invalid branch condition!");
  case SyncVMCC::COND_E:
    CC = SyncVMCC::COND_NE;
    break;
  case SyncVMCC::COND_NE:
    CC = SyncVMCC::COND_E;
    break;
  case SyncVMCC::COND_LT:
    CC = SyncVMCC::COND_GE;
    break;
  case SyncVMCC::COND_GE:
    CC = SyncVMCC::COND_LT;
    break;
  case SyncVMCC::COND_LE:
    CC = SyncVMCC::COND_GT;
    break;
  case SyncVMCC::COND_GT:
    CC = SyncVMCC::COND_LE;
    break;
  }

  Cond[0].setImm(CC);
  return false;
}

bool SyncVMInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
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
    if (I->getOperand(0).getMBB() == I->getOperand(1).getMBB()) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        break;
      }

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
        break;
      }

      // TBB is used to indicate the unconditinal destination.
      TBB = I->getOperand(0).getMBB();
      continue;
    }

    TBB = I->getOperand(0).getMBB();
    FBB = I->getOperand(1).getMBB();
  }

  return false;
}

unsigned SyncVMInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.size() == 0) &&
         "SyncVM branch conditions have one component!");
  assert(!BytesAdded && "code size not handled");

  if (Cond.empty()) {
    // Unconditional branch?
    assert(!FBB && "Unconditional branch with multiple successors!");
    BuildMI(&MBB, DL, get(SyncVM::JCC))
        .addMBB(TBB)
        .addMBB(TBB)
        .addImm(SyncVMCC::COND_NONE);
  } else {
    // Conditional branch.
    BuildMI(&MBB, DL, get(SyncVM::JCC))
        .addMBB(TBB)
        .addMBB(FBB)
        .addImm(Cond[0].getImm());
  }

  return 1;
}
void SyncVMInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                          Register SrcReg, bool isKill,
                                          int FrameIdx,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &SyncVM::GR256RegClass)
    BuildMI(MBB, MI, DL, get(SyncVM::MOVrs))
        .addReg(SrcReg, getKillRegState(isKill))
        .addFrameIndex(FrameIdx)
        .addImm(0)
        .addImm(32)
        .addMemOperand(MMO);
  else
    llvm_unreachable("Cannot store this register to stack slot!");
}

void SyncVMInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIdx, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &SyncVM::GR256RegClass)
    BuildMI(MBB, MI, DL, get(SyncVM::MOVsr))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIdx)
        .addImm(0)
        .addImm(32)
        .addMemOperand(MMO);
  else
    llvm_unreachable("Cannot store this register to stack slot!");
}

void SyncVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const DebugLoc &DL, MCRegister DestReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  BuildMI(MBB, I, DL, get(SyncVM::MOVrr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addReg(SyncVM::R0);
}

/// GetInstSize - Return the number of bytes of code the specified
/// instruction may be.  This returns the maximum number of bytes.
///
unsigned SyncVMInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  return 256;
}
