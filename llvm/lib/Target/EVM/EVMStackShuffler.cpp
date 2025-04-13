//===---- EVMStackShuffler.cpp - Implementation of stack shuffling ---- C++*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMStackShuffler.h"
#include <deque>

using namespace llvm;

bool EVMStackShuffler::copyMissingSlotFromTarget(size_t StartRIdx,
                                                 bool CannotFail) {
  assert(StartRIdx < Target.size());
  std::deque<size_t> Worklist{StartRIdx};
  DenseSet<size_t> Visited;

  while (!Worklist.empty()) {
    size_t RIdx = Worklist.front();
    Worklist.pop_front();
    Visited.insert(RIdx);
    // If a slot is missing or underrepresented in the current stack, copy it
    // from the target.
    if (getTargetNumOccurrences(Target[RIdx]) > 0) {
      rematerialize(Target[RIdx]);
      return true;
    }
    // Otherwise, the target slot already exists in the current stack,
    // indicating that matching slots are present but not in the correct
    // positions. Continue searching among the corresponding target slots.
    assert(Current.size() <= Target.size());
    for (size_t NextRIdx = 0, Size = Current.size(); NextRIdx < Size;
         ++NextRIdx) {
      if (Visited.count(NextRIdx))
        continue;
      if (match(Current[NextRIdx], Target[NextRIdx]))
        continue;
      if (match(Current[NextRIdx], Target[RIdx]))
        Worklist.push_back(NextRIdx);
    }
  }
  if (CannotFail)
    report_fatal_error("Unexpected stack state.");

  return false;
}

bool EVMStackShuffler::rematerializeUnreachableSlot() {
  size_t Limit = StackDepthLimit - 1;
  size_t Size = Current.size();
  if (Size < Limit)
    return false;

  assert(Size <= Target.size());
  // Check whether any deep slot might still be needed later (i.e. we still
  // need to reach it with a DUP or SWAP).
  for (size_t RIdx = 0, REndIdx = Size - Limit; RIdx < REndIdx; ++RIdx) {
    if (match(Current[RIdx], Target[RIdx])) {
      if (getCurrentNumOccurrences(Current[RIdx]) > 0) {
        // If this slot occurs again later, we skip this occurrence.
        auto *It = std::find_if(
            Current.begin() + RIdx + 1, Current.end(),
            [&](const StackSlot *S) { return Current[RIdx] == S; });
        if (It != Current.end())
          continue;

        // Duplicate unreachable slot.
        for (const auto *TargetSlot : Target) {
          if (isa<UnusedSlot>(TargetSlot))
            continue;
          if (match(Current[RIdx], TargetSlot)) {
            rematerialize(TargetSlot);
            return true;
          }
        }
      }
      continue;
    }

    // This slot needs to be moved.
    // If the current top matches the slot, swap it down.
    if (match(Current[Size - 1], Target[RIdx])) {
      swap(Size - RIdx - 1);
      return true;
    }
    // Otherwise try to copy it on top from target.
    if (copyMissingSlotFromTarget(RIdx))
      return true;
    // Otherwise swap up with the first slot that matches the target one.
    if (swapIfCurrent(RIdx + 1, Size, [this, RIdx](const StackSlot *S) {
          return match(S, Target[RIdx]);
        }))
      return true;
  }
  return false;
}

