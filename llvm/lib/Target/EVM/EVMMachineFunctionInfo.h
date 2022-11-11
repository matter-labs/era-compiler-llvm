//=------ EVMMachineFunctionInfo.h - EVM machine function info ----*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares EVM-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// This class is derived from MachineFunctionInfo and contains private
/// EVM-specific information for each MachineFunction.
class EVMFunctionInfo final : public MachineFunctionInfo {
public:
  explicit EVMFunctionInfo(MachineFunction &MF) {}
  EVMFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}
  ~EVMFunctionInfo() override;

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H
