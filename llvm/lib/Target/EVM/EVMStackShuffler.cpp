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

bool EVMStackShuffler::bringUpTargetSlot(size_t TOffset, bool CannotFail) {
  std::deque<size_t> ToVisit{TOffset};
  DenseSet<size_t> Visited;

  while (!ToVisit.empty()) {
    size_t Offset = *ToVisit.begin();
    ToVisit.erase(ToVisit.begin());
    Visited.insert(Offset);
    if (getTargetSignificantUses(Offset) > 0) {
      rematerialize(Target[Offset]);
      return true;
    }
    // There must be another slot we can dup/push that will lead to the target
    // slot at ``offset`` to be fixed.
    for (size_t NextOffset = 0;
         NextOffset < std::min(Current.size(), Target.size()); ++NextOffset)
      if (!isCompatible(NextOffset, NextOffset) &&
          isCompatible(NextOffset, Offset))
        if (!Visited.count(NextOffset))
          ToVisit.emplace_back(NextOffset);
  }
  if (CannotFail)
    llvm_unreachable("Unexpected shuffler behavior.");

  return false;
}

bool EVMStackShuffler::rematerializeUnreachableSlots() {
  if (Current.size() < (StackDepthLimit - 1))
    return false;

  // Check whether any deep slot might still be needed later (i.e. we still
  // need to reach it with a DUP or SWAP).
  for (size_t COffset = 0; COffset < (Current.size() - (StackDepthLimit - 1));
       ++COffset) {
    if (isCompatible(COffset, COffset)) {
      // The slot is in place, but we might need another copy if it.
      if (getCurrentSignificantUses(COffset) > 0) {
        // If this slot occurs again later, we skip this occurrence.
        if (const auto &R = llvm::seq<size_t>(COffset + 1, Current.size());
            any_of(R, [&](size_t Offset) {
              return Current[COffset] == Current[Offset];
            }))
          continue;

        // Duplicate unreachable slot.
        for (size_t TOffset = 0; TOffset < Target.size(); ++TOffset) {
          if (!isArbitraryTarget(TOffset) && isCompatible(COffset, TOffset)) {
            rematerialize(Target[TOffset]);
            return true;
          }
        }
      }
      continue;
    }

    // This slot needs to be moved.
    // If the current top fixes the slot, swap it down now.
    if (isCompatible(Current.size() - 1, COffset)) {
      swap(Current.size() - COffset - 1);
      return true;
    }
    // Bring up a slot to fix this now, if possible.
    if (bringUpTargetSlot(COffset))
      return true;
    // Otherwise swap up the slot that will fix the offending slot.
    for (size_t Off = COffset + 1; Off < Current.size(); ++Off)
      if (isCompatible(Off, COffset)) {
        swap(Current.size() - Off - 1);
        return true;
      }
    // Otherwise give up - we will need stack compression or stack limit
    // evasion.
  }
  return false;
}

