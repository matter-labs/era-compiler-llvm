//===-- EraVMRegisterInfo.cpp - EraVM Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EraVM implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMRegisterInfo.h"
#include "EraVMTargetMachine.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-reg-info"

#define GET_REGINFO_TARGET_DESC
#include "EraVMGenRegisterInfo.inc"

EraVMRegisterInfo::EraVMRegisterInfo() : EraVMGenRegisterInfo(0) {}

const MCPhysReg *
EraVMRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

BitVector EraVMRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(EraVM::SP);
  Reserved.set(EraVM::Flags);
  Reserved.set(EraVM::R0);
  return Reserved;
}

bool EraVMRegisterInfo::isConstantPhysReg(MCRegister PhysReg) const {
  return PhysReg == EraVM::R0;
}

const TargetRegisterClass *
EraVMRegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                      unsigned Kind) const {
  return nullptr;
}

Register EraVMRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return 0;
}

void EraVMRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected");

  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  DebugLoc DL = MI.getDebugLoc();
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

  auto BasePtr = EraVM::SP;
  int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex);
  Offset -= MF.getFrameInfo().getStackSize();

  if (MI.getOpcode() == EraVM::FRAMEirrr) {
    BuildMI(MBB, II, DL, TII.get(EraVM::ADDrrr_s))
        .addDef(MI.getOperand(0).getReg())
        .add(MI.getOperand(FIOperandNum + 2))
        .add(MI.getOperand(FIOperandNum + 1))
        .addImm(EraVMCC::COND_NONE);
    assert(Offset < 0 && "On EraVM, offset cannot be positive");
    auto Sub = BuildMI(MBB, II, DL, TII.get(EraVM::SUBxrr_s))
                   .addDef(MI.getOperand(0).getReg())
                   .addImm(-Offset / 32)
                   .addReg(MI.getOperand(0).getReg(), RegState::Kill)
                   .addImm(EraVMCC::COND_NONE)
                   .getInstr();

    // Set that immediate represents stack slot index.
    Sub->getOperand(1).setTargetFlags(EraVMII::MO_STACK_SLOT_IDX);
    MI.eraseFromParent();
    return;
  }

  if (MI.getOpcode() == EraVM::ADDframe) {
    auto SPInst = BuildMI(MBB, II, DL, TII.get(EraVM::CTXr_se))
                      .addDef(MI.getOperand(0).getReg())
                      .addImm(EraVMCTX::SP)
                      .addImm(EraVMCC::COND_NONE)
                      .getInstr();
    assert(Offset < 0 && "On EraVM, offset cannot be positive");
    SPInst = BuildMI(MBB, II, DL, TII.get(EraVM::SUBxrr_s))
                 .addDef(MI.getOperand(0).getReg())
                 .addImm(-Offset / 32)
                 .addReg(SPInst->getOperand(0).getReg())
                 .addImm(EraVMCC::COND_NONE)
                 .getInstr();

    // Set that immediate represents stack slot index.
    SPInst->getOperand(1).setTargetFlags(EraVMII::MO_STACK_SLOT_IDX);

    BuildMI(MBB, II, DL, TII.get(EraVM::MULirrr_s))
        .addDef(MI.getOperand(0).getReg())
        .addDef(EraVM::R0)
        .addImm(32)
        .addReg(SPInst->getOperand(0).getReg())
        .addImm(EraVMCC::COND_NONE);
    MI.eraseFromParent();
    return;
  }

  // Fold imm into offset
  Offset /= 32;
  Offset += MI.getOperand(FIOperandNum + 2).getImm();

  MI.getOperand(FIOperandNum).ChangeToRegister(BasePtr, false);
  MI.getOperand(FIOperandNum + 2).ChangeToImmediate(Offset);
}
