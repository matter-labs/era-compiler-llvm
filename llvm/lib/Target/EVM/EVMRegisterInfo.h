//===----- EVMRegisterInfo.h - EVM Register Information Impl -*- C++ -*---====//
//
// This file contains the EVM implementation of the
// EVMRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMREGISTERINFO_H
#define LLVM_LIB_TARGET_EVM_EVMREGISTERINFO_H

#define GET_REGINFO_HEADER
#include "EVMGenRegisterInfo.inc"

namespace llvm {

class MachineFunction;
class RegScavenger;

class EVMRegisterInfo final : public EVMGenRegisterInfo {
public:
  explicit EVMRegisterInfo();

  // Code Generation virtual methods.
  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  BitVector getReservedRegs(const MachineFunction &MF) const override;
  void eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;

  const TargetRegisterClass *
  getPointerRegClass(const MachineFunction &MF,
                     unsigned Kind = 0) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMREGISTERINFO_H
