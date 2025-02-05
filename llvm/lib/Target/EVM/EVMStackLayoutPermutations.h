//===---- EVMStackLayoutPermutations.h - Stack layout permute ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a set of methods for manipulating and optimizing the stack
// layout.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTPERMUTATIONS_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTPERMUTATIONS_H

#include "EVMStackModel.h"

namespace llvm {

class EVMStackLayoutPermutations {
public:
  /// Returns the number of operations required to transform stack \p Source to
  /// \p Target.
  static size_t evaluateStackTransform(Stack Source, const Stack &Target);

  /// Returns the ideal stack to have before executing the MachineInstr \p MI
  /// s.t. shuffling to \p Post is cheap (excluding the input of the operation
  /// itself). If \p RematerializeSlot returns true for a slot, this slot
  /// should not occur in the ideal stack, but rather be generated on the fly
  /// during shuffling.
  static Stack createIdealLayout(
      const SmallVector<StackSlot *> &OpDefs, const Stack &Post,
      const std::function<bool(const StackSlot *)> &RematerializeSlot);

  /// Calculates the ideal stack layout, s.t., both \p Stack1 and \p Stack2 can
  /// be achieved with minimal stack shuffling when starting from the returned
  /// layout.
  static Stack combineStack(const Stack &Stack1, const Stack &Stack2);

  /// Returns true if there is stack too deep error when shuffling \p Source
  /// to \p Target.
  static bool hasStackTooDeep(const Stack &Source, const Stack &Target);

  /// Transforms \p CurrentStack to \p TargetStack, invoking the provided
  /// shuffling operations. Modifies `CurrentStack` itself after each invocation
  /// of the shuffling operations.
  /// \p Swap is a function that is called when the top most slot is swapped
  /// with the slot `depth` slots below the top. In terms of EVM opcodes this
  /// is supposed to be a `SWAP<depth>`.
  /// \p PushOrDup is a function that is called to push or dup the slot given
  /// as its argument to the stack top.
  /// \p Pop is a function that is called when the top most slot is popped.
  static void
  createStackLayout(Stack &CurrentStack, const Stack &TargetStack,
                    const std::function<void(unsigned)> &Swap,
                    const std::function<void(const StackSlot *)> &PushOrDup,
                    const std::function<void()> &Pop);
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTPERMUTATIONS_H