bool EVMStackShuffler::step() {
  if (Current.size() <= Target.size() &&
      all_of(zip(Current, Target), [&](auto Slots) {
        return match(std::get<0>(Slots), std::get<1>(Slots));
      })) {
    if (Current.size() < Target.size()) {
      if (!rematerializeUnreachableSlot())
        // Since all current slots already match their target counterparts,
        // a deficit must exist.
        copyMissingSlotFromTarget(Current.size(), /* can't fail */ true);
      return true;
    }
    return false;
  }

  size_t Top = Current.size() - 1;
  const auto *SrcTop = Current[Top];
  const auto *TgtTop = Top < Target.size() ? Target[Top] : nullptr;

  if (getCurrentNumOccurrences(SrcTop) < 0 &&
      !isa_and_nonnull<UnusedSlot>(TgtTop)) {
    pop();
    return true;
  }

  assert(!Target.empty());
  // If the top slot is not on its position, swap it down to increase number of
  // matching slots.
  if (!match(SrcTop, TgtTop) || isa_and_nonnull<UnusedSlot>(TgtTop)) {
    for (size_t RIdx = 0, REndIdx = std::min(Current.size(), Target.size());
         RIdx < REndIdx; ++RIdx) {
      bool CanSwap = SrcTop != Current[RIdx] &&
                     !match(Current[RIdx], Target[RIdx]) &&
                     match(SrcTop, Target[RIdx]);
      if (!CanSwap)
        continue;

      if (Current.size() - RIdx - 1 > StackDepthLimit) {
        // If there is a reachable slot to be removed, swap it and remove.
        for (size_t SwapDepth = StackDepthLimit; SwapDepth > 0; --SwapDepth) {
          if (getCurrentNumOccurrences(
                  Current[Current.size() - 1 - SwapDepth]) < 0) {
            swap(SwapDepth);
            // Pop the redundant slot to compress the stack, even if the
            // target's top slot is unused and therefore match with anything.
            pop();
            return true;
          }
        }
        // We are about to create a swap with a too deep element.
        StackTooDeepErr = true;
      }
      swap(Current.size() - RIdx - 1);
      return true;
    }
  }

  // If the top of the current stack was not needed, it was popped;
  // if it was needed, it was swapped into the correct position.
  assert(Current.size() <= Target.size());

  // If a non-top slot is to be removed, try coping its corresponding target
  // slot so that a swap can occur and the slot can be popped in the next
  // iteration.
  for (size_t RIdx = 0, REndIdx = Current.size(); RIdx < REndIdx; ++RIdx) {
    if (match(Current[RIdx], Target[RIdx]))
      continue;

    if (getCurrentNumOccurrences(Current[RIdx]) < 0) {
      if (!rematerializeUnreachableSlot())
        // Given that the current stack is no larger than the target,
        // any excess implies a corresponding deficit, so copying is guaranteed.
        copyMissingSlotFromTarget(RIdx, /* can't fail */ true);
      return true;
    }
  }

  for ([[maybe_unused]] const auto *CurrentSlot : Current)
    assert(getCurrentNumOccurrences(CurrentSlot) >= 0);

  // Put the top at the right position.
  if (!match(SrcTop, TgtTop))
    if (swapIfCurrent(0, Current.size(),
                      [TgtTop, this](const StackSlot *SrcSlot) {
                        return match(SrcSlot, TgtTop);
                      }))
      return true;

  if (Current.size() < Target.size()) {
    if (!rematerializeUnreachableSlot())
      copyMissingSlotFromTarget(Current.size(), /* can't fail */ true);
    return true;
  }

  assert(Current.size() == Target.size());
  for (size_t RIdx = 0, REndIdx = Current.size(); RIdx < REndIdx; ++RIdx) {
    assert(getCurrentNumOccurrences(Current[RIdx]) == 0 &&
           "Current stack should have required number of slot copies.");
    assert((isa<UnusedSlot>(Target[RIdx]) ||
            getTargetNumOccurrences(Target[RIdx]) == 0) &&
           "Target stack should have required number of copies of used slots.");
  }
  assert(match(SrcTop, TgtTop) && "The top slot must match at this point.");

  size_t StartRIdx = Current.size() > (StackDepthLimit + 1)
                         ? Current.size() - (StackDepthLimit + 1)
                         : 0U;
  // If a lower slot that mismatches the target matches the top slot,
  // swap it so that it can be processed in the next iteration.
  if (swapIfTarget(StartRIdx, Current.size(),
                   [SrcTop, this](const StackSlot *TgtSlot) {
                     return match(SrcTop, TgtSlot);
                   }))
    return true;

  // Swap any mismatch reachable slot.
  if (swapIfCurrent(
          StartRIdx, Current.size(),
          [SrcTop](const StackSlot *SrcSlot) { return SrcTop != SrcSlot; }))
    return true;

  // Stack has unreachable slots, remove slots that match with unused.
  if (isa<UnusedSlot>(TgtTop)) {
    pop();
    return true;
  }
  for (size_t RIdx = 0, REndIdx = Current.size(); RIdx < REndIdx; ++RIdx) {
    if (isa<UnusedSlot>(Target[RIdx])) {
      swap(Current.size() - RIdx - 1);
      pop();
      return true;
    }
  }

  // Unreachable slots cannot be handled; equate stacks without enforcing the
  // depth limit.
  if (swapIfTarget(0, Current.size(), [SrcTop, this](const StackSlot *TgtSlot) {
        return match(SrcTop, TgtSlot);
      }))
    return true;

  if (swapIfCurrent(0, Current.size(), [SrcTop](const StackSlot *SrcSlot) {
        return SrcTop != SrcSlot;
      }))
    return true;

  llvm_unreachable("Unexpected state");
}

void llvm::calculateStack(
    Stack &CurrentStack, Stack const &TargetStack, unsigned StackDepthLimit,
    const std::function<void(unsigned)> &Swap,
    const std::function<void(const StackSlot *)> &Rematerialize,
    const std::function<void()> &Pop) {
  EVMStackShuffler TheShuffler =
      EVMStackShuffler(CurrentStack, TargetStack, StackDepthLimit);
  auto getNumOccurrences = [](const StackSlot *Slot, Stack &C, const Stack &T) {
    int CUses = -count(C, Slot);
    if (T.size() > C.size())
      CUses += std::count(T.begin() + C.size(), T.end(), Slot);
    CUses += count_if(zip(C, T), [Slot](auto In) {
      auto [CSlot, TSlot] = In;
      return isa<UnusedSlot>(TSlot) ? CSlot == Slot : TSlot == Slot;
    });
    return CUses;
  };

  TheShuffler.setGetCurrentNumOccurrences(getNumOccurrences);
  TheShuffler.setGetTargetNumOccurrences(getNumOccurrences);

  TheShuffler.setSwap([&Swap](size_t Idx, Stack &C) { Swap(Idx); });
  TheShuffler.setPop(Pop);
  TheShuffler.setRematerialize(Rematerialize);

  TheShuffler.shuffle();

  assert(CurrentStack.size() == TargetStack.size());
  for (unsigned I = 0; I < CurrentStack.size(); ++I) {
    const StackSlot *&Current = CurrentStack[I];
    const StackSlot *Target = TargetStack[I];
    if (isa<UnusedSlot>(Target))
      Current = EVMStackModel::getUnusedSlot();
    else
      assert(Current == Target);
  }
}
