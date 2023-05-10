//===-------- EVMFrameLowering.cpp - EVM Frame Information ----------------===//
//
// This file contains the EVM implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "EVMFrameLowering.h"

using namespace llvm;

bool EVMFrameLowering::hasFP(const MachineFunction &MF) const { return false; }

void EVMFrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {}

void EVMFrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {}
