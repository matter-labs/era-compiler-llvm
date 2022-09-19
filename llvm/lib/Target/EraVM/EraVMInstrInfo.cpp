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

static bool isDefinedAsFatPtr(const MachineInstr &MI, const EraVMInstrInfo &TII,
                              const MachineRegisterInfo &MRI) {
  // FIXME: For some reason loops of COPY definition sometimes appear. It needs
  // further investigation.
  static DenseSet<const MachineInstr *> Visited;
  if (!Visited.count(&MI) && MI.getOpcode() == TargetOpcode::COPY) {
    Visited.insert(&MI);
    Register MOReg = MI.getOperand(1).getReg();
    if (const MachineInstr *MI = MRI.getUniqueVRegDef(MOReg)) {
      return isDefinedAsFatPtr(*MI, TII, MRI);
    }
    return false;
  }
  if (TII.getName(MI.getOpcode()).starts_with("PTR_"))
    return true;
  return false;
}

void EraVMInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  auto *TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  if (RC == &EraVM::GR256RegClass) {
    MachineInstr *Def = MRI.getUniqueVRegDef(SrcReg);
    // TODO: This code should go off once MVT::fatptr is properly supported
    // R1 needs special threatment because far calls produce fat pointer in R1.
    bool IsFatPtrInPhysReg = false;
    if (!Def && SrcReg == EraVM::R1) {
      auto findSrcRegDef = [SrcReg](const MachineInstr &Inst) {
        return Inst.isCall() ||
               any_of(Inst.defs(), [SrcReg](const MachineOperand &MO) {
                 return MO.getReg() == SrcReg;
               });
      };

      // Look for the closest definition in MBB.
      auto DefI =
          std::find_if(std::make_reverse_iterator(MI),
                       std::make_reverse_iterator(MBB.begin()), findSrcRegDef);
      if (DefI != std::make_reverse_iterator(MBB.begin()))
        IsFatPtrInPhysReg = isFarCall(*DefI);
      // If not found check predecessors.
      // It's not expected to have a fatptr from one branch, and i256 from
      // another, so it's ok to find the very first definition using dfs. Loops
      // are an issue, so track visited BBs to not to continue ad infinum.
      if (!IsFatPtrInPhysReg) {
        const MachineBasicBlock &Entry = MBB.getParent()->front();
        DenseSet<const MachineBasicBlock *> Visited{&MBB};
        std::deque<const MachineBasicBlock *> WorkList(MBB.pred_begin(),
                                                       MBB.pred_end());
        while (!WorkList.empty()) {
          const MachineBasicBlock &CurrBB = *WorkList.front();
          if (Visited.count(&CurrBB)) {
            WorkList.pop_front();
            continue;
          }
          Visited.insert(&CurrBB);
          // Look for the closest definition of R1.
          auto DefI = std::find_if(MBB.rbegin(), MBB.rend(), findSrcRegDef);
          if (DefI != MBB.rend()) {
            IsFatPtrInPhysReg = isFarCall(*DefI);
            break;
          }
          // TODO: If this code persist for a reson, handle calling conventions
          // here.
          if (IsFatPtrInPhysReg || &CurrBB == &Entry)
            break;
          WorkList.pop_front();
          WorkList.insert(WorkList.begin(), CurrBB.pred_begin(),
                          CurrBB.pred_end());
        }
      }
    }
    if (IsFatPtrInPhysReg || (Def && isDefinedAsFatPtr(*Def, *TII, MRI))) {
      BuildMI(MBB, MI, DL, get(EraVM::PTR_ADDrrs_s))
          .addReg(SrcReg, getKillRegState(isKill))
          .addReg(EraVM::R0)
          .addFrameIndex(FrameIndex)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
    } else {
      BuildMI(MBB, MI, DL, get(EraVM::ADDrrs_s))
          .addReg(SrcReg, getKillRegState(isKill))
          .addReg(EraVM::R0)
          .addFrameIndex(FrameIndex)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
    }
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
    // TODO: This code should go off once MVT::fatptr is properly supported
    // R1 needs special threatment because far calls produce fat pointer in R1.
    bool IsFatPtrInFrameIndex = false;
    auto findFrameIndexDef = [FrameIndex](const MachineInstr &Inst) {
      if (Inst.getOpcode() != EraVM::ADDrrs_s &&
          Inst.getOpcode() != EraVM::PTR_ADDrrs_s)
        return false;
      return Inst.getOperand(2).isFI() &&
             Inst.getOperand(2).getIndex() == FrameIndex &&
             Inst.getOperand(1).isReg() &&
             Inst.getOperand(1).getReg() == EraVM::R0 &&
             Inst.getOperand(3).isImm() && Inst.getOperand(3).getImm() == 32 &&
             Inst.getOperand(4).isImm() && Inst.getOperand(4).getImm() == 0 &&
             Inst.getOperand(5).isImm() && Inst.getOperand(5).getImm() == 0;
    };

    // Look for the closest definition in MBB.
    auto DefI = std::find_if(std::make_reverse_iterator(MI),
                             std::make_reverse_iterator(MBB.begin()),
                             findFrameIndexDef);
    if (DefI != std::make_reverse_iterator(MBB.begin()))
      IsFatPtrInFrameIndex = DefI->getOpcode() == EraVM::PTR_ADDrrs_s;
    // If not found check predecessors.
    // It's not expected to have a fatptr from one branch, and i256 from
    // another, so it's ok to find the very first definition using dfs. Loops
    // are an issue, so track visited BBs to not to continue ad infinum.
    if (!IsFatPtrInFrameIndex) {
      const MachineBasicBlock &Entry = MBB.getParent()->front();
      DenseSet<const MachineBasicBlock *> Visited{&MBB};
      std::deque<const MachineBasicBlock *> WorkList(MBB.pred_begin(),
                                                     MBB.pred_end());
      while (!WorkList.empty()) {
        const MachineBasicBlock &CurrBB = *WorkList.front();
        if (Visited.count(&CurrBB)) {
          WorkList.pop_front();
          continue;
        }
        Visited.insert(&CurrBB);
        // Look for the closest definition of R1.
        auto DefI =
            std::find_if(CurrBB.rbegin(), CurrBB.rend(), findFrameIndexDef);
        if (DefI != CurrBB.rend()) {
          IsFatPtrInFrameIndex = DefI->getOpcode() == EraVM::PTR_ADDrrs_s;
          break;
        }
        // TODO: If this code persist for a reson, handle calling conventions
        // here.
        if (IsFatPtrInFrameIndex || &CurrBB == &Entry)
          break;
        WorkList.pop_front();
        WorkList.insert(WorkList.begin(), CurrBB.pred_begin(),
                        CurrBB.pred_end());
      }
    }
    if (!IsFatPtrInFrameIndex)
      BuildMI(MBB, MI, DL, get(EraVM::ADDsrr_s))
          .addReg(DestReg, getDefRegState(true))
          .addFrameIndex(FrameIndex)
          .addImm(32)
          .addImm(0)
          .addImm(0)
          .addImm(0)
          .addMemOperand(MMO);
    else
      BuildMI(MBB, MI, DL, get(EraVM::PTR_ADDsrr_s))
          .addReg(DestReg, getDefRegState(true))
          .addFrameIndex(FrameIndex)
          .addImm(32)
          .addImm(0)
          .addImm(0)
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

bool EraVMInstrInfo::isFarCall(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case EraVM::FAR_CALL:
  case EraVM::FAR_CALLL:
  case EraVM::STATIC_CALL:
  case EraVM::STATIC_CALLL:
  case EraVM::DELEGATE_CALL:
  case EraVM::DELEGATE_CALLL:
  case EraVM::MIMIC_CALL:
  case EraVM::MIMIC_CALLL:
    return true;
  }
  return false;
}

