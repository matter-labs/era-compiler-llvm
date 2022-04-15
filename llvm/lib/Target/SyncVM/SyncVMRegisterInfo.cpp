//===-- SyncVMRegisterInfo.cpp - SyncVM Register Information --------------===//
//
// This file contains the SyncVM implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "SyncVMRegisterInfo.h"
#include "SyncVM.h"
#include "SyncVMMachineFunctionInfo.h"
#include "SyncVMTargetMachine.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-reg-info"

#define GET_REGINFO_TARGET_DESC
#include "SyncVMGenRegisterInfo.inc"

SyncVMRegisterInfo::SyncVMRegisterInfo() : SyncVMGenRegisterInfo(0) {}

const MCPhysReg *
SyncVMRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

BitVector SyncVMRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(SyncVM::SP);
  Reserved.set(SyncVM::Flags);
  Reserved.set(SyncVM::R0);
  return Reserved;
}

const TargetRegisterClass *
SyncVMRegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                       unsigned Kind) const {
  return nullptr;
}

Register SyncVMRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return 0;
}

void SyncVMRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                             int SPAdj, unsigned FIOperandNum,
                                             RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected");

  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  DebugLoc DL = MI.getDebugLoc();
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

  auto BasePtr = SyncVM::SP;
  int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex);
  Offset -= MF.getFrameInfo().getStackSize();

  if (MI.getOpcode() == SyncVM::ADDframe) {
    auto SPInst = BuildMI(MBB, II, DL, TII.get(SyncVM::CTXr_se))
                      .add(MI.getOperand(0))
                      .addImm(SyncVMCTX::SP)
                      .addImm(SyncVMCC::COND_NONE)
                      .getInstr();
    if (Offset < 0)
      SPInst = BuildMI(MBB, II, DL, TII.get(SyncVM::SUBxrr_s))
                   .addDef(MI.getOperand(0).getReg())
                   .addImm(- Offset /32)
                   .add(SPInst->getOperand(0))
                   .addImm(SyncVMCC::COND_NONE)
                   .getInstr();
    else
      SPInst = BuildMI(MBB, II, DL, TII.get(SyncVM::ADDirr_s))
                   .addDef(MI.getOperand(0).getReg())
                   .addImm(Offset /32)
                   .add(SPInst->getOperand(0))
                   .addImm(SyncVMCC::COND_NONE)
                   .getInstr();
    BuildMI(MBB, II, DL, TII.get(SyncVM::MULirrr_s))
        .addDef(MI.getOperand(0).getReg())
        .addDef(SyncVM::R0)
        .addImm(32)
        .add(SPInst->getOperand(0))
        .addImm(SyncVMCC::COND_NONE);
    MI.eraseFromParent();
    return;
  }


  // Fold imm into offset
  Offset /= 32;
  Offset += MI.getOperand(FIOperandNum + 2).getImm();

  MI.getOperand(FIOperandNum).ChangeToRegister(BasePtr, false);
  MI.getOperand(FIOperandNum + 2).ChangeToImmediate(Offset);
}
