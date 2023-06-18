//===------------ EVMRegisterInfo.cpp - EVM Register Information ----------===//
//
// This file contains the EVM implementation of the
// TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "EVMFrameLowering.h"
#include "EVMInstrInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
using namespace llvm;

#define DEBUG_TYPE "evm-reg-info"

#define GET_REGINFO_TARGET_DESC
#include "EVMGenRegisterInfo.inc"

EVMRegisterInfo::EVMRegisterInfo() : EVMGenRegisterInfo(0) {}

const MCPhysReg *
EVMRegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

BitVector
EVMRegisterInfo::getReservedRegs(const MachineFunction & /*MF*/) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(EVM::SP);
  return Reserved;
}

void EVMRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  assert(SPAdj == 0);
  MachineInstr &MI = *II;

  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  int64_t FrameOffset = MFI.getObjectOffset(FrameIndex);
  const auto *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();

  assert(FrameOffset >= 0 && "FrameOffset < 0");
  assert(FrameOffset < static_cast<int64_t>(MFI.getStackSize()) &&
         "FrameOffset overflows stack size");
  assert(MFI.getObjectSize(FrameIndex) != 0 &&
         "We assume that variable-sized objects have already been lowered, "
         "and don't use FrameIndex operands.");

  Register FrameRegister = getFrameRegister(MF);
  Register FIRegOperand = FrameRegister;
  if (FrameOffset > 0) {
    FIRegOperand = MRI.createVirtualRegister(&EVM::GPRRegClass);
    Register OffsetReg = MRI.createVirtualRegister(&EVM::GPRRegClass);
    BuildMI(MBB, MI, II->getDebugLoc(), TII->get(EVM::CONST_I256), OffsetReg)
        .addImm(FrameOffset);
    BuildMI(MBB, MI, II->getDebugLoc(), TII->get(EVM::ADD), FIRegOperand)
        .addReg(FrameRegister)
        .addReg(OffsetReg);
  }

  MI.getOperand(FIOperandNum).ChangeToRegister(FIRegOperand, /*isDef=*/false);
}

Register EVMRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return EVM::SP;
}

const TargetRegisterClass *
EVMRegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                    unsigned Kind) const {
  return &EVM::GPRRegClass;
}
