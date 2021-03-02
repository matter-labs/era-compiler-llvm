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

SyncVMRegisterInfo::SyncVMRegisterInfo()
  : SyncVMGenRegisterInfo(SyncVM::PC) {}

const MCPhysReg*
SyncVMRegisterInfo::getCalleeSavedRegs() const {
  static const MCPhysReg CalleeSavedRegs[] = {
    SyncVM::R1,  SyncVM::R2,  SyncVM::R3,  SyncVM::R4,
    SyncVM::R5,  SyncVM::R6,  SyncVM::R7,  SyncVM::R8,
    SyncVM::R9,  SyncVM::R10, SyncVM::R11, SyncVM::R12,
  };

  return CalleeSavedRegs;
}

BitVector SyncVMRegisterInfo::getReservedRegs() const {
  BitVector Reserved(getNumRegs());

  // Mark 2 special registers as reserved.
  Reserved.set(SyncVM::PC);
  Reserved.set(SyncVM::SP);

  return Reserved;
}

const TargetRegisterClass *
SyncVMRegisterInfo::getPointerRegClass() const {
  return &SyncVM::GR256;
}

Register SyncVMRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return SyncVM::SP;
}
