//===-- EVMStackShuffler.h - Implementation of stack shuffling ---*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares template algorithms to find optimal (cheapest) transition
// between two stack layouts using three shuffling primitives: `swap`, `dup`,
// and `pop`.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKSHUFFLER_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKSHUFFLER_H

#include "EVMControlFlowGraph.h"
#include "EVMHelperUtilities.h"
#include "llvm/CodeGen/MachineFunction.h"
#include <cassert>
#include <map>
#include <set>
#include <variant>

namespace llvm {

/*
template <class... Ts> struct Overload : Ts... {
using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

template <class T> static inline SmallVector<T> iota17(T begin, T end) {
SmallVector<T> R(end - begin);
std::iota(R.begin(), R.end(), begin);
return R;
}
*/
// Abstraction of stack shuffling operations. Can be defined as actual concept
// once we switch to C++20. Used as an interface for the stack shuffler below.
// The shuffle operation class is expected to internally keep track of a current
// stack layout (the "source layout") that the shuffler is supposed to shuffle
// to a fixed target stack layout. The shuffler works iteratively. At each
// iteration it instantiates an instance of the shuffle operations and queries
// it for various information about the current source stack layout and the
// target layout, as described in the interface below. Based on that information
// the shuffler decides which is the next optimal operation to perform on the
// stack and calls the corresponding entry point in the shuffling operations
// (swap, pushOrDupTarget or pop).
/*
template<typename ShuffleOperations>
concept ShuffleOperationConcept =
  requires(ShuffleOperations ops, size_t sourceOffset,
  size_t targetOffset, size_t depth) {

  // Returns true, iff the current slot at sourceOffset in source layout
  // is a suitable slot at targetOffset.
  { ops.isCompatible(sourceOffset, targetOffset) }
      -> std::convertible_to<bool>;

  // Returns true, iff the slots at the two given source offsets are identical.
  { ops.sourceIsSame(sourceOffset, sourceOffset) } ->
      std::convertible_to<bool>;

  // Returns a positive integer n, if the slot at the given source offset
  // needs n more copies. Returns a negative integer -n, if the slot at the
  // given source offsets occurs n times too many. Returns zero if the amount
  // of occurrences, in the current source layout, of the slot at the given
  // source offset matches the desired amount of occurrences in the target.
  { ops.sourceMultiplicity(sourceOffset) } -> std::convertible_to<int>;

  // Returns a positive integer n, if the slot at the given target offset
  // needs n more copies. Returns a negative integer -n, if the slot at the
  // given target offsets occurs n times too many. Returns zero if the amount
  // of occurrences, in the current source layout, of the slot at the given
  // target offset matches the desired amount of occurrences in the target.
  { ops.targetMultiplicity(targetOffset) } -> std::convertible_to<int>;

  // Returns true, iff any slot is compatible with the given target offset.
  { ops.targetIsArbitrary(targetOffset) } -> std::convertible_to<bool>;

  // Returns the number of slots in the source layout.
  { ops.sourceSize() } -> std::convertible_to<size_t>;

  // Returns the number of slots in the target layout.
  { ops.targetSize() } -> std::convertible_to<size_t>;

  // Swaps the top most slot in the source with the slot `depth` slots below
  // the top. In terms of EVM opcodes this is supposed to be a `SWAP<depth>`.
  // In terms of vectors this is supposed to be
  //`std::swap(source.at(source.size() - depth - 1, source.top))`.
  { ops.swap(depth) };

  // Pops the top most slot in the source, i.e. the slot at offset
  // ops.sourceSize() - 1. In terms of EVM opcodes this is `POP`.
  // In terms of vectors this is `source.pop();`.
  { ops.pop() };

  // Dups or pushes the slot that is supposed to end up at the given
  // target offset.
  { ops.pushOrDupTarget(targetOffset) };
};
*/

/// Helper class that can perform shuffling of a source stack layout to a target
/// stack layout via abstracted shuffle operations.
template </*ShuffleOperationConcept*/ typename ShuffleOperations>
class Shuffler {
public:
  /// Executes the stack shuffling operations. Instantiates an instance of
  /// ShuffleOperations in each iteration. Each iteration performs exactly one
  /// operation that modifies the stack. After `shuffle`, source and target have
  /// the same size and all slots in the source layout are compatible with the
  /// slots at the same target offset.
  template <typename... Args> static void shuffle(Args &&...args) {
    bool NeedsMoreShuffling = true;
    // The shuffling algorithm should always terminate in polynomial time, but
    // we provide a limit in case it does not terminate due to a bug.
    size_t IterationCount = 0;
    while (IterationCount < 1000 &&
           (NeedsMoreShuffling = shuffleStep(std::forward<Args>(args)...)))
      ++IterationCount;

    if (NeedsMoreShuffling)
      llvm_unreachable("Could not create stack layout after 1000 iterations.");
  }

private:
  // If dupping an ideal slot causes a slot that will still be required to
  // become unreachable, then dup the latter slot first.
  // Returns true, if it performed a dup.
  static bool dupDeepSlotIfRequired(ShuffleOperations &Ops) {
    // Check if the stack is large enough for anything to potentially become
    // unreachable.
    if (Ops.sourceSize() < 15)
      return false;
    // Check whether any deep slot might still be needed later (i.e. we still
    // need to reach it with a DUP or SWAP).
    for (size_t SourceOffset = 0; SourceOffset < (Ops.sourceSize() - 15);
         ++SourceOffset) {
      // This slot needs to be moved.
      if (!Ops.isCompatible(SourceOffset, SourceOffset)) {
        // If the current top fixes the slot, swap it down now.
        if (Ops.isCompatible(Ops.sourceSize() - 1, SourceOffset)) {
          Ops.swap(Ops.sourceSize() - SourceOffset - 1);
          return true;
        }
        // Bring up a slot to fix this now, if possible.
        if (bringUpTargetSlot(Ops, SourceOffset))
          return true;
        // Otherwise swap up the slot that will fix the offending slot.
        for (auto offset = SourceOffset + 1; offset < Ops.sourceSize();
             ++offset)
          if (Ops.isCompatible(offset, SourceOffset)) {
            Ops.swap(Ops.sourceSize() - offset - 1);
            return true;
          }
        // Otherwise give up - we will need stack compression or stack limit
        // evasion.
      }
      // We need another copy of this slot.
      else if (Ops.sourceMultiplicity(SourceOffset) > 0) {
        // If this slot occurs again later, we skip this occurrence.
        // TODO: use C++ 20 ranges::views::iota
        if (const auto &R = iota17<size_t>(SourceOffset + 1, Ops.sourceSize());
            std::any_of(R.begin(), R.end(), [&](size_t Offset) {
              return Ops.sourceIsSame(SourceOffset, Offset);
            }))
          continue;

        // Bring up the target slot that would otherwise become unreachable.
        for (size_t TargetOffset = 0; TargetOffset < Ops.targetSize();
             ++TargetOffset)
          if (!Ops.targetIsArbitrary(TargetOffset) &&
              Ops.isCompatible(SourceOffset, TargetOffset)) {
            Ops.pushOrDupTarget(TargetOffset);
            return true;
          }
      }
    }
    return false;
  }

