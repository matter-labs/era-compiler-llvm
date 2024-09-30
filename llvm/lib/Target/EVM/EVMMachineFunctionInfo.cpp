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

EVMMachineFunctionInfo::~EVMMachineFunctionInfo() = default;

MachineFunctionInfo *EVMMachineFunctionInfo::clone(
    BumpPtrAllocator &Allocator, MachineFunction &DestMF,
    const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
    const {
  return DestMF.cloneInfo<EVMMachineFunctionInfo>(*this);
}

yaml::EVMMachineFunctionInfo::~EVMMachineFunctionInfo() = default;

yaml::EVMMachineFunctionInfo::EVMMachineFunctionInfo(
    const llvm::EVMMachineFunctionInfo &MFI)
    : IsStackified(MFI.getIsStackified()) {}

void yaml::EVMMachineFunctionInfo::mappingImpl(yaml::IO &YamlIO) {
  MappingTraits<EVMMachineFunctionInfo>::mapping(YamlIO, *this);
}

void EVMMachineFunctionInfo::initializeBaseYamlFields(
    const yaml::EVMMachineFunctionInfo &YamlMFI) {
  IsStackified = YamlMFI.IsStackified;
}
