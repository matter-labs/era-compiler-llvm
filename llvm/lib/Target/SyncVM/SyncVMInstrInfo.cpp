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
    // Remove the branch.
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return Count;
}

bool SyncVMInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Xbranch condition!");

  SyncVMCC::CondCodes CC = static_cast<SyncVMCC::CondCodes>(
      Cond[0].isImm() ? Cond[0].getImm() : Cond[0].getCImm()->getZExtValue());

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

  Cond.pop_back();
  Cond.push_back(MachineOperand::CreateImm(CC));
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

    SyncVMCC::CondCodes BranchCode =
        static_cast<SyncVMCC::CondCodes>(getImmOrCImm(I->getOperand(1)));
    if (BranchCode == SyncVMCC::COND_INVALID)
      return true; // Can't handle weird stuff.

    // Working from the bottom, handle the first conditional branch.
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      LLVMContext &C = MBB.getParent()->getFunction().getContext();
      auto CImmCC =
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

    SyncVMCC::CondCodes OldBranchCode =
        (SyncVMCC::CondCodes)Cond[0].getCImm()->getZExtValue();
    // If the conditions are the same, we can leave them alone.
    if (OldBranchCode == BranchCode)
      continue;

    return true;
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
    BuildMI(&MBB, DL, get(SyncVM::J)).addMBB(TBB).addImm(SyncVMCC::COND_NONE);
    return 1;
  }
  // Conditional branch.
  unsigned Count = 0;
  auto cond_code = getImmOrCImm(Cond[0]);
  BuildMI(&MBB, DL, get(SyncVM::J)).addMBB(TBB).addImm(cond_code);
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(&MBB, DL, get(SyncVM::J)).addMBB(FBB).addImm(SyncVMCC::COND_NONE);
    ++Count;
  }
  return Count;
}

static bool isFatPtrValue(const MachineInstr& MI, const SyncVMInstrInfo& TII, const MachineRegisterInfo &MRI) {
  if (MI.getOpcode() == TargetOpcode::COPY) {
    Register MOReg = MI.getOperand(1).getReg();
    if (const MachineInstr *MI = MRI.getUniqueVRegDef(MOReg))
      return isFatPtrValue(*MI, TII, MRI);
    return false;
  }
  if (TII.getName(MI.getOpcode()).startswith("PTR_"))
    return true;
  return false;
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
  auto *TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &SyncVM::GR256RegClass) {
    if (MI != MBB.end() && !isFatPtrValue(*MI, *TII, MRI))
      BuildMI(MBB, MI, DL, get(SyncVM::ADDrrs_s))
          .addReg(SrcReg, getKillRegState(isKill))
          .addReg(SyncVM::R0)
          .addFrameIndex(FrameIdx)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
    else
      BuildMI(MBB, MI, DL, get(SyncVM::PTR_ADDrrs_s))
          .addReg(SrcReg, getKillRegState(isKill))
          .addReg(SyncVM::R0)
          .addFrameIndex(FrameIdx)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

void SyncVMInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIdx, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  auto *TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &SyncVM::GR256RegClass) {
    if (!isFatPtrValue(*MI, *TII, MRI))
      BuildMI(MBB, MI, DL, get(SyncVM::ADDsrr_s))
          .addReg(DestReg, getDefRegState(true))
          .addFrameIndex(FrameIdx)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
    else
      BuildMI(MBB, MI, DL, get(SyncVM::PTR_ADDsrr_s))
          .addReg(DestReg, getDefRegState(true))
          .addFrameIndex(FrameIdx)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

void SyncVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const DebugLoc &DL, MCRegister DestReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  MachineFunction &MF = *MBB.getParent();
  auto *TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  if (!isFatPtrValue(*I, *TII, MRI))
    BuildMI(MBB, I, DL, get(SyncVM::ADDrrr_s), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addReg(SyncVM::R0)
        .addImm(0);
  else
    BuildMI(MBB, I, DL, get(SyncVM::PTR_ADDrrr_s), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addReg(SyncVM::R0)
        .addImm(0);
}

/// GetInstSize - Return the number of bytes of code the specified
/// instruction may be.  This returns the maximum number of bytes.
///
unsigned SyncVMInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  const MCInstrDesc &Desc = MI.getDesc();
  if (Desc.getOpcode() == TargetOpcode::DBG_VALUE)
    return 0;
  return Desc.getSize();
}

bool SyncVMInstrInfo::isAdd(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.startswith("ADD");
}

bool SyncVMInstrInfo::isSub(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.startswith("SUB");
}

bool SyncVMInstrInfo::isMul(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.startswith("MUL");
}

bool SyncVMInstrInfo::isDiv(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.startswith("DIV");
}

bool SyncVMInstrInfo::isSilent(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.endswith("s");
}

SyncVMInstrInfo::GenericInstruction
SyncVMInstrInfo::genericInstructionFor(const MachineInstr &MI) const {
  if (isAdd(MI))
    return ADD;
  if (isSub(MI))
    return SUB;
  if (isMul(MI))
    return MUL;
  if (isDiv(MI))
    return DIV;
  return Unsupported;
}

bool SyncVMInstrInfo::hasRIOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return find(Mnemonic, 'i') != Mnemonic.end();
}

bool SyncVMInstrInfo::hasRXOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return find(Mnemonic, 'x') != Mnemonic.end();
}

bool SyncVMInstrInfo::hasRSOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  auto LCIt = find_if<StringRef, int(int)>(std::move(Mnemonic), std::islower);

  return LCIt != Mnemonic.end() && (*LCIt == 's' || *LCIt == 'y');
}

bool SyncVMInstrInfo::hasRROperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  auto LCIt = find_if<StringRef, int(int)>(std::move(Mnemonic), std::islower);

  return LCIt != Mnemonic.end() && *LCIt == 'r' && *std::next(LCIt) == 'r';
}
