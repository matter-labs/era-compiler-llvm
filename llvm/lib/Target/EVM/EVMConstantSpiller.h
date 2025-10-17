//===----- EVMConstantSpiller.h - Spill constants to memory ----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file identifies frequently used large constants across the module.
// If a constant’s usage exceeds the threshold and its bit-width is sufficient,
// it is spilled at the beginning of the __entry function and reloaded at use
// sites.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMCONSTANTSPILLER_H
#define LLVM_LIB_TARGET_EVM_EVMCONSTANTSPILLER_H

#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"

namespace llvm {

class MachineInstr;

class EVMConstantSpiller {
public:
  EVMConstantSpiller(Module &M, MachineModuleInfo &MMI);

  /// Return the total size needed for the spill area.
  uint64_t getSpillSize() const;

  /// Insert constant spills into the first basic block of the __entry
  /// function and insert reloads at their use sites.
  void emitConstantSpills(uint64_t SpillOffset, MachineFunction &EntryMF);

private:
  /// Maps each APInt constant to the number of times it appears across all
  /// functions in the module
  SmallDenseMap<APInt, unsigned> ConstantUseCount;
  /// PUSH instructions that need to be reloaded
  SmallVector<MachineInstr *> ReloadCandidates;

  /// Filters out constants whose usage count is below the threshold.
  void filterCandidates();

  void analyzeModule(Module &M, MachineModuleInfo &MMI);
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMCONSTANTSPILLER_H