  /// Finds a slot to dup or push with the aim of eventually fixing \p
  /// TargetOffset in the target. In the simplest case, the slot at \p
  /// TargetOffset has a multiplicity > 0, i.e. it can directly be dupped or
  /// pushed and the next iteration will fix \p TargetOffset. But, in general,
  /// there may already be enough copies of the slot that is supposed to end up
  /// at \p TargetOffset on stack, s.t. it cannot be dupped again. In that case
  /// there has to be a copy of the desired slot on stack already elsewhere that
  /// is not yet in place (`nextOffset` below). The fact that ``nextOffset`` is
  /// not in place means that we can (recursively) try bringing up the slot that
  /// is supposed to end up at ``nextOffset`` in the *target*. When the target
  /// slot at ``nextOffset`` is fixed, the current source slot at ``nextOffset``
  /// will be at the stack top, which is the slot required at \p TargetOffset.
  static bool bringUpTargetSlot(ShuffleOperations &Ops, size_t TargetOffset) {
    std::list<size_t> ToVisit{TargetOffset};
    std::set<size_t> Visited;

    while (!ToVisit.empty()) {
      size_t Offset = *ToVisit.begin();
      ToVisit.erase(ToVisit.begin());
      Visited.emplace(Offset);
      if (Ops.targetMultiplicity(Offset) > 0) {
        Ops.pushOrDupTarget(Offset);
        return true;
      }
      // There must be another slot we can dup/push that will lead to the target
      // slot at ``offset`` to be fixed.
      for (size_t NextOffset = 0;
           NextOffset < std::min(Ops.sourceSize(), Ops.targetSize());
           ++NextOffset)
        if (!Ops.isCompatible(NextOffset, NextOffset) &&
            Ops.isCompatible(NextOffset, Offset))
          if (!Visited.count(NextOffset))
            ToVisit.emplace_back(NextOffset);
    }
    return false;
  }

