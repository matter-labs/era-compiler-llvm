//===-------- EVMFrameLowering.cpp - EVM Frame Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
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
