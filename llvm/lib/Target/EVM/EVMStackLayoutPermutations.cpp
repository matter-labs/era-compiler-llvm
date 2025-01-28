//===- EVMStackLayoutPermutations.h - Stack layout permutations -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines various stack layout permutations.
//
//===----------------------------------------------------------------------===//

#include "EVMStackLayoutPermutations.h"
#include "EVMStackShuffler.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

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

/// Return a range covering  the last N elements of \p RangeOrContainer.
template <typename T> auto take_back(T &&RangeOrContainer, size_t N = 1) {
  return make_range(std::prev(adl_end(RangeOrContainer), N),
                    adl_end(RangeOrContainer));
}

/// Returns all stack too deep errors that would occur when shuffling \p Source
/// to \p Target.
SmallVector<EVMStackLayoutPermutations::StackTooDeep>
EVMStackLayoutPermutations::findStackTooDeep(Stack const &Source,
                                             Stack const &Target) {
  Stack CurrentStack = Source;
  SmallVector<EVMStackLayoutPermutations::StackTooDeep> Errors;

  auto getVariableChoices = [](auto &&SlotRange) {
    SmallVector<Register> Result;
    for (auto const *Slot : SlotRange)
      if (auto const *VarSlot = dyn_cast<VariableSlot>(Slot))
        if (!is_contained(Result, VarSlot->getReg()))
          Result.push_back(VarSlot->getReg());
    return Result;
  };

  createStackLayout(
      CurrentStack, Target,
      [&](unsigned I) {
        if (I > 16)
          Errors.emplace_back(EVMStackLayoutPermutations::StackTooDeep{
              I - 16, getVariableChoices(take_back(CurrentStack, I + 1))});
      },
      [&](const StackSlot *Slot) {
        if (Slot->isRematerializable())
          return;

        if (auto Depth = offset(reverse(CurrentStack), Slot);
            Depth && *Depth >= 16)
          Errors.emplace_back(EVMStackLayoutPermutations::StackTooDeep{
              *Depth - 15,
              getVariableChoices(take_back(CurrentStack, *Depth + 1))});
      },
      [&]() {});
  return Errors;
}

