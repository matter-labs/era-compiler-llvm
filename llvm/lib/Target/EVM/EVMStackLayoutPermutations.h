//===---- EVMStackLayoutGenerator.h - Stack layout generator ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the stack layout generator which for each operation
// finds complete stack layout that:
//   - has the slots required for the operation at the stack top.
//   - will have the operation result in a layout that makes it easy to achieve
//     the next desired layout.
// It also finds an entering/exiting stack layout for each block.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTPERMUTATIONS_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTPERMUTATIONS_H

#include "EVMStackModel.h"
#include "llvm/ADT/DenseMap.h"

#include <deque>
#include <functional>

namespace llvm {

class EVMStackLayoutPermutations {
public:
  struct StackTooDeep {
    /// Number of slots that need to be saved.
    size_t deficit = 0;
    /// Set of variables, eliminating which would decrease the stack deficit.
    SmallVector<Register> variableChoices;
  };

  /// Returns the number of operations required to transform stack \p Source to
  /// \p Target.
  static size_t evaluateStackTransform(Stack Source, Stack const &Target);

  /// Returns the ideal stack to have before executing an operation that outputs
  /// \p OperationOutput, s.t. shuffling to \p Post is cheap (excluding the
  /// input of the operation itself). If \p GenerateSlotOnTheFly returns true
  /// for a slot, this slot should not occur in the ideal stack, but rather be
  /// generated on the fly during shuffling.
  static Stack
  createIdealLayout(const Stack &OperationOutput, const Stack &Post,
                    std::function<bool(const StackSlot *)> RematerializeSlot);

  /// Calculates the ideal stack layout, s.t., both \p Stack1 and \p Stack2 can
  /// be achieved with minimal stack shuffling when starting from the returned
  /// layout.
  static Stack combineStack(const Stack &Stack1, const Stack &Stack2);

  /// Walks through the CFG and reports any stack too deep errors that would
  /// occur when generating code for it without countermeasures.
  static SmallVector<StackTooDeep> findStackTooDeep(Stack const &Source,
                                                    Stack const &Target);

  /// Returns a copy of \p Stack stripped of all duplicates and slots that can
  /// be freely generated. Attempts to create a layout that requires a minimal
  /// amount of operations to reconstruct the original stack \p Stack.
  static Stack compressStack(Stack Stack);
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTPERMUTATIONS_H
