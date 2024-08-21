//=--------- EVMMachineFunctionInfo.cpp - EVM Machine Function Info ---------=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements EVM-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#include "EVMMachineFunctionInfo.h"
using namespace llvm;

EVMFunctionInfo::~EVMFunctionInfo() = default; // anchor.

MachineFunctionInfo *
EVMFunctionInfo::clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
                       const DenseMap<MachineBasicBlock *, MachineBasicBlock *>
                           &Src2DstMBB) const {
  return DestMF.cloneInfo<EVMFunctionInfo>(*this);
}
