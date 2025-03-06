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

bool EVMStackShuffler::bringUpTargetSlot(size_t StartRIdx, bool CannotFail) {
  std::deque<size_t> Worklist{StartRIdx};
  DenseSet<size_t> Visited;

  while (!Worklist.empty()) {
    size_t RIdx = Worklist.front();
    Worklist.pop_front();
    Visited.insert(RIdx);
    if (getTargetNumOccurrences(Target[RIdx]) > 0) {
      rematerialize(Target[RIdx]);
      return true;
    }
    // There must be another slot we can dup/push that will lead to the target
    // slot at RIdx to be fixed.
    assert(Current.size() <= Target.size());
    for (size_t NextRIdx = 0, Size = Current.size(); NextRIdx < Size;
         ++NextRIdx) {
      if (Visited.count(NextRIdx))
        continue;
      if (match(Current[NextRIdx], Target[NextRIdx]))
        continue;
      if (RIdx < Target.size() && match(Current[NextRIdx], Target[RIdx]))
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
    // The slot is in place, but we might need another copy if it.
    if (match(Current[RIdx], Target[RIdx])) {
      if (getCurrentNumOccurrences(Current[RIdx]) > 0) {
        // If this slot occurs again later, we skip this occurrence.
        auto It = std::find_if(
            Current.begin() + RIdx + 1, Current.end(),
            [&](const StackSlot *S) { return Current[RIdx] == S; });
        if (It != Current.end())
          continue;

        // Duplicate unreachable slot.
        for (size_t TgtRIdx = 0; TgtRIdx < Target.size(); ++TgtRIdx) {
          if (isa<UnusedSlot>(Target[TgtRIdx]))
            continue;
          if (match(Current[RIdx], Target[TgtRIdx])) {
            rematerialize(Target[TgtRIdx]);
            return true;
          }
        }
      }
      continue;
    }

    // This slot needs to be moved.
    // If the current top fixes the slot, swap it down now.
    if (match(Current[Size - 1], Target[RIdx])) {
      swap(Size - RIdx - 1);
      return true;
    }
    // Bring up a slot to fix this now, if possible.
    if (bringUpTargetSlot(RIdx))
      return true;
    // Otherwise swap up the slot that will fix the offending slot.
    if (swapIfCurrent(RIdx + 1, Size, [&](const StackSlot *S) {
          return match(S, Target[RIdx]);
        }))
      return true;
    // Otherwise give up - we will need stack compression or stack limit
    // evasion.
  }
  return false;
}

bool EVMStackShuffler::step() {
  // All source slots are final.
  if (Current.size() <= Target.size() &&
      all_of(zip(Current, Target),
             [&](std::pair<const StackSlot *, const StackSlot *> Slots) {
               return match(Slots.first, Slots.second);
             })) {
    // Bring up all remaining target slots, if any, or terminate otherwise.
    if (Current.size() < Target.size()) {
      if (!rematerializeUnreachableSlot())
        bringUpTargetSlot(Current.size(), /* can't fail */ true);
      return true;
    }
    return false;
  }

  size_t Top = Current.size() - 1;
  const auto *SrcTop = Current[Top];
  const auto *TgtTop = Top < Target.size() ? Target[Top] : nullptr;

  // If we no longer need the current stack top, we pop it, unless we need an
  // arbitrary slot at this position in the target.
  if (getCurrentNumOccurrences(SrcTop) < 0 &&
      !isa_and_nonnull<UnusedSlot>(TgtTop)) {
    pop();
    return true;
  }

  assert(Target.size() > 0);
  // If the top is not supposed to be exactly what is on top right now, try to
  // find a lower position to swap it to.
  if (!match(SrcTop, TgtTop) || isa_and_nonnull<UnusedSlot>(TgtTop)) {
    for (size_t RIdx = 0, REndIdx = std::min(Current.size(), Target.size());
         RIdx < REndIdx; ++RIdx) {
      // It makes sense to swap to a lower position, if 1) the slots are not
      // the same, 2) the lower slot is not already in position, 3) the lower
      // position wants to have this slot.
      bool CanSwap = SrcTop != Current[RIdx] &&
                     !match(Current[RIdx], Target[RIdx]) &&
                     match(SrcTop, Target[RIdx]);
      if (!CanSwap)
        continue;

      // We cannot swap that deep.
      if (Current.size() - RIdx - 1 > StackDepthLimit) {
        // If there is a reachable slot to be removed, park the current top
        // there.
        for (size_t SwapDepth = StackDepthLimit; SwapDepth > 0; --SwapDepth) {
          if (getCurrentNumOccurrences(
                  Current[Current.size() - 1 - SwapDepth]) < 0) {
            swap(SwapDepth);
            if (isa_and_nonnull<UnusedSlot>(TgtTop)) {
              // Usually we keep a slot that is to-be-removed, if the current
              // top is arbitrary. However, since we are in a stack-too-deep
              // situation, pop it immediately to compress the stack (we can
              // always push back an unused slot in the end).
              pop();
            }
            return true;
          }
        }
        // TODO: otherwise we rely on stack compression or stack-to-memory.
      }
      swap(Current.size() - RIdx - 1);
      return true;
    }
  }

  // Current.size() > Target.size() cannot be true anymore, since if the
  // current top is no longer required, we already popped it, and if it is
  // required, we already swapped it down to a suitable target position.
  assert(Current.size() <= Target.size());

  // If a lower slot should be removed, try to bring up the slot that should
  // end up there. Note that after the cases above, there will always be
  // a target slot to duplicate in this case.
  for (size_t RIdx = 0, REndIdx = Current.size(); RIdx < REndIdx; ++RIdx) {
    if (match(Current[RIdx], Target[RIdx]) || isa<UnusedSlot>(Target[RIdx]))
      continue;

    // There are too many copies of the slot and there is a target slot at this
    // position.
    if (getCurrentNumOccurrences(Current[RIdx]) < 0) {
      if (!rematerializeUnreachableSlot())
        bringUpTargetSlot(RIdx, /* can't fail */ true);
      return true;
    }
  }

  // At this point we want to keep all slots.
  for (size_t RIdx = 0, REndIdx = Current.size(); RIdx < REndIdx; ++RIdx)
    assert(getCurrentNumOccurrences(Current[RIdx]) >= 0);

  // If the top is not in position, try to find a slot that wants to be at the
  // top and swap it up.
  if (!match(SrcTop, TgtTop))
    if (swapIfCurrent(0, Current.size(), [&](const StackSlot *SrcSlot) {
          return match(SrcSlot, TgtTop);
        }))
      return true;

  // If we still need more slots, produce a suitable one.
  if (Current.size() < Target.size()) {
    if (!rematerializeUnreachableSlot())
      bringUpTargetSlot(Current.size(), /* can't fail */ true);
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
  assert(match(SrcTop, TgtTop) && "The top slot should be already shuffled.");

  size_t StartRIdx = Current.size() > (StackDepthLimit + 1)
                         ? Current.size() - (StackDepthLimit + 1)
                         : 0u;
  // If we find a lower slot that is out of position, but also compatible with
  // the top, swap that up.
  if (swapIfTarget(StartRIdx, Current.size(), [&](const StackSlot *TgtSlot) {
        return match(SrcTop, TgtSlot);
      }))
    return true;

  // Swap up any reachable slot that is still out of position.
  if (swapIfCurrent(StartRIdx, Current.size(), [&](const StackSlot *SrcSlot) {
        return SrcTop != SrcSlot;
      }))
    return true;

  // We are in a stack-too-deep situation and try to reduce the stack size.
  // If the current top is merely kept since the target slot is arbitrary, pop
  // it.
  if (isa<UnusedSlot>(TgtTop) && getCurrentNumOccurrences(SrcTop) <= 0) {
    pop();
    return true;
  }

  // If any reachable slot is merely kept, since the target slot is arbitrary,
  // swap it up and pop it.
  for (size_t RIdx = 0, REndIdx = Current.size(); RIdx < REndIdx; ++RIdx) {
    if (isa<UnusedSlot>(Target[RIdx]) &&
        getCurrentNumOccurrences(Current[RIdx]) <= 0) {
      swap(Current.size() - RIdx - 1);
      pop();
      return true;
    }
  }

  // We cannot avoid a stack-too-deep error. Repeat the above without
  // restricting to reachable slots.
  if (swapIfTarget(0, Current.size(), [&](const StackSlot *TgtSlot) {
        return match(SrcTop, TgtSlot);
      }))
    return true;

  if (swapIfCurrent(0, Current.size(), [&](const StackSlot *SrcSlot) {
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
