//===------------ EVMRegisterInfo.cpp - EVM Register Information ----------===//
//
// This file contains the EVM implementation of the
// TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "EVMFrameLowering.h"
#include "EVMInstrInfo.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
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
  llvm_unreachable("Subrotines are not supported yet");
}

Register EVMRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return 0;
}

const TargetRegisterClass *
EVMRegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                    unsigned Kind) const {
  return &EVM::GPRRegClass;
}
