#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKHELPERS_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKHELPERS_H

#include "EVMControlFlowGraph.h"
#include "llvm/CodeGen/MachineFunction.h"
#include <cassert>
#include <map>
#include <numeric>
#include <set>
#include <variant>

namespace llvm {

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

template <class T> static inline SmallVector<T> iota17(T Begin, T End) {
  SmallVector<T> R(End - Begin);
  std::iota(R.begin(), R.end(), Begin);
  return R;
}

StringRef getInstName(const MachineInstr *MI);
const Function *getCalledFunction(const MachineInstr &MI);
std::string stackSlotToString(const StackSlot &Slot);
std::string stackToString(Stack const &S);

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
concept ShuffleOperationConcept = requires(ShuffleOperations ops, size_t
sourceOffset, size_t targetOffset, size_t depth) {
        // Returns true, iff the current slot at sourceOffset in source layout
is a suitable slot at targetOffset. { ops.isCompatible(sourceOffset,
targetOffset) } -> std::convertible_to<bool>;
        // Returns true, iff the slots at the two given source offsets are
identical. { ops.sourceIsSame(sourceOffset, sourceOffset) } ->
std::convertible_to<bool>;
        // Returns a positive integer n, if the slot at the given source offset
needs n more copies.
        // Returns a negative integer -n, if the slot at the given source
offsets occurs n times too many.
        // Returns zero if the amount of occurrences, in the current source
layout, of the slot at the given source offset
        // matches the desired amount of occurrences in the target.
        { ops.sourceMultiplicity(sourceOffset) } -> std::convertible_to<int>;
        // Returns a positive integer n, if the slot at the given target offset
needs n more copies.
        // Returns a negative integer -n, if the slot at the given target
offsets occurs n times too many.
        // Returns zero if the amount of occurrences, in the current source
layout, of the slot at the given target offset
        // matches the desired amount of occurrences in the target.
        { ops.targetMultiplicity(targetOffset) } -> std::convertible_to<int>;
        // Returns true, iff any slot is compatible with the given target
offset. { ops.targetIsArbitrary(targetOffset) } -> std::convertible_to<bool>;
        // Returns the number of slots in the source layout.
        { ops.sourceSize() } -> std::convertible_to<size_t>;
        // Returns the number of slots in the target layout.
        { ops.targetSize() } -> std::convertible_to<size_t>;
        // Swaps the top most slot in the source with the slot `depth` slots
below the top.
        // In terms of EVM opcodes this is supposed to be a `SWAP<depth>`.
        // In terms of vectors this is supposed to be
`std::swap(source.at(source.size() - depth - 1, source.top))`. { ops.swap(depth)
};
        // Pops the top most slot in the source, i.e. the slot at offset
ops.sourceSize() - 1.
        // In terms of EVM opcodes this is `POP`.
        // In terms of vectors this is `source.pop();`.
        { ops.pop() };
        // Dups or pushes the slot that is supposed to end up at the given
target offset. { ops.pushOrDupTarget(targetOffset) };
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
    assert(!NeedsMoreShuffling &&
           "Could not create stack layout after 1000 iterations.");
  }

private:
  // If dupping an ideal slot causes a slot that will still be required to
  // become unreachable, then dup the latter slot first.
  // @returns true, if it performed a dup.
  static bool dupDeepSlotIfRequired(ShuffleOperations &Ops) {
    // Check if the stack is large enough for anything to potentially become
    // unreachable.
    if (Ops.sourceSize() < 15)
      return false;
    // Check whether any deep slot might still be needed later (i.e. we still
    // need to reach it with a DUP or SWAP).
    for (size_t sourceOffset = 0; sourceOffset < (Ops.sourceSize() - 15);
         ++sourceOffset) {
      // This slot needs to be moved.
      if (!Ops.isCompatible(sourceOffset, sourceOffset)) {
        // If the current top fixes the slot, swap it down now.
        if (Ops.isCompatible(Ops.sourceSize() - 1, sourceOffset)) {
          Ops.swap(Ops.sourceSize() - sourceOffset - 1);
          return true;
        }
        // Bring up a slot to fix this now, if possible.
        if (bringUpTargetSlot(Ops, sourceOffset))
          return true;
        // Otherwise swap up the slot that will fix the offending slot.
        for (auto offset = sourceOffset + 1; offset < Ops.sourceSize();
             ++offset)
          if (Ops.isCompatible(offset, sourceOffset)) {
            Ops.swap(Ops.sourceSize() - offset - 1);
            return true;
          }
        // Otherwise give up - we will need stack compression or stack limit
        // evasion.
      }
      // We need another copy of this slot.
      else if (Ops.sourceMultiplicity(sourceOffset) > 0) {
        // If this slot occurs again later, we skip this occurrence.
        // TODO: use C++ 20 ranges::views::iota
        if (const auto &R = iota17<size_t>(sourceOffset + 1, Ops.sourceSize());
            std::any_of(R.begin(), R.end(), [&](size_t Offset) {
              return Ops.sourceIsSame(sourceOffset, Offset);
            }))
          continue;

        // Bring up the target slot that would otherwise become unreachable.
        for (size_t targetOffset = 0; targetOffset < Ops.targetSize();
             ++targetOffset)
          if (!Ops.targetIsArbitrary(targetOffset) &&
              Ops.isCompatible(sourceOffset, targetOffset)) {
            Ops.pushOrDupTarget(targetOffset);
            return true;
          }
      }
    }
    return false;
  }