  /// Performs a single stack operation, transforming the source layout closer
  /// to the target layout.
  template <typename... Args> static bool shuffleStep(Args &&...args) {
    ShuffleOperations Ops{std::forward<Args>(args)...};

    // All source slots are final.
    if (const auto &R = iota17<size_t>(0u, Ops.sourceSize());
        std::all_of(R.begin(), R.end(), [&](size_t Index) {
          return Ops.isCompatible(Index, Index);
        })) {
      // Bring up all remaining target slots, if any, or terminate otherwise.
      if (Ops.sourceSize() < Ops.targetSize()) {
        if (!dupDeepSlotIfRequired(Ops))
          assert(bringUpTargetSlot(Ops, Ops.sourceSize()));
        return true;
      }
      return false;
    }

    size_t SourceTop = Ops.sourceSize() - 1;
    // If we no longer need the current stack top, we pop it, unless we need an
    // arbitrary slot at this position in the target.
    if (Ops.sourceMultiplicity(SourceTop) < 0 &&
        !Ops.targetIsArbitrary(SourceTop)) {
      Ops.pop();
      return true;
    }

    assert(Ops.targetSize() > 0);

    // If the top is not supposed to be exactly what is on top right now, try to
    // find a lower position to swap it to.
    if (!Ops.isCompatible(SourceTop, SourceTop) ||
        Ops.targetIsArbitrary(SourceTop))
      for (size_t Offset = 0;
           Offset < std::min(Ops.sourceSize(), Ops.targetSize()); ++Offset)
        // It makes sense to swap to a lower position, if
        if (!Ops.isCompatible(
                Offset, Offset) && // The lower slot is not already in position.
            !Ops.sourceIsSame(
                Offset, SourceTop) && // We would not just swap identical slots.
            Ops.isCompatible(
                SourceTop,
                Offset)) { // The lower position wants to have this slot.
          // We cannot swap that deep.
          if (Ops.sourceSize() - Offset - 1 > 16) {
            // If there is a reachable slot to be removed, park the current top
            // there.
            for (size_t SwapDepth = 16; SwapDepth > 0; --SwapDepth)
              if (Ops.sourceMultiplicity(Ops.sourceSize() - 1 - SwapDepth) <
                  0) {
                Ops.swap(SwapDepth);
                if (Ops.targetIsArbitrary(SourceTop))
                  // Usually we keep a slot that is to-be-removed, if the
                  // current top is arbitrary. However, since we are in a
                  // stack-too-deep situation, pop it immediately to compress
                  // the stack (we can always push back junk in the end).
                  Ops.pop();
                return true;
              }
            // TODO: otherwise we rely on stack compression or stack-to-memory.
          }
          Ops.swap(Ops.sourceSize() - Offset - 1);
          return true;
        }

    // Ops.sourceSize() > Ops.targetSize() cannot be true anymore, since if the
    // source top is no longer required, we already popped it, and if it is
    // required, we already swapped it down to a suitable target position.
    assert(Ops.sourceSize() <= Ops.targetSize());

    // If a lower slot should be removed, try to bring up the slot that should
    // end up there and bring it up. Note that after the cases above, there will
    // always be a target slot to duplicate in this case.
    for (size_t Offset = 0; Offset < Ops.sourceSize(); ++Offset)
      if (!Ops.isCompatible(
              Offset, Offset) && // The lower slot is not already in position.
          Ops.sourceMultiplicity(Offset) <
              0 && // We have too many copies of this slot.
          Offset <=
              Ops.targetSize() && // There is a target slot at this position.
          !Ops.targetIsArbitrary(
              Offset)) { // And that target slot is not arbitrary.
        if (!dupDeepSlotIfRequired(Ops))
          assert(bringUpTargetSlot(Ops, Offset));
        return true;
      }

    // At this point we want to keep all slots.
    for (size_t i = 0; i < Ops.sourceSize(); ++i)
      assert(Ops.sourceMultiplicity(i) >= 0);
    assert(Ops.sourceSize() <= Ops.targetSize());

    // If the top is not in position, try to find a slot that wants to be at the
    // top and swap it up.
    if (!Ops.isCompatible(SourceTop, SourceTop))
      for (size_t sourceOffset = 0; sourceOffset < Ops.sourceSize();
           ++sourceOffset)
        if (!Ops.isCompatible(sourceOffset, sourceOffset) &&
            Ops.isCompatible(sourceOffset, SourceTop)) {
          Ops.swap(Ops.sourceSize() - sourceOffset - 1);
          return true;
        }

    // If we still need more slots, produce a suitable one.
    if (Ops.sourceSize() < Ops.targetSize()) {
      if (!dupDeepSlotIfRequired(Ops))
        assert(bringUpTargetSlot(Ops, Ops.sourceSize()));
      return true;
    }

    // The stack has the correct size, each slot has the correct number of
    // copies and the top is in position.
    assert(Ops.sourceSize() == Ops.targetSize());
    size_t Size = Ops.sourceSize();
    for (size_t I = 0; I < Ops.sourceSize(); ++I)
      assert(Ops.sourceMultiplicity(I) == 0 &&
             (Ops.targetIsArbitrary(I) || Ops.targetMultiplicity(I) == 0));
    assert(Ops.isCompatible(SourceTop, SourceTop));

    const auto &SwappableOffsets = iota17(Size > 17 ? Size - 17 : 0u, Size);

    // If we find a lower slot that is out of position, but also compatible with
    // the top, swap that up.
    for (size_t Offset : SwappableOffsets)
      if (!Ops.isCompatible(Offset, Offset) &&
          Ops.isCompatible(SourceTop, Offset)) {
        Ops.swap(Size - Offset - 1);
        return true;
      }

    // Swap up any reachable slot that is still out of position.
    for (size_t Offset : SwappableOffsets)
      if (!Ops.isCompatible(Offset, Offset) &&
          !Ops.sourceIsSame(Offset, SourceTop)) {
        Ops.swap(Size - Offset - 1);
        return true;
      }

    // We are in a stack-too-deep situation and try to reduce the stack size.
    // If the current top is merely kept since the target slot is arbitrary, pop
    // it.
    if (Ops.targetIsArbitrary(SourceTop) &&
        Ops.sourceMultiplicity(SourceTop) <= 0) {
      Ops.pop();
      return true;
    }

    // If any reachable slot is merely kept, since the target slot is arbitrary,
    // swap it up and pop it.
    for (size_t Offset : SwappableOffsets)
      if (Ops.targetIsArbitrary(Offset) &&
          Ops.sourceMultiplicity(Offset) <= 0) {
        Ops.swap(Size - Offset - 1);
        Ops.pop();
        return true;
      }

    // We cannot avoid a stack-too-deep error. Repeat the above without
    // restricting to reachable slots.
    for (size_t Offset = 0; Offset < Size; ++Offset)
      if (!Ops.isCompatible(Offset, Offset) &&
          Ops.isCompatible(SourceTop, Offset)) {
        Ops.swap(Size - Offset - 1);
        return true;
      }

    for (size_t Offset = 0; Offset < Size; ++Offset)
      if (!Ops.isCompatible(Offset, Offset) &&
          !Ops.sourceIsSame(Offset, SourceTop)) {
        Ops.swap(Size - Offset - 1);
        return true;
      }

    llvm_unreachable("Unexpected state");
  }
};

/// A simple optimized map for mapping StackSlot to ints.
class Multiplicity {
public:
  int &operator[](StackSlot const &Slot) {
    if (auto *p = std::get_if<FunctionCallReturnLabelSlot>(&Slot))
      return FunctionCallReturnLabelSlotMultiplicity[*p];
    if (std::holds_alternative<FunctionReturnLabelSlot>(Slot))
      return FunctionReturnLabelSlotMultiplicity;
    if (auto *p = std::get_if<VariableSlot>(&Slot))
      return VariableSlotMultiplicity[*p];
    if (auto *p = std::get_if<LiteralSlot>(&Slot))
      return LiteralSlotMultiplicity[*p];
    if (auto *p = std::get_if<SymbolSlot>(&Slot))
      return SymbolSlotMultiplicity[*p];
    if (auto *p = std::get_if<TemporarySlot>(&Slot))
      return TemporarySlotMultiplicity[*p];

    assert(std::holds_alternative<JunkSlot>(Slot));
    return JunkSlotMultiplicity;
  }