/// Returns the ideal stack to have before executing an operation that outputs
/// \p OperationOutput, s.t., shuffling to \p Post is cheap (excluding the
/// input of the operation itself). If \p GenerateSlotOnTheFly returns true for
/// a slot, this slot should not occur in the ideal stack, but rather be
/// generated on the fly during shuffling.
Stack EVMStackLayoutPermutations::createIdealLayout(
    const Stack &OperationOutput, const Stack &Post,
    std::function<bool(const StackSlot *)> GenerateSlotOnTheFly) {
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
    if (is_contained(OperationOutput, Slot) || GenerateSlotOnTheFly(Slot))
      --PreOperationLayoutSize;

  // The symbolic layout directly after the operation has the form
  // PreviousSlot{0}, ..., PreviousSlot{n}, [output<0>], ..., [output<m>]
  LayoutT Layout;
  for (size_t Index = 0; Index < PreOperationLayoutSize; ++Index)
    Layout.emplace_back(PreviousSlot{Index});
  Layout.append(OperationOutput.begin(), OperationOutput.end());

  // Shortcut for trivial case.
  if (Layout.empty())
    return Stack{};

  // Next we will shuffle the layout to the Post stack using ShuffleOperations
  // that are aware of PreviousSlot's.
  struct ShuffleOperations {
    LayoutT &Layout;
    const Stack &Post;
    std::set<StackSlot *> Outputs;
    Multiplicity Mult;
    std::function<bool(const StackSlot *)> GenerateSlotOnTheFly;
    ShuffleOperations(
        LayoutT &Layout, Stack const &Post,
        std::function<bool(const StackSlot *)> GenerateSlotOnTheFly)
        : Layout(Layout), Post(Post),
          GenerateSlotOnTheFly(GenerateSlotOnTheFly) {
      for (const auto &LayoutSlot : Layout)
        if (auto Slot = std::get_if<StackSlot *>(&LayoutSlot))
          Outputs.insert(*Slot);

      for (const auto &LayoutSlot : Layout)
        if (auto Slot = std::get_if<StackSlot *>(&LayoutSlot))
          --Mult[*Slot];

      for (auto *Slot : Post)
        if (Outputs.count(Slot) || GenerateSlotOnTheFly(Slot))
          ++Mult[Slot];
    }

    bool isCompatible(size_t Source, size_t Target) {
      return Source < Layout.size() && Target < Post.size() &&
             (isa<JunkSlot>(Post[Target]) ||
              std::visit(Overload{[&](const PreviousSlot &) {
                                    return !Outputs.count(Post[Target]) &&
                                           !GenerateSlotOnTheFly(Post[Target]);
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

      auto SlotLHS = std::get_if<StackSlot *>(&Layout[Lhs]);
      auto SlotRHS = std::get_if<StackSlot *>(&Layout[Rhs]);
      return SlotLHS && SlotRHS && *SlotLHS == *SlotRHS;
    }

    int sourceMultiplicity(size_t Offset) {
      return std::visit(
          Overload{[&](PreviousSlot const &) { return 0; },
                   [&](const StackSlot *S) { return Mult.at(S); }},
          Layout[Offset]);
    }

    int targetMultiplicity(size_t Offset) {
      if (!Outputs.count(Post[Offset]) && !GenerateSlotOnTheFly(Post[Offset]))
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
  };

  Shuffler<ShuffleOperations>::shuffle(Layout, Post, GenerateSlotOnTheFly);

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
    if (PreviousSlot *PrevSlot = std::get_if<PreviousSlot>(&IdealPosition))
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

Stack EVMStackLayoutPermutations::combineStack(Stack const &Stack1,
                                               Stack const &Stack2) {
  // TODO: it would be nicer to replace this by a constructive algorithm.
  // Currently it uses a reduced version of the Heap Algorithm to partly
  // brute-force, which seems to work decently well.

  Stack CommonPrefix;
  for (unsigned Idx = 0; Idx < std::min(Stack1.size(), Stack2.size()); ++Idx) {
    StackSlot *Slot1 = Stack1[Idx];
    const StackSlot *Slot2 = Stack2[Idx];
    if (!(Slot1 == Slot2))
      break;
    CommonPrefix.push_back(Slot1);
  }

  Stack Stack1Tail, Stack2Tail;
  for (auto *Slot : drop_begin(Stack1, CommonPrefix.size()))
    Stack1Tail.push_back(Slot);

  for (auto *Slot : drop_begin(Stack2, CommonPrefix.size()))
    Stack2Tail.push_back(Slot);

  if (Stack1Tail.empty()) {
    CommonPrefix.append(compressStack(Stack2Tail));
    return CommonPrefix;
  }

  if (Stack2Tail.empty()) {
    CommonPrefix.append(compressStack(Stack1Tail));
    return CommonPrefix;
  }

  Stack Candidate;
  for (auto *Slot : Stack1Tail)
    if (!is_contained(Candidate, Slot))
      Candidate.push_back(Slot);

  for (auto *Slot : Stack2Tail)
    if (!is_contained(Candidate, Slot))
      Candidate.push_back(Slot);

  {
    auto RemIt = std::remove_if(
        Candidate.begin(), Candidate.end(), [](const StackSlot *Slot) {
          return isa<LiteralSlot>(Slot) || isa<SymbolSlot>(Slot) ||
                 isa<FunctionCallReturnLabelSlot>(Slot);
        });
    Candidate.erase(RemIt, Candidate.end());
  }

  auto evaluate = [&](Stack const &Candidate) -> size_t {
    size_t NumOps = 0;
    Stack TestStack = Candidate;
    auto Swap = [&](unsigned SwapDepth) {
      ++NumOps;
      if (SwapDepth > 16)
        NumOps += 1000;
    };

    auto DupOrPush = [&](const StackSlot *Slot) {
      if (Slot->isRematerializable())
        return;

      Stack Tmp = CommonPrefix;
      Tmp.append(TestStack);
      auto Depth = offset(reverse(Tmp), Slot);
      if (Depth && *Depth >= 16)
        NumOps += 1000;
    };
    createStackLayout(TestStack, Stack1Tail, Swap, DupOrPush, [&]() {});
    TestStack = Candidate;
    createStackLayout(TestStack, Stack2Tail, Swap, DupOrPush, [&]() {});
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

Stack EVMStackLayoutPermutations::compressStack(Stack CurStack) {
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
        if (Depth + *DupDepth <= 16) {
          FirstDupOffset = CurStack.size() - Depth - 1;
          break;
        }
      }
    }
  } while (FirstDupOffset);
  return CurStack;
}

/// Returns the number of operations required to transform stack \p Source to
/// \p Target.
size_t EVMStackLayoutPermutations::evaluateStackTransform(Stack Source,
                                                          Stack const &Target) {
  size_t OpGas = 0;
  auto Swap = [&](unsigned SwapDepth) {
    if (SwapDepth > 16)
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

      if (*Depth < 16)
        OpGas += 3; // DUP* gas price
      else
        OpGas += 1000;
    }
  };
  auto Pop = [&]() { OpGas += 2; };

  createStackLayout(Source, Target, Swap, DupOrPush, Pop);
  return OpGas;
}