  /// Finds a slot to dup or push with the aim of eventually fixing @a
  /// TargetOffset in the target. In the simplest case, the slot at @a
  /// TargetOffset has a multiplicity > 0, i.e. it can directly be dupped or
  /// pushed and the next iteration will fix @a TargetOffset. But, in general,
  /// there may already be enough copies of the slot that is supposed to end up
  /// at @a TargetOffset on stack, s.t. it cannot be dupped again. In that case
  /// there has to be a copy of the desired slot on stack already elsewhere that
  /// is not yet in place (`nextOffset` below). The fact that ``nextOffset`` is
  /// not in place means that we can (recursively) try bringing up the slot that
  /// is supposed to end up at ``nextOffset`` in the *target*. When the target
  /// slot at ``nextOffset`` is fixed, the current source slot at ``nextOffset``
  /// will be at the stack top, which is the slot required at @a TargetOffset.
  static bool bringUpTargetSlot(ShuffleOperations &Ops, size_t TargetOffset) {
    std::list<size_t> toVisit{TargetOffset};
    std::set<size_t> visited;

    while (!toVisit.empty()) {
      auto offset = *toVisit.begin();
      toVisit.erase(toVisit.begin());
      visited.emplace(offset);
      if (Ops.targetMultiplicity(offset) > 0) {
        Ops.pushOrDupTarget(offset);
        return true;
      }
      // There must be another slot we can dup/push that will lead to the target
      // slot at ``offset`` to be fixed.
      for (size_t nextOffset = 0;
           nextOffset < std::min(Ops.sourceSize(), Ops.targetSize());
           ++nextOffset)
        if (!Ops.isCompatible(nextOffset, nextOffset) &&
            Ops.isCompatible(nextOffset, offset))
          if (!visited.count(nextOffset))
            toVisit.emplace_back(nextOffset);
    }
    return false;
  }

