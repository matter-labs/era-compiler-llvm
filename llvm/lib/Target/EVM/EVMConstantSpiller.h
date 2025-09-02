//===----- EVMConstantSpiller.h - Spill constants to memory ----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file identifies IMM_RELOAD instructions representing spilled constants
// throughout the module. It spills constants at the start of the entry function
// and replaces IMM_RELOAD with the corresponding reload instructions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMCONSTANTSPILLER_H
#define LLVM_LIB_TARGET_EVM_EVMCONSTANTSPILLER_H

// #include "llvm/IR/Module.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace llvm {

class MachineInstr;
class MachineFunction;

class EVMConstantSpiller {
public:
  explicit EVMConstantSpiller(SmallVector<MachineFunction *> &MFs);

  /// Inserts constant spills into the first basic block of the entry
  /// function and replaces IMM_RELOAD with the corresponding reload
  /// instructions at their use sites.
  void emitSpills(uint64_t SpillOffset, MachineFunction &EntryMF);

  /// Return the total size needed for the spill area.
  uint64_t getSpillSize() const;

private:
  /// Maps each APInt constant to the number of times it appears across all
  /// functions in the module
  SmallDenseMap<APInt, unsigned> ConstantToUseCount;

  /// IMM_RELOAD instructions that need to be converted into actuall reloads.
  SmallVector<MachineInstr *> Reloads;
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMCONSTANTSPILLER_H
