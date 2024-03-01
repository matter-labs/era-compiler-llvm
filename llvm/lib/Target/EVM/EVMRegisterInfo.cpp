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
  static const std::array<MCPhysReg, 1> CalleeSavedRegs = {0};
  return CalleeSavedRegs.data();
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
  const int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const int64_t FrameOffset = MFI.getObjectOffset(FrameIndex);

  assert(FrameOffset >= 0 && "FrameOffset < 0");
  assert(FrameOffset < static_cast<int64_t>(MFI.getStackSize()) &&
         "FrameOffset overflows stack size");
  assert(MFI.getObjectSize(FrameIndex) != 0 &&
         "We assume that variable-sized objects have already been lowered, "
         "and don't use FrameIndex operands.");

  if (FrameOffset > 0) {
    MachineOperand &OffsetMO = MI.getOperand(FIOperandNum + 1);
    const ConstantInt *ConstVal = OffsetMO.getCImm();
    APInt Offset = ConstVal->getValue();
    Offset += FrameOffset;
    assert((Offset.getZExtValue() % 32) == 0);
    OffsetMO.setCImm(ConstantInt::get(ConstVal->getContext(), Offset));
  }
  MI.getOperand(FIOperandNum)
      .ChangeToRegister(getFrameRegister(MF),
                        /*isDef=*/false);
}

Register EVMRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return EVM::SP;
}

const TargetRegisterClass *
EVMRegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                    unsigned Kind) const {
  return &EVM::GPRRegClass;
}