  /// Performs a single stack operation, transforming the source layout closer
  /// to the target layout.
  template <typename... Args> static bool shuffleStep(Args &&...args) {
    ShuffleOperations ops{std::forward<Args>(args)...};

    // All source slots are final.
    if (const auto &R = iota17<size_t>(0u, ops.sourceSize());
        std::all_of(R.begin(), R.end(), [&](size_t _index) {
          return ops.isCompatible(_index, _index);
        })) {
      // Bring up all remaining target slots, if any, or terminate otherwise.
      if (ops.sourceSize() < ops.targetSize()) {
        if (!dupDeepSlotIfRequired(ops))
          assert(bringUpTargetSlot(ops, ops.sourceSize()));
        return true;
      }
      return false;
    }

    size_t sourceTop = ops.sourceSize() - 1;
    // If we no longer need the current stack top, we pop it, unless we need an
    // arbitrary slot at this position in the target.
    if (ops.sourceMultiplicity(sourceTop) < 0 &&
        !ops.targetIsArbitrary(sourceTop)) {
      ops.pop();
      return true;
    }

    assert(ops.targetSize() > 0);

    // If the top is not supposed to be exactly what is on top right now, try to
    // find a lower position to swap it to.
    if (!ops.isCompatible(sourceTop, sourceTop) ||
        ops.targetIsArbitrary(sourceTop))
      for (size_t offset = 0;
           offset < std::min(ops.sourceSize(), ops.targetSize()); ++offset)
        // It makes sense to swap to a lower position, if
        if (!ops.isCompatible(
                offset, offset) && // The lower slot is not already in position.
            !ops.sourceIsSame(
                offset, sourceTop) && // We would not just swap identical slots.
            ops.isCompatible(
                sourceTop,
                offset)) { // The lower position wants to have this slot.
          // We cannot swap that deep.
          if (ops.sourceSize() - offset - 1 > 16) {
            // If there is a reachable slot to be removed, park the current top
            // there.
            for (size_t swapDepth = 16; swapDepth > 0; --swapDepth)
              if (ops.sourceMultiplicity(ops.sourceSize() - 1 - swapDepth) <
                  0) {
                ops.swap(swapDepth);
                if (ops.targetIsArbitrary(sourceTop))
                  // Usually we keep a slot that is to-be-removed, if the
                  // current top is arbitrary. However, since we are in a
                  // stack-too-deep situation, pop it immediately to compress
                  // the stack (we can always push back junk in the end).
                  ops.pop();
                return true;
              }
            // Otherwise we rely on stack compression or stack-to-memory.
          }
          ops.swap(ops.sourceSize() - offset - 1);
          return true;
        }

    // ops.sourceSize() > ops.targetSize() cannot be true anymore, since if the
    // source top is no longer required, we already popped it, and if it is
    // required, we already swapped it down to a suitable target position.
    assert(ops.sourceSize() <= ops.targetSize());

    // If a lower slot should be removed, try to bring up the slot that should
    // end up there and bring it up. Note that after the cases above, there will
    // always be a target slot to duplicate in this case.
    for (size_t offset = 0; offset < ops.sourceSize(); ++offset)
      if (!ops.isCompatible(
              offset, offset) && // The lower slot is not already in position.
          ops.sourceMultiplicity(offset) <
              0 && // We have too many copies of this slot.
          offset <=
              ops.targetSize() && // There is a target slot at this position.
          !ops.targetIsArbitrary(
              offset)) { // And that target slot is not arbitrary.
        if (!dupDeepSlotIfRequired(ops))
          assert(bringUpTargetSlot(ops, offset));
        return true;
      }

    // At this point we want to keep all slots.
    for (size_t i = 0; i < ops.sourceSize(); ++i)
      assert(ops.sourceMultiplicity(i) >= 0);
    assert(ops.sourceSize() <= ops.targetSize());

    // If the top is not in position, try to find a slot that wants to be at the
    // top and swap it up.
    if (!ops.isCompatible(sourceTop, sourceTop))
      for (size_t sourceOffset = 0; sourceOffset < ops.sourceSize();
           ++sourceOffset)
        if (!ops.isCompatible(sourceOffset, sourceOffset) &&
            ops.isCompatible(sourceOffset, sourceTop)) {
          ops.swap(ops.sourceSize() - sourceOffset - 1);
          return true;
        }

    // If we still need more slots, produce a suitable one.
    if (ops.sourceSize() < ops.targetSize()) {
      if (!dupDeepSlotIfRequired(ops))
        assert(bringUpTargetSlot(ops, ops.sourceSize()));
      return true;
    }

    // The stack has the correct size, each slot has the correct number of
    // copies and the top is in position.
    assert(ops.sourceSize() == ops.targetSize());
    size_t size = ops.sourceSize();
    for (size_t i = 0; i < ops.sourceSize(); ++i)
      assert(ops.sourceMultiplicity(i) == 0 &&
             (ops.targetIsArbitrary(i) || ops.targetMultiplicity(i) == 0));
    assert(ops.isCompatible(sourceTop, sourceTop));

    const auto &swappableOffsets = iota17(size > 17 ? size - 17 : 0u, size);

    // If we find a lower slot that is out of position, but also compatible with
    // the top, swap that up.
    for (size_t offset : swappableOffsets)
      if (!ops.isCompatible(offset, offset) &&
          ops.isCompatible(sourceTop, offset)) {
        ops.swap(size - offset - 1);
        return true;
      }

    // Swap up any reachable slot that is still out of position.
    for (size_t offset : swappableOffsets)
      if (!ops.isCompatible(offset, offset) &&
          !ops.sourceIsSame(offset, sourceTop)) {
        ops.swap(size - offset - 1);
        return true;
      }

    // We are in a stack-too-deep situation and try to reduce the stack size.
    // If the current top is merely kept since the target slot is arbitrary, pop
    // it.
    if (ops.targetIsArbitrary(sourceTop) &&
        ops.sourceMultiplicity(sourceTop) <= 0) {
      ops.pop();
      return true;
    }

    // If any reachable slot is merely kept, since the target slot is arbitrary,
    // swap it up and pop it.
    for (size_t offset : swappableOffsets)
      if (ops.targetIsArbitrary(offset) &&
          ops.sourceMultiplicity(offset) <= 0) {
        ops.swap(size - offset - 1);
        ops.pop();
        return true;
      }

    // We cannot avoid a stack-too-deep error. Repeat the above without
    // restricting to reachable slots.
    for (size_t offset = 0; offset < size; ++offset)
      if (!ops.isCompatible(offset, offset) &&
          ops.isCompatible(sourceTop, offset)) {
        ops.swap(size - offset - 1);
        return true;
      }

    for (size_t offset = 0; offset < size; ++offset)
      if (!ops.isCompatible(offset, offset) &&
          !ops.sourceIsSame(offset, sourceTop)) {
        ops.swap(size - offset - 1);
        return true;
      }

    llvm_unreachable("Unexpected state");
  }
};