bool EraVMInstrInfo::isAdd(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.starts_with("ADD");
}

bool EraVMInstrInfo::isSub(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.starts_with("SUB");
}

bool EraVMInstrInfo::isMul(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.starts_with("MUL");
}

bool EraVMInstrInfo::isDiv(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.starts_with("DIV");
}

bool EraVMInstrInfo::isPtr(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.starts_with("PTR");
}

bool EraVMInstrInfo::isNull(const MachineInstr &MI) const {
  return isAdd(MI) && hasRROperandAddressingMode(MI) && MI.getNumDefs() == 1U &&
         MI.getOperand(1).getReg() == EraVM::R0 &&
         MI.getOperand(2).getReg() == EraVM::R0;
}

bool EraVMInstrInfo::isSilent(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.ends_with("s");
}

EraVMInstrInfo::GenericInstruction
EraVMInstrInfo::genericInstructionFor(const MachineInstr &MI) const {
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

bool EraVMInstrInfo::hasRIOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return find(Mnemonic, 'i') != Mnemonic.end();
}

bool EraVMInstrInfo::hasRXOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return find(Mnemonic, 'x') != Mnemonic.end();
}

bool EraVMInstrInfo::hasRSOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  auto LCIt = find_if<StringRef, int(int)>(std::move(Mnemonic), std::islower);

  return LCIt != Mnemonic.end() && (*LCIt == 's' || *LCIt == 'y');
}

bool EraVMInstrInfo::hasRROperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  auto LCIt = find_if<StringRef, int(int)>(std::move(Mnemonic), std::islower);

  return LCIt != Mnemonic.end() && *LCIt == 'r' && *std::next(LCIt) == 'r';
}