bool EVMStackShuffler::step() {
  // All source slots are final.
  if (const auto &R = llvm::seq<size_t>(0u, Current.size());
      all_of(R, [&](size_t Index) { return isCompatible(Index, Index); })) {
    // Bring up all remaining target slots, if any, or terminate otherwise.
    if (Current.size() < Target.size()) {
      if (!rematerializeUnreachableSlots())
        bringUpTargetSlot(Current.size(), /* can't fail */ true);
      return true;
    }
    return false;
  }

  size_t CurrentTop = Current.size() - 1;
  // If we no longer need the current stack top, we pop it, unless we need an
  // arbitrary slot at this position in the target.
  if (getCurrentSignificantUses(CurrentTop) < 0 &&
      !isArbitraryTarget(CurrentTop)) {
    pop();
    return true;
  }

  assert(Target.size() > 0);
  // If the top is not supposed to be exactly what is on top right now, try to
  // find a lower position to swap it to.
  if (!isCompatible(CurrentTop, CurrentTop) || isArbitraryTarget(CurrentTop)) {
    for (size_t Off = 0; Off < std::min(Current.size(), Target.size()); ++Off) {
      // It makes sense to swap to a lower position, if
      // * the lower slot is not already in position,
      // * we would not just swap identical slots,
      // * the lower position wants to have this slot.
      bool NeedsSwapping = !isCompatible(Off, Off) &&
                           Current[Off] != Current[CurrentTop] &&
                           isCompatible(CurrentTop, Off);
      if (!NeedsSwapping)
        continue;

      // We cannot swap that deep.
      if (Current.size() - Off - 1 > StackDepthLimit) {
        // If there is a reachable slot to be removed, park the current top
        // there.
        for (size_t SwapDepth = StackDepthLimit; SwapDepth > 0; --SwapDepth) {
          if (getCurrentSignificantUses(Current.size() - 1 - SwapDepth) < 0) {
            swap(SwapDepth);
            if (isArbitraryTarget(CurrentTop)) {
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
      swap(Current.size() - Off - 1);
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
  for (size_t Off = 0; Off < Current.size(); ++Off) {
    if (isCompatible(Off, Off) || isArbitraryTarget(Off))
      continue;

    // There are too many copies of the slot and there is a target slot at this
    // position.
    if (getCurrentSignificantUses(Off) < 0 && Off <= Target.size()) {
      if (!rematerializeUnreachableSlots())
        bringUpTargetSlot(Off, /* can't fail */ true);
      return true;
    }
  }

  // At this point we want to keep all slots.
  for (size_t I = 0; I < Current.size(); ++I)
    assert(getCurrentSignificantUses(I) >= 0);
  assert(Current.size() <= Target.size());

  // If the top is not in position, try to find a slot that wants to be at the
  // top and swap it up.
  if (!isCompatible(CurrentTop, CurrentTop)) {
    for (size_t COffset = 0; COffset < Current.size(); ++COffset) {
      if (!isCompatible(COffset, COffset) &&
          isCompatible(COffset, CurrentTop)) {
        swap(Current.size() - COffset - 1);
        return true;
      }
    }
  }

  // If we still need more slots, produce a suitable one.
  if (Current.size() < Target.size()) {
    if (!rematerializeUnreachableSlots())
      bringUpTargetSlot(Current.size(), /* can't fail */ true);
    return true;
  }

  // The stack has the correct size, each slot has the correct number of
  // copies and the top is in position.
  assert(Current.size() == Target.size());
  size_t Size = Current.size();
  for (size_t I = 0; I < Current.size(); ++I)
    assert(getCurrentSignificantUses(I) == 0 &&
           (isArbitraryTarget(I) || getTargetSignificantUses(I) == 0));
  assert(isCompatible(CurrentTop, CurrentTop));

  const auto &SwappableOffsets = llvm::seq<size_t>(
      Size > (StackDepthLimit + 1) ? Size - (StackDepthLimit + 1) : 0u, Size);

  // If we find a lower slot that is out of position, but also compatible with
  // the top, swap that up.
  for (size_t Offset : SwappableOffsets) {
    if (!isCompatible(Offset, Offset) && isCompatible(CurrentTop, Offset)) {
      swap(Size - Offset - 1);
      return true;
    }
  }

  // Swap up any reachable slot that is still out of position.
  for (size_t Offset : SwappableOffsets) {
    if (!isCompatible(Offset, Offset) &&
        Current[Offset] != Current[CurrentTop]) {
      swap(Size - Offset - 1);
      return true;
    }
  }

  // We are in a stack-too-deep situation and try to reduce the stack size.
  // If the current top is merely kept since the target slot is arbitrary, pop
  // it.
  if (isArbitraryTarget(CurrentTop) &&
      getCurrentSignificantUses(CurrentTop) <= 0) {
    pop();
    return true;
  }

  // If any reachable slot is merely kept, since the target slot is arbitrary,
  // swap it up and pop it.
  for (size_t Offset : SwappableOffsets) {
    if (isArbitraryTarget(Offset) && getCurrentSignificantUses(Offset) <= 0) {
      swap(Size - Offset - 1);
      pop();
      return true;
    }
  }

  // We cannot avoid a stack-too-deep error. Repeat the above without
  // restricting to reachable slots.
  for (size_t Offset = 0; Offset < Size; ++Offset) {
    if (!isCompatible(Offset, Offset) && isCompatible(CurrentTop, Offset)) {
      swap(Size - Offset - 1);
      return true;
    }
  }

  for (size_t Offset = 0; Offset < Size; ++Offset) {
    if (!isCompatible(Offset, Offset) &&
        Current[Offset] != Current[CurrentTop]) {
      swap(Size - Offset - 1);
      return true;
    }
  }

  llvm_unreachable("Unexpected state");
}

void llvm::calculateStack(
    Stack &CurrentStack, Stack const &TargetStack, unsigned StackDepthLimit,
    const std::function<void(unsigned)> &Swap,
    const std::function<void(const StackSlot *)> &Rematerialize,
    const std::function<void()> &Pop) {
  EVMStackShuffler TheShuffler =
      EVMStackShuffler(CurrentStack, TargetStack, StackDepthLimit);
  auto getSignificantUses = [](const StackSlot *Slot, Stack &C,
                               const Stack &T) {
    int CUses = -count(C, Slot);
    if (T.size() > C.size())
      CUses += std::count(T.begin() + C.size(), T.end(), Slot);
    CUses += count_if(zip(C, T), [Slot](auto In) {
      auto [CSlot, TSlot] = In;
      if (isa<UnusedSlot>(TSlot))
        return CSlot == Slot;
      return TSlot == Slot;
    });
    return CUses;
  };

  TheShuffler.setGetCurrentSignificantUses(getSignificantUses);
  TheShuffler.setGetTargetSignificantUses(getSignificantUses);

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