/// A simple optimized map for mapping StackSlots to ints.
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
  std::map<TemporarySlot, int> TemporarySlotMultiplicity;
  int JunkSlotMultiplicity = 0;
};

/// Transforms @a CurrentStack to @a TargetStack, invoking the provided
/// shuffling operations. Modifies @a CurrentStack itself after each invocation
/// of the shuffling operations.
/// @a Swap is a function with signature void(unsigned) that is called when the
/// top most slot is swapped with the slot `depth` slots below the top. In terms
/// of EVM opcodes this is supposed to be a `SWAP<depth>`.
/// @a PushOrDup is a function with signature void(StackSlot const&) that is
/// called to push or dup the slot given as its argument to the stack top.
/// @a Pop is a function with signature void() that is called when the top most
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

      for (unsigned offset = 0; offset < targetStack.size(); ++offset) {
        auto &slot = targetStack[offset];
        if (std::holds_alternative<JunkSlot>(slot) &&
            offset < currentStack.size())
          ++multiplicity[currentStack.at(offset)];
        else
          ++multiplicity[slot];
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

    bool targetIsArbitrary(size_t offset) {
      return offset < targetStack.size() &&
             std::holds_alternative<JunkSlot>(targetStack.at(offset));
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
  for (unsigned Idx = 0; Idx < CurrentStack.size(); ++Idx) {
    auto &current = CurrentStack[Idx];
    auto &target = TargetStack[Idx];
    if (std::holds_alternative<JunkSlot>(target))
      current = JunkSlot{};
    else
      assert(current == target);
  }
}

} // end namespace llvm
  //
#endif // LLVM_LIB_TARGET_EVM_EVMSTACKHELPERS_H
