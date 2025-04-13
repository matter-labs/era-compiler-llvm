//===---- EVMStackShuffler.h - Implementation of stack shuffling ---- C++*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Attempts to find the cheapest transition between two stacks.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKSHUFFLER_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKSHUFFLER_H

#include "EVMStackModel.h"

namespace llvm {

/// Attempts to find the cheapest transition between two stacks.
class EVMStackShuffler {
  Stack &Current;
  const Stack &Target;
  unsigned StackDepthLimit;
  bool StackTooDeepErr = false;

  using MatchFTy = std::function<bool(const StackSlot *, const StackSlot *)>;
  using GetNumOccurrencesFTy =
      std::function<int(const StackSlot *, Stack &, const Stack &)>;
  using SwapFTy = std::function<void(size_t, Stack &)>;
  using PopFTy = std::function<void()>;
  using RematerializeFTy = std::function<void(const StackSlot *)>;

  MatchFTy MatchF = nullptr;
  GetNumOccurrencesFTy GetCurrentNumOccurrencesF = nullptr;
  GetNumOccurrencesFTy GetTargetNumOccurrencesF = nullptr;
  SwapFTy SwapF = nullptr;
  PopFTy PopF = nullptr;
  RematerializeFTy RematerializeF = nullptr;

public:
  EVMStackShuffler(Stack &Current, const Stack &Target,
                   unsigned StackDepthLimit)
      : Current(Current), Target(Target), StackDepthLimit(StackDepthLimit) {}

  bool hasError() { return StackTooDeepErr; }
  void setMatch(MatchFTy F) { MatchF = std::move(F); }
  void setGetCurrentNumOccurrences(GetNumOccurrencesFTy F) {
    GetCurrentNumOccurrencesF = std::move(F);
  }
  void setGetTargetNumOccurrences(GetNumOccurrencesFTy F) {
    GetTargetNumOccurrencesF = std::move(F);
  }
  void setSwap(SwapFTy F) { SwapF = std::move(F); }
  void setPop(PopFTy F) { PopF = std::move(F); }
  void setRematerialize(RematerializeFTy F) { RematerializeF = std::move(F); }

private:
  /// Checks if \p SrcSlot can share the same position in a stack with
  /// \p TgtSlot.
  /// \return true if the slots are equal or if \p TgtSlot is
  /// unused allowing any slot to be placed at that position.
  bool match(const StackSlot *SrcSlot, const StackSlot *TgtSlot) {
    // nullptr is used to pad the smaller stack so it matches the larger one.
    // A missing slot does not match any slot.
    if (!SrcSlot || !TgtSlot)
      return false;
    if (isa<UnusedSlot>(TgtSlot))
      return true;
    return MatchF ? MatchF(SrcSlot, TgtSlot) : SrcSlot == TgtSlot;
  }
  int getCurrentNumOccurrences(const StackSlot *SrcSlot) {
    if (GetCurrentNumOccurrencesF)
      return GetCurrentNumOccurrencesF(SrcSlot, Current, Target);
    return 0;
  }
  int getTargetNumOccurrences(const StackSlot *TgtSlot) {
    if (GetTargetNumOccurrencesF)
      return GetTargetNumOccurrencesF(TgtSlot, Current, Target);
    return 0;
  }
  bool swapIfCurrent(size_t StartRIdx, size_t EndRIdx,
                     const std::function<bool(const StackSlot *)> &P) {
    for (size_t RIdx = StartRIdx; RIdx < EndRIdx; ++RIdx) {
      if (match(Current[RIdx], Target[RIdx]))
        continue;
      if (P(Current[RIdx])) {
        swap(Current.size() - RIdx - 1);
        return true;
      }
    }
    return false;
  }
  bool swapIfTarget(size_t StartRIdx, size_t EndRIdx,
                    const std::function<bool(const StackSlot *)> &P) {
    for (size_t RIdx = StartRIdx; RIdx < EndRIdx; ++RIdx) {
      if (match(Current[RIdx], Target[RIdx]))
        continue;
      if (P(Target[RIdx])) {
        swap(Current.size() - RIdx - 1);
        return true;
      }
    }
    return false;
  }
  void swap(size_t I) {
    if (SwapF)
      SwapF(I, Current);
    std::swap(Current[Current.size() - I - 1], Current.back());
  }
  void pop() {
    if (PopF)
      PopF();
    Current.pop_back();
  }
  void rematerialize(const StackSlot *S) {
    if (RematerializeF)
      RematerializeF(S);
    Current.push_back(S);
  }

public:
  /// After `shuffle`, the source and target stacks are of equal size and
  /// corresponding slots match.
  /// TODO: assert
  void shuffle() {
    // The shuffling algorithm should always terminate in polynomial time, but
    // we provide a limit in case it does not terminate due to a bug.
    for (unsigned I = 0; I < 1000; ++I)
      if (!step())
        return;
    report_fatal_error("Could not create stack after 1000 iterations.");
  }

private:
  /// Perform one stack manipulation (push, pop, dup, swap).
  /// \return false if shuffling is done.
  bool step();

  /// Copies a slot from the target stack into the current stack if it is either
  /// missing or present in fewer copies.
  /// \param StartRIdx The index from which to start searching.
  bool copyMissingSlotFromTarget(size_t StartRIdx, bool CannotFail = false);

  // If dupping an ideal slot causes a slot that will still be required to
  // become unreachable, then dup the latter slot first.
  // \return true, if it performed a dup.
  bool rematerializeUnreachableSlot();
};

/// Transforms \p CurrentStack to \p TargetStack. Modifies `CurrentStack` itself
/// after each step().
void calculateStack(Stack &CurrentStack, Stack const &TargetStack,
                    unsigned StackDepthLimit,
                    const std::function<void(unsigned)> &Swap,
                    const std::function<void(const StackSlot *)> &Rematerialize,
                    const std::function<void()> &Pop);

} // end namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVMSTACKSHUFFLER_H
