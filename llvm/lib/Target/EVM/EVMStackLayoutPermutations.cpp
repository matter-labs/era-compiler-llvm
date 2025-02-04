//===---- EVMStackLayoutPermutations.cpp - Stack layout permute -*- C++ -*-===//
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

#include "EVMStackLayoutPermutations.h"
#include <deque>

using namespace llvm;

namespace {

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

/// Return the number of hops from the beginning of the \p RangeOrContainer
/// to the \p Item. If no \p Item is found in the \p RangeOrContainer,
/// std::nullopt is returned.
template <typename T, typename V>
std::optional<size_t> offset(T &&RangeOrContainer, V &&Item) {
  auto It = find(RangeOrContainer, Item);
  return (It == adl_end(RangeOrContainer))
             ? std::nullopt
             : std::optional(std::distance(adl_begin(RangeOrContainer), It));
}

/// Helper class that can perform shuffling of a source stack layout to a target
/// stack layout via abstracted shuffle operations.
template <typename ShuffleOperations> class Shuffler {
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
    if (Ops.sourceSize() < (Ops.stackDepthLimit() - 1))
      return false;
    // Check whether any deep slot might still be needed later (i.e. we still
    // need to reach it with a DUP or SWAP).
    for (size_t SourceOffset = 0;
         SourceOffset < (Ops.sourceSize() - (Ops.stackDepthLimit() - 1));
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
        if (const auto &R =
                llvm::seq<size_t>(SourceOffset + 1, Ops.sourceSize());
            any_of(R, [&](size_t Offset) {
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
    std::deque<size_t> ToVisit{TargetOffset};
    DenseSet<size_t> Visited;

    while (!ToVisit.empty()) {
      size_t Offset = *ToVisit.begin();
      ToVisit.erase(ToVisit.begin());
      Visited.insert(Offset);
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
    if (const auto &R = llvm::seq<size_t>(0U, Ops.sourceSize()); all_of(
            R, [&](size_t Index) { return Ops.isCompatible(Index, Index); })) {
      // Bring up all remaining target slots, if any, or terminate otherwise.
      if (Ops.sourceSize() < Ops.targetSize()) {
        if (!dupDeepSlotIfRequired(Ops)) {
          [[maybe_unused]] bool Res = bringUpTargetSlot(Ops, Ops.sourceSize());
          assert(Res);
        }
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
          if (Ops.sourceSize() - Offset - 1 > Ops.stackDepthLimit()) {
            // If there is a reachable slot to be removed, park the current top
            // there.
            for (size_t SwapDepth = Ops.stackDepthLimit(); SwapDepth > 0;
                 --SwapDepth)
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
        if (!dupDeepSlotIfRequired(Ops)) {
          [[maybe_unused]] bool Res = bringUpTargetSlot(Ops, Offset);
          assert(Res);
        }
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
      if (!dupDeepSlotIfRequired(Ops)) {
        [[maybe_unused]] bool Res = bringUpTargetSlot(Ops, Ops.sourceSize());
        assert(Res);
      }
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

    const auto &SwappableOffsets = llvm::seq<size_t>(
        Size > (Ops.stackDepthLimit() + 1) ? Size - (Ops.stackDepthLimit() + 1)
                                           : 0u,
        Size);

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
  int &operator[](const StackSlot *Slot) {
    if (const auto *p = dyn_cast<FunctionCallReturnLabelSlot>(Slot))
      return FunctionCallReturnLabelSlotMultiplicity[p];
    if (isa<FunctionReturnLabelSlot>(Slot))
      return FunctionReturnLabelSlotMultiplicity;
    if (const auto *p = dyn_cast<RegisterSlot>(Slot))
      return RegisterSlotMultiplicity[p];
    if (const auto *p = dyn_cast<LiteralSlot>(Slot))
      return LiteralSlotMultiplicity[p];
    if (const auto *p = dyn_cast<SymbolSlot>(Slot))
      return SymbolSlotMultiplicity[p];

    assert(isa<JunkSlot>(Slot));
    return JunkSlotMultiplicity;
  }

  int at(const StackSlot *Slot) const {
    if (const auto *p = dyn_cast<FunctionCallReturnLabelSlot>(Slot))
      return FunctionCallReturnLabelSlotMultiplicity.at(p);
    if (isa<FunctionReturnLabelSlot>(Slot))
      return FunctionReturnLabelSlotMultiplicity;
    if (const auto *p = dyn_cast<RegisterSlot>(Slot))
      return RegisterSlotMultiplicity.at(p);
    if (const auto *p = dyn_cast<LiteralSlot>(Slot))
      return LiteralSlotMultiplicity.at(p);
    if (const auto *p = dyn_cast<SymbolSlot>(Slot))
      return SymbolSlotMultiplicity.at(p);

    assert(isa<JunkSlot>(Slot));
    return JunkSlotMultiplicity;
  }

private:
  DenseMap<const FunctionCallReturnLabelSlot *, int>
      FunctionCallReturnLabelSlotMultiplicity;
  int FunctionReturnLabelSlotMultiplicity = 0;
  DenseMap<const RegisterSlot *, int> RegisterSlotMultiplicity;
  DenseMap<const LiteralSlot *, int> LiteralSlotMultiplicity;
  DenseMap<const SymbolSlot *, int> SymbolSlotMultiplicity;
  int JunkSlotMultiplicity = 0;
};

/// Returns a copy of \p Stack stripped of all duplicates and slots that can
/// be freely generated. Attempts to create a layout that requires a minimal
/// amount of operations to reconstruct the original stack \p Stack.
Stack compressStack(Stack CurStack, unsigned StackDepthLimit) {
  std::optional<size_t> FirstDupOffset;
  do {
    if (FirstDupOffset) {
      if (*FirstDupOffset != (CurStack.size() - 1))
        std::swap(CurStack[*FirstDupOffset], CurStack.back());
      CurStack.pop_back();
      FirstDupOffset.reset();
    }

    auto I = CurStack.rbegin(), E = CurStack.rend();
    for (size_t Depth = 0; I < E; ++I, ++Depth) {
      StackSlot *Slot = *I;
      if (Slot->isRematerializable()) {
        FirstDupOffset = CurStack.size() - Depth - 1;
        break;
      }

      if (auto DupDepth =
              offset(drop_begin(reverse(CurStack), Depth + 1), Slot)) {
        if (Depth + *DupDepth <= StackDepthLimit) {
          FirstDupOffset = CurStack.size() - Depth - 1;
          break;
        }
      }
    }
  } while (FirstDupOffset);
  return CurStack;
}
} // namespace

void EVMStackLayoutPermutations::createStackLayout(
    Stack &CurrentStack, const Stack &TargetStack, unsigned StackDepthLimit,
    const std::function<void(unsigned)> &Swap,
    const std::function<void(const StackSlot *)> &PushOrDup,
    const std::function<void()> &Pop) {
  struct ShuffleOperations {
    Stack &currentStack;
    const Stack &targetStack;
    const std::function<void(unsigned)> &swapCallback;
    const std::function<void(const StackSlot *)> &pushOrDupCallback;
    const std::function<void()> &popCallback;
    Multiplicity multiplicity;
    unsigned StackDepthLimit;

    ShuffleOperations(Stack &CurrentStack, const Stack &TargetStack,
                      unsigned StackDepthLimit,
                      const std::function<void(unsigned)> &Swap,
                      const std::function<void(const StackSlot *)> &PushOrDup,
                      const std::function<void()> &Pop)
        : currentStack(CurrentStack), targetStack(TargetStack),
          swapCallback(Swap), pushOrDupCallback(PushOrDup), popCallback(Pop),
          StackDepthLimit(StackDepthLimit) {
      for (const auto &slot : currentStack)
        --multiplicity[slot];

      for (unsigned Offset = 0; Offset < targetStack.size(); ++Offset) {
        auto *Slot = targetStack[Offset];
        if (isa<JunkSlot>(Slot) && Offset < currentStack.size())
          ++multiplicity[currentStack[Offset]];
        else
          ++multiplicity[Slot];
      }
    }

    bool isCompatible(size_t Source, size_t Target) {
      return Source < currentStack.size() && Target < targetStack.size() &&
             (isa<JunkSlot>(targetStack[Target]) ||
              currentStack[Source] == targetStack[Target]);
    }

    bool sourceIsSame(size_t Lhs, size_t Rhs) {
      return currentStack[Lhs] == currentStack[Rhs];
    }

    int sourceMultiplicity(size_t Offset) {
      return multiplicity.at(currentStack[Offset]);
    }

    int targetMultiplicity(size_t Offset) {
      return multiplicity.at(targetStack[Offset]);
    }

    bool targetIsArbitrary(size_t Offset) {
      return Offset < targetStack.size() && isa<JunkSlot>(targetStack[Offset]);
    }

    void swap(size_t I) {
      swapCallback(static_cast<unsigned>(I));
      std::swap(currentStack[currentStack.size() - I - 1], currentStack.back());
    }

    size_t sourceSize() { return currentStack.size(); }

    size_t targetSize() { return targetStack.size(); }

    void pop() {
      popCallback();
      currentStack.pop_back();
    }

    void pushOrDupTarget(size_t Offset) {
      auto *targetSlot = targetStack[Offset];
      pushOrDupCallback(targetSlot);
      currentStack.push_back(targetSlot);
    }

    unsigned stackDepthLimit() const { return StackDepthLimit; }
  };

  Shuffler<ShuffleOperations>::shuffle(CurrentStack, TargetStack,
                                       StackDepthLimit, Swap, PushOrDup, Pop);

  assert(CurrentStack.size() == TargetStack.size());
  for (unsigned I = 0; I < CurrentStack.size(); ++I) {
    StackSlot *&Current = CurrentStack[I];
    auto *Target = TargetStack[I];
    if (isa<JunkSlot>(Target))
      Current = EVMStackModel::getJunkSlot();
    else
      assert(Current == Target);
  }
}

size_t EVMStackLayoutPermutations::evaluateStackTransform(
    Stack Source, const Stack &Target, unsigned StackDepthLimit) {
  size_t OpGas = 0;
  auto Swap = [&](unsigned SwapDepth) {
    if (SwapDepth > StackDepthLimit)
      OpGas += 1000;
    else
      OpGas += 3; // SWAP* gas price;
  };

  auto DupOrPush = [&](const StackSlot *Slot) {
    if (Slot->isRematerializable())
      OpGas += 3;
    else {
      auto Depth = offset(reverse(Source), Slot);
      if (!Depth)
        llvm_unreachable("No slot in the stack");

      if (*Depth < StackDepthLimit)
        OpGas += 3; // DUP* gas price
      else
        OpGas += 1000;
    }
  };
  auto Pop = [&]() { OpGas += 2; };

  createStackLayout(Source, Target, StackDepthLimit, Swap, DupOrPush, Pop);
  return OpGas;
}

bool EVMStackLayoutPermutations::hasStackTooDeep(const Stack &Source,
                                                 const Stack &Target,
                                                 unsigned StackDepthLimit) {
  Stack CurrentStack = Source;
  bool HasError = false;
  createStackLayout(
      CurrentStack, Target, StackDepthLimit,
      [&](unsigned I) {
        if (I > StackDepthLimit)
          HasError = true;
      },
      [&](const StackSlot *Slot) {
        if (Slot->isRematerializable())
          return;

        if (auto Depth = offset(reverse(CurrentStack), Slot);
            Depth && *Depth >= StackDepthLimit)
          HasError = true;
      },
      [&]() {});
  return HasError;
}

Stack EVMStackLayoutPermutations::createIdealLayout(
    const SmallVector<StackSlot *> &OpDefs, const Stack &Post,
    unsigned StackDepthLimit,
    const std::function<bool(const StackSlot *)> &RematerializeSlot) {
  struct PreviousSlot {
    size_t slot;
  };
  using LayoutT = SmallVector<std::variant<PreviousSlot, StackSlot *>>;

  // Determine the number of slots that have to be on stack before executing the
  // operation (excluding the inputs of the operation itself). That is slots
  // that should not be generated on the fly and are not outputs of the
  // operation.
  size_t PreOperationLayoutSize = Post.size();
  for (const auto *Slot : Post)
    if (is_contained(OpDefs, Slot) || RematerializeSlot(Slot))
      --PreOperationLayoutSize;

  // The symbolic layout directly after the operation has the form
  // PreviousSlot{0}, ..., PreviousSlot{n}, [output<0>], ..., [output<m>]
  LayoutT Layout;
  for (size_t Index = 0; Index < PreOperationLayoutSize; ++Index)
    Layout.emplace_back(PreviousSlot{Index});
  append_range(Layout, OpDefs);

  // Shortcut for trivial case.
  if (Layout.empty())
    return Stack{};

  // Next we will shuffle the layout to the Post stack using ShuffleOperations
  // that are aware of PreviousSlot's.
  struct ShuffleOperations {
    LayoutT &Layout;
    const Stack &Post;
    DenseSet<StackSlot *> Outputs;
    Multiplicity Mult;
    const std::function<bool(const StackSlot *)> &RematerializeSlot;
    unsigned StackDepthLimit;
    ShuffleOperations(
        LayoutT &Layout, const Stack &Post, unsigned StackDepthLimit,
        const std::function<bool(const StackSlot *)> &RematerializeSlot)
        : Layout(Layout), Post(Post), RematerializeSlot(RematerializeSlot),
          StackDepthLimit(StackDepthLimit) {
      for (const auto &LayoutSlot : Layout)
        if (const auto *Slot = std::get_if<StackSlot *>(&LayoutSlot))
          Outputs.insert(*Slot);

      for (const auto &LayoutSlot : Layout)
        if (const auto *Slot = std::get_if<StackSlot *>(&LayoutSlot))
          --Mult[*Slot];

      for (auto *Slot : Post)
        if (Outputs.count(Slot) || RematerializeSlot(Slot))
          ++Mult[Slot];
    }

    bool isCompatible(size_t Source, size_t Target) {
      return Source < Layout.size() && Target < Post.size() &&
             (isa<JunkSlot>(Post[Target]) ||
              std::visit(Overload{[&](const PreviousSlot &) {
                                    return !Outputs.count(Post[Target]) &&
                                           !RematerializeSlot(Post[Target]);
                                  },
                                  [&](const StackSlot *S) {
                                    return S == Post[Target];
                                  }},
                         Layout[Source]));
    }

    bool sourceIsSame(size_t Lhs, size_t Rhs) {
      if (std::holds_alternative<PreviousSlot>(Layout[Lhs]) &&
          std::holds_alternative<PreviousSlot>(Layout[Rhs]))
        return true;

      auto *SlotLHS = std::get_if<StackSlot *>(&Layout[Lhs]);
      auto *SlotRHS = std::get_if<StackSlot *>(&Layout[Rhs]);
      return SlotLHS && SlotRHS && *SlotLHS == *SlotRHS;
    }

    int sourceMultiplicity(size_t Offset) {
      return std::visit(
          Overload{[&](const PreviousSlot &) { return 0; },
                   [&](const StackSlot *S) { return Mult.at(S); }},
          Layout[Offset]);
    }

    int targetMultiplicity(size_t Offset) {
      if (!Outputs.count(Post[Offset]) && !RematerializeSlot(Post[Offset]))
        return 0;
      return Mult.at(Post[Offset]);
    }

    bool targetIsArbitrary(size_t Offset) {
      return Offset < Post.size() && isa<JunkSlot>(Post[Offset]);
    }

    void swap(size_t I) {
      assert(!std::holds_alternative<PreviousSlot>(
                 Layout[Layout.size() - I - 1]) ||
             !std::holds_alternative<PreviousSlot>(Layout.back()));
      std::swap(Layout[Layout.size() - I - 1], Layout.back());
    }

    size_t sourceSize() { return Layout.size(); }

    size_t targetSize() { return Post.size(); }

    void pop() { Layout.pop_back(); }

    void pushOrDupTarget(size_t Offset) { Layout.push_back(Post[Offset]); }

    unsigned stackDepthLimit() const { return StackDepthLimit; }
  };

  Shuffler<ShuffleOperations>::shuffle(Layout, Post, StackDepthLimit,
                                       RematerializeSlot);

  // Now we can construct the ideal layout before the operation.
  // "layout" has shuffled the PreviousSlot{x} to new places using minimal
  // operations to move the operation output in place. The resulting permutation
  // of the PreviousSlot yields the ideal positions of slots before the
  // operation, i.e. if PreviousSlot{2} is at a position at which Post contains
  // VariableSlot{"tmp"}, then we want the variable tmp in the slot at offset 2
  // in the layout before the operation.
  assert(Layout.size() == Post.size());
  SmallVector<StackSlot *> IdealLayout(Post.size(), nullptr);
  for (unsigned Idx = 0; Idx < std::min(Layout.size(), Post.size()); ++Idx) {
    auto *Slot = Post[Idx];
    auto &IdealPosition = Layout[Idx];
    if (auto *PrevSlot = std::get_if<PreviousSlot>(&IdealPosition))
      IdealLayout[PrevSlot->slot] = Slot;
  }

  // The tail of layout must have contained the operation outputs and will not
  // have been assigned slots in the last loop.
  while (!IdealLayout.empty() && !IdealLayout.back())
    IdealLayout.pop_back();

  assert(IdealLayout.size() == PreOperationLayoutSize);

  Stack Result;
  for (auto *Item : IdealLayout) {
    assert(Item);
    Result.push_back(Item);
  }

  return Result;
}

Stack EVMStackLayoutPermutations::combineStack(const Stack &Stack1,
                                               const Stack &Stack2,
                                               unsigned StackDepthLimit) {
  // TODO: it would be nicer to replace this by a constructive algorithm.
  // Currently it uses a reduced version of the Heap Algorithm to partly
  // brute-force, which seems to work decently well.

  Stack CommonPrefix;
  for (unsigned Idx = 0; Idx < std::min(Stack1.size(), Stack2.size()); ++Idx) {
    StackSlot *Slot1 = Stack1[Idx];
    const StackSlot *Slot2 = Stack2[Idx];
    if (Slot1 != Slot2)
      break;
    CommonPrefix.push_back(Slot1);
  }

  Stack Stack1Tail, Stack2Tail;
  for (auto *Slot : drop_begin(Stack1, CommonPrefix.size()))
    Stack1Tail.push_back(Slot);

  for (auto *Slot : drop_begin(Stack2, CommonPrefix.size()))
    Stack2Tail.push_back(Slot);

  if (Stack1Tail.empty()) {
    CommonPrefix.append(compressStack(Stack2Tail, StackDepthLimit));
    return CommonPrefix;
  }

  if (Stack2Tail.empty()) {
    CommonPrefix.append(compressStack(Stack1Tail, StackDepthLimit));
    return CommonPrefix;
  }

  Stack Candidate;
  for (auto *Slot : Stack1Tail)
    if (!is_contained(Candidate, Slot))
      Candidate.push_back(Slot);

  for (auto *Slot : Stack2Tail)
    if (!is_contained(Candidate, Slot))
      Candidate.push_back(Slot);

  erase_if(Candidate, [](const StackSlot *Slot) {
    return isa<LiteralSlot>(Slot) || isa<SymbolSlot>(Slot) ||
           isa<FunctionCallReturnLabelSlot>(Slot);
  });

  auto evaluate = [&](const Stack &Candidate) -> size_t {
    size_t NumOps = 0;
    Stack TestStack = Candidate;
    auto Swap = [&](unsigned SwapDepth) {
      ++NumOps;
      if (SwapDepth > StackDepthLimit)
        NumOps += 1000;
    };

    auto DupOrPush = [&](const StackSlot *Slot) {
      if (Slot->isRematerializable())
        return;

      Stack Tmp = CommonPrefix;
      Tmp.append(TestStack);
      auto Depth = offset(reverse(Tmp), Slot);
      if (Depth && *Depth >= StackDepthLimit)
        NumOps += 1000;
    };
    createStackLayout(TestStack, Stack1Tail, StackDepthLimit, Swap, DupOrPush,
                      [&]() {});
    TestStack = Candidate;
    createStackLayout(TestStack, Stack2Tail, StackDepthLimit, Swap, DupOrPush,
                      [&]() {});
    return NumOps;
  };

  // See https://en.wikipedia.org/wiki/Heap's_algorithm
  size_t N = Candidate.size();
  Stack BestCandidate = Candidate;
  size_t BestCost = evaluate(Candidate);
  SmallVector<size_t> C(N, 0);
  size_t I = 1;
  while (I < N) {
    if (C[I] < I) {
      if (I & 1)
        std::swap(Candidate.front(), Candidate[I]);
      else
        std::swap(Candidate[C[I]], Candidate[I]);

      size_t Cost = evaluate(Candidate);
      if (Cost < BestCost) {
        BestCost = Cost;
        BestCandidate = Candidate;
      }
      ++C[I];
      // Note that for a proper implementation of the Heap algorithm this would
      // need to revert back to 'I = 1'. However, the incorrect implementation
      // produces decent result and the proper version would have N! complexity
      // and is thereby not feasible.
      ++I;
    } else {
      C[I] = 0;
      ++I;
    }
  }

  CommonPrefix.append(BestCandidate);
  return CommonPrefix;
}