  int at(StackSlot const &Slot) const {
    if (auto *p = std::get_if<FunctionCallReturnLabelSlot>(&Slot))
      return FunctionCallReturnLabelSlotMultiplicity.at(*p);
    if (std::holds_alternative<FunctionReturnLabelSlot>(Slot))
      return FunctionReturnLabelSlotMultiplicity;
    if (auto *p = std::get_if<VariableSlot>(&Slot))
      return VariableSlotMultiplicity.at(*p);
    if (auto *p = std::get_if<LiteralSlot>(&Slot))
      return LiteralSlotMultiplicity.at(*p);
    if (auto *p = std::get_if<SymbolSlot>(&Slot))
      return SymbolSlotMultiplicity.at(*p);
    if (auto *p = std::get_if<TemporarySlot>(&Slot))
      return TemporarySlotMultiplicity.at(*p);

    assert(std::holds_alternative<JunkSlot>(Slot));
    return JunkSlotMultiplicity;
  }

private:
  std::map<FunctionCallReturnLabelSlot, int>
      FunctionCallReturnLabelSlotMultiplicity;
  int FunctionReturnLabelSlotMultiplicity = 0;
  std::map<VariableSlot, int> VariableSlotMultiplicity;
  std::map<LiteralSlot, int> LiteralSlotMultiplicity;
  std::map<SymbolSlot, int> SymbolSlotMultiplicity;
  std::map<TemporarySlot, int> TemporarySlotMultiplicity;
  int JunkSlotMultiplicity = 0;
};

/// Transforms \p CurrentStack to \p TargetStack, invoking the provided
/// shuffling operations. Modifies `CurrentStack` itself after each invocation
/// of the shuffling operations.
/// \p Swap is a function with signature void(unsigned) that is called when the
/// top most slot is swapped with the slot `depth` slots below the top. In terms
/// of EVM opcodes this is supposed to be a `SWAP<depth>`.
/// \p PushOrDup is a function with signature void(StackSlot const&) that is
/// called to push or dup the slot given as its argument to the stack top.
/// \p Pop is a function with signature void() that is called when the top most
/// slot is popped.
template <typename SwapT, typename PushOrDupT, typename PopT>
void createStackLayout(Stack &CurrentStack, Stack const &TargetStack,
                       SwapT Swap, PushOrDupT PushOrDup, PopT Pop) {
  struct ShuffleOperations {
    Stack &currentStack;
    Stack const &targetStack;
    SwapT swapCallback;
    PushOrDupT pushOrDupCallback;
    PopT popCallback;
    Multiplicity multiplicity;

    ShuffleOperations(Stack &CurrentStack, Stack const &TargetStack, SwapT Swap,
                      PushOrDupT PushOrDup, PopT Pop)
        : currentStack(CurrentStack), targetStack(TargetStack),
          swapCallback(Swap), pushOrDupCallback(PushOrDup), popCallback(Pop) {
      for (auto const &slot : currentStack)
        --multiplicity[slot];

      for (unsigned Offset = 0; Offset < targetStack.size(); ++Offset) {
        auto &Slot = targetStack[Offset];
        if (std::holds_alternative<JunkSlot>(Slot) &&
            Offset < currentStack.size())
          ++multiplicity[currentStack.at(Offset)];
        else
          ++multiplicity[Slot];
      }
    }

    bool isCompatible(size_t Source, size_t Target) {
      return Source < currentStack.size() && Target < targetStack.size() &&
             (std::holds_alternative<JunkSlot>(targetStack.at(Target)) ||
              currentStack.at(Source) == targetStack.at(Target));
    }

    bool sourceIsSame(size_t Lhs, size_t Rhs) {
      return currentStack.at(Lhs) == currentStack.at(Rhs);
    }

    int sourceMultiplicity(size_t Offset) {
      return multiplicity.at(currentStack.at(Offset));
    }

    int targetMultiplicity(size_t Offset) {
      return multiplicity.at(targetStack.at(Offset));
    }

    bool targetIsArbitrary(size_t Offset) {
      return Offset < targetStack.size() &&
             std::holds_alternative<JunkSlot>(targetStack.at(Offset));
    }

    void swap(size_t I) {
      swapCallback(static_cast<unsigned>(I));
      std::swap(currentStack.at(currentStack.size() - I - 1),
                currentStack.back());
    }

    size_t sourceSize() { return currentStack.size(); }

    size_t targetSize() { return targetStack.size(); }

    void pop() {
      popCallback();
      currentStack.pop_back();
    }

    void pushOrDupTarget(size_t Offset) {
      auto const &targetSlot = targetStack.at(Offset);
      pushOrDupCallback(targetSlot);
      currentStack.push_back(targetSlot);
    }
  };

  Shuffler<ShuffleOperations>::shuffle(CurrentStack, TargetStack, Swap,
                                       PushOrDup, Pop);

  assert(CurrentStack.size() == TargetStack.size());
  for (unsigned I = 0; I < CurrentStack.size(); ++I) {
    auto &Current = CurrentStack[I];
    auto &Target = TargetStack[I];
    if (std::holds_alternative<JunkSlot>(Target))
      Current = JunkSlot{};
    else
      assert(Current == Target);
  }
}

} // end namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVMSTACKSHUFFLER_H
