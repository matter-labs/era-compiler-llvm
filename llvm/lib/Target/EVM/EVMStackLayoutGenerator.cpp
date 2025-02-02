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

#include "EVMStackLayoutGenerator.h"
#include "EVMMachineCFGInfo.h"
#include "EVMRegisterInfo.h"
#include "EVMStackShuffler.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-stack-layout-generator"

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

/// Return a range covering  the last N elements of \p RangeOrContainer.
template <typename T> auto take_back(T &&RangeOrContainer, size_t N = 1) {
  return make_range(std::prev(adl_end(RangeOrContainer), N),
                    adl_end(RangeOrContainer));
}

/// Returns all stack too deep errors that would occur when shuffling \p Source
/// to \p Target.
SmallVector<EVMStackLayoutGenerator::StackTooDeep>
findStackTooDeep(Stack const &Source, Stack const &Target) {
  Stack CurrentStack = Source;
  SmallVector<EVMStackLayoutGenerator::StackTooDeep> Errors;

  auto getVariableChoices = [](auto &&SlotRange) {
    SmallVector<Register> Result;
    for (auto const *Slot : SlotRange)
      if (auto const *RegSlot = dyn_cast<RegisterSlot>(Slot))
        if (!is_contained(Result, RegSlot->getReg()))
          Result.push_back(RegSlot->getReg());
    return Result;
  };

  ::createStackLayout(
      CurrentStack, Target,
      [&](unsigned I) {
        if (I > 16)
          Errors.emplace_back(EVMStackLayoutGenerator::StackTooDeep{
              I - 16, getVariableChoices(take_back(CurrentStack, I + 1))});
      },
      [&](const StackSlot *Slot) {
        if (Slot->isRematerializable())
          return;

        if (auto Depth = offset(reverse(CurrentStack), Slot);
            Depth && *Depth >= 16)
          Errors.emplace_back(EVMStackLayoutGenerator::StackTooDeep{
              *Depth - 15,
              getVariableChoices(take_back(CurrentStack, *Depth + 1))});
      },
      [&]() {});
  return Errors;
}

/// Returns the ideal stack to have before executing the MachineInstr \p MI
/// s.t. shuffling to \p Post is cheap (excluding the input of the operation
/// itself). If \p GenerateSlotOnTheFly returns true for a slot, this slot
/// should not occur in the ideal stack, but rather be generated on the fly
/// during shuffling.
template <typename Callable>
Stack createIdealLayout(const SmallVector<StackSlot *> &OpDefs,
                        const Stack &Post, Callable GenerateSlotOnTheFly) {

  // Determine the number of slots that have to be on stack before executing the
  // operation (excluding the inputs of the operation itself). That is slots
  // that should not be generated on the fly and are not outputs of the
  // operation.
  size_t PreOperationLayoutSize = Post.size();
  for (const auto *Slot : Post)
    if (is_contained(OpDefs, Slot) || GenerateSlotOnTheFly(Slot))
      --PreOperationLayoutSize;

  SmallVector<UnknownSlot> UnknownSlots;
  for (size_t Index = 0; Index < PreOperationLayoutSize; ++Index)
    UnknownSlots.emplace_back(Index);

  // The symbolic layout directly after the operation has the form
  // PreviousSlot{0}, ..., PreviousSlot{n}, [output<0>], ..., [output<m>]
  Stack Layout;
  for (auto &S : UnknownSlots)
    Layout.push_back(&S);
  append_range(Layout, OpDefs);

  // Shortcut for trivial case.
  if (Layout.empty())
    return Stack{};

  // Next we will shuffle the layout to the Post stack using ShuffleOperations
  // that are aware of PreviousSlot's.
  struct ShuffleOperations {
    Stack &Layout;
    const Stack &Post;
    std::set<StackSlot *> Outputs;
    Multiplicity Mult;
    Callable GenerateSlotOnTheFly;
    ShuffleOperations(Stack &Layout, Stack const &Post,
                      Callable GenerateSlotOnTheFly)
        : Layout(Layout), Post(Post),
          GenerateSlotOnTheFly(GenerateSlotOnTheFly) {
      for (auto *Slot : Layout)
        if (!isa<UnknownSlot>(Slot))
          Outputs.insert(Slot);

      for (const auto *Slot : Layout)
        if (!isa<UnknownSlot>(Slot))
          --Mult[Slot];

      for (auto *Slot : Post)
        if (Outputs.count(Slot) || GenerateSlotOnTheFly(Slot))
          ++Mult[Slot];
    }

    bool isCompatible(size_t Source, size_t Target) {
      if (Source >= Layout.size() || Target >= Post.size())
        return false;
      if (isa<JunkSlot>(Post[Target]))
        return true;
      if (isa<UnknownSlot>(Layout[Source]))
        return !Outputs.count(Post[Target]) &&
               !GenerateSlotOnTheFly(Post[Target]);
      return Layout[Source] == Post[Target];
    }

    bool sourceIsSame(size_t Lhs, size_t Rhs) {
      return Layout[Lhs] == Layout[Rhs];
    }

    int sourceMultiplicity(size_t Offset) {
      return isa<UnknownSlot>(Layout[Offset]) ? 0 : Mult.at(Layout[Offset]);
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
      assert(!isa<UnknownSlot>(Layout[Layout.size() - I - 1]) ||
             !isa<UnknownSlot>(Layout.back()));
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
    auto *IdealPosition = Layout[Idx];
    if (auto *PrevSlot = dyn_cast<UnknownSlot>(IdealPosition))
      IdealLayout[PrevSlot->getIndex()] = Slot;
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

} // end anonymous namespace

EVMStackLayoutGenerator::EVMStackLayoutGenerator(
    const MachineFunction &MF, const MachineLoopInfo *MLI,
    const EVMStackModel &StackModel, const EVMMachineCFGInfo &CFGInfo)
    : MF(MF), MLI(MLI), StackModel(StackModel), CFGInfo(CFGInfo) {}

std::unique_ptr<EVMStackLayout> EVMStackLayoutGenerator::run() {
  runPropagation();
  LLVM_DEBUG({
    dbgs() << "************* Stack Layout *************\n";
    dump(dbgs());
  });

  return std::make_unique<EVMStackLayout>(MBBEntryLayoutMap, MBBExitLayoutMap,
                                          OperationEntryLayoutMap);
}

Stack EVMStackLayoutGenerator::propagateStackThroughOperation(
    Stack ExitStack, const Operation &Op, bool CompressStack) {
  // Enable aggressive stack compression for recursive calls.
  if (Op.isFunctionCall())
    // TODO: compress stack for recursive functions.
    CompressStack = false;

  // This is a huge tradeoff between code size, gas cost and stack size.
  auto generateSlotOnTheFly = [&](const StackSlot *Slot) {
    return CompressStack && Slot->isRematerializable();
  };

  SmallVector<StackSlot *> OpDefs =
      StackModel.getSlotsForInstructionDefs(Op.getMachineInstr());

  // Determine the ideal permutation of the slots in ExitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // 'IdealStack + Operation.output' to ExitLayout is cheap.
  Stack IdealStack = createIdealLayout(OpDefs, ExitStack, generateSlotOnTheFly);

#ifndef NDEBUG
  // Make sure the resulting previous slots do not overlap with any assigned
  // variables.
  if (Op.isAssignment())
    for (auto *StackSlot : IdealStack)
      if (const auto *RegSlot = dyn_cast<RegisterSlot>(StackSlot))
        assert(!Op.getMachineInstr()->definesRegister(RegSlot->getReg()));
#endif // NDEBUG

  // Since stack+Operation.output can be easily shuffled to ExitLayout, the
  // desired layout before the operation is stack+Operation.input;
  IdealStack.append(Op.getInput());

  // Store the exact desired operation entry layout. The stored layout will be
  // recreated by the code transform before executing the operation. However,
  // this recreation can produce slots that can be freely generated or are
  // duplicated, i.e. we can compress the stack afterwards without causing
  // problems for code generation later.
  OperationEntryLayoutMap[&Op] = IdealStack;

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!IdealStack.empty()) {
    if (IdealStack.back()->isRematerializable())
      IdealStack.pop_back();
    else if (auto Offset = offset(drop_begin(reverse(IdealStack), 1),
                                  IdealStack.back())) {
      if (*Offset + 2 < 16)
        IdealStack.pop_back();
      else
        break;
    } else
      break;
  }

  return IdealStack;
}

Stack EVMStackLayoutGenerator::propagateStackThroughBlock(
    Stack ExitStack, const MachineBasicBlock *Block, bool CompressStack) {
  Stack CurrentStack = ExitStack;
  for (const Operation &Op : reverse(StackModel.getOperations(Block))) {
    Stack NewStack =
        propagateStackThroughOperation(CurrentStack, Op, CompressStack);
    if (!CompressStack && !findStackTooDeep(NewStack, CurrentStack).empty())
      // If we had stack errors, run again with stack compression enabled.
      return propagateStackThroughBlock(std::move(ExitStack), Block,
                                        /*CompressStack*/ true);
    CurrentStack = std::move(NewStack);
  }
  return CurrentStack;
}

// Returns the number of junk slots to be prepended to \p TargetLayout for
// an optimal transition from \p EntryLayout to \p TargetLayout.
static size_t getOptimalNumberOfJunks(const Stack &EntryLayout,
                                      const Stack &TargetLayout) {
  size_t BestCost = EvaluateStackTransform(EntryLayout, TargetLayout);
  size_t BestNumJunk = 0;
  size_t MaxJunk = EntryLayout.size();
  for (size_t NumJunk = 1; NumJunk <= MaxJunk; ++NumJunk) {
    Stack JunkedTarget(NumJunk, EVMStackModel::getJunkSlot());
    JunkedTarget.append(TargetLayout);
    size_t Cost = EvaluateStackTransform(EntryLayout, JunkedTarget);
    if (Cost < BestCost) {
      BestCost = Cost;
      BestNumJunk = NumJunk;
    }
  }
  return BestNumJunk;
}

void EVMStackLayoutGenerator::runPropagation() {
  std::deque<const MachineBasicBlock *> ToVisit{&MF.front()};
  DenseSet<const MachineBasicBlock *> Visited;

  // Collect all the backedges in the MF.
  // TODO: CPR-1847. Consider changing CFG before the stackification such that
  // every loop has only one backedge.
  SmallVector<std::pair<const MachineBasicBlock *, const MachineBasicBlock *>,
              64>
      Backedges;
  for (const MachineLoop *TopLevelLoop : *MLI) {
    // TODO: CPR-1847. Investigate in which order it's better to traverse
    // loops.
    for (const MachineLoop *L : depth_first(TopLevelLoop)) {
      SmallVector<MachineBasicBlock *, 8> Latches;
      L->getLoopLatches(Latches);
      const MachineBasicBlock *Header = L->getHeader();
      transform(Latches, std::back_inserter(Backedges),
                [Header](const MachineBasicBlock *MBB) {
                  return std::make_pair(MBB, Header);
                });
    }
  }

  while (!ToVisit.empty()) {
    // First calculate stack layouts without walking backwards jumps, i.e.
    // assuming the current preliminary entry layout of the backwards jump
    // target as the initial exit layout of the backwards-jumping block.
    while (!ToVisit.empty()) {
      const MachineBasicBlock *Block = *ToVisit.begin();
      ToVisit.pop_front();
      if (Visited.count(Block))
        continue;

      if (std::optional<Stack> ExitLayout =
              getExitLayoutOrStageDependencies(Block, Visited, ToVisit)) {
        Visited.insert(Block);
        MBBExitLayoutMap[Block] = *ExitLayout;
        MBBEntryLayoutMap[Block] =
            propagateStackThroughBlock(*ExitLayout, Block);
        for (auto Pred : Block->predecessors())
          ToVisit.emplace_back(Pred);
      }
    }

    // Revisit these blocks again.
    for (auto [Latch, Header] : Backedges) {
      const Stack &HeaderEntryLayout = MBBEntryLayoutMap[Header];
      const Stack &LatchExitLayout = MBBExitLayoutMap[Latch];
      if (all_of(HeaderEntryLayout, [LatchExitLayout](const StackSlot *Slot) {
            return is_contained(LatchExitLayout, Slot);
          }))
        continue;

      // The latch block does not provide all slots required by the loop
      // header. Therefore we need to visit the subgraph between the latch
      // and header again. We will visit blocks backwards starting from latch
      // and mark all MBBs to-be-visited again until we reach the header.

      ToVisit.emplace_back(Latch);

      // Since we are likely to permute the entry layout of 'Header', we
      // also visit its entries again. This is not required for correctness,
      // since the set of stack slots will match, but it may move some
      // required stack shuffling from the loop condition to outside the loop.
      for (const MachineBasicBlock *Pred : Header->predecessors())
        Visited.erase(Pred);

      // DFS upwards traversal from latch to the header.
      for (auto I = idf_begin(Latch), E = idf_end(Latch); I != E;) {
        const MachineBasicBlock *MBB = *I;
        Visited.erase(MBB);
        if (MBB == Header) {
          I.skipChildren();
          continue;
        }
        ++I;
      }
      // TODO: Consider revisiting the entire graph to propagate the optimal
      // layout above the loop.
    }
  }

  // At this point layouts at conditional jumps are merely
  // compatible, i.e. the exit layout of the jumping block is a superset of the
  // entry layout of the target block. We need to modify the entry layouts
  // of conditional jump targets, s.t., the entry layout of target blocks match
  // the exit layout of the jumping block exactly, except that slots not
  // required after the jump are marked as 'JunkSlot'.
  for (const MachineBasicBlock &MBB : MF) {
    const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(&MBB);
    MBBExitType ExitType = TermInfo->getExitType();
    if (ExitType != MBBExitType::ConditionalBranch)
      continue;

    Stack ExitLayout = MBBExitLayoutMap.at(&MBB);

#ifndef NDEBUG
    // The last block must have produced the condition at the stack top.
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    assert(ExitLayout.back() == StackModel.getStackSlot(*Condition));
#endif // NDEBUG

    // The condition is consumed by the conditional jump.
    ExitLayout.pop_back();
    for (const MachineBasicBlock *Succ : MBB.successors()) {
      const Stack &SuccEntryLayout = MBBEntryLayoutMap.at(Succ);
      Stack NewSuccEntryLayout = ExitLayout;
      // Whatever the block being jumped to does not actually require,
      // can be marked as junk.
      for (StackSlot *&Slot : NewSuccEntryLayout)
        if (!is_contained(SuccEntryLayout, Slot))
          Slot = EVMStackModel::getJunkSlot();

#ifndef NDEBUG
      // Make sure everything the block being jumped to requires is
      // actually present or can be generated.
      for (const StackSlot *Slot : SuccEntryLayout)
        assert(Slot->isRematerializable() ||
               is_contained(NewSuccEntryLayout, Slot));
#endif // NDEBUG

      MBBEntryLayoutMap[Succ] = NewSuccEntryLayout;
    }
  }

  // Create the function entry layout.
  Stack EntryStack;
  bool IsNoReturn = MF.getFunction().hasFnAttribute(Attribute::NoReturn);
  if (!IsNoReturn)
    EntryStack.push_back(StackModel.getFunctionReturnLabelSlot(&MF));

  // Calling convention: input arguments are passed in stack such that the
  // first one specified in the function declaration is passed on the stack TOP.
  EntryStack.append(StackModel.getFunctionParameters());
  std::reverse(IsNoReturn ? EntryStack.begin() : std::next(EntryStack.begin()),
               EntryStack.end());
  MBBEntryLayoutMap[&MF.front()] = std::move(EntryStack);

  // Traverse the CFG and at each block that allows junk, i.e. that is a
  // cut-vertex that never leads to a function return, checks if adding junk
  // reduces the shuffling cost upon entering and if so recursively adds junk
  // to the spanned subgraph. This is needed only for optimization purposes,
  // not for correctness.
  for (const MachineBasicBlock &MBB : MF) {
    if (!CFGInfo.isCutVertex(&MBB) || CFGInfo.isOnPathToFuncReturn(&MBB))
      continue;

    const Stack EntryLayout = MBBEntryLayoutMap.at(&MBB);
    const Stack &ExitLayout = MBBExitLayoutMap.at(&MBB);
    const SmallVector<Operation> &Ops = StackModel.getOperations(&MBB);
    Stack const &NextLayout =
        Ops.empty() ? ExitLayout : OperationEntryLayoutMap.at(&Ops.front());
    if (EntryLayout != NextLayout) {
      size_t OptimalNumJunks = getOptimalNumberOfJunks(EntryLayout, NextLayout);
      if (OptimalNumJunks > 0) {
        addJunksToStackBottom(&MBB, OptimalNumJunks);
        MBBEntryLayoutMap[&MBB] = EntryLayout;
      }
    }
  }
}

std::optional<Stack> EVMStackLayoutGenerator::getExitLayoutOrStageDependencies(
    const MachineBasicBlock *Block,
    const DenseSet<const MachineBasicBlock *> &Visited,
    std::deque<const MachineBasicBlock *> &ToVisit) const {
  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(Block);
  MBBExitType ExitType = TermInfo->getExitType();
  if (ExitType == MBBExitType::UnconditionalBranch) {
    auto [_, Target] = TermInfo->getUnconditionalBranch();
    if (MachineLoop *ML = MLI->getLoopFor(Block);
        ML && ML->isLoopLatch(Block)) {
      // Choose the best currently known entry layout of the jump target
      // as initial exit. Note that this may not yet be the final
      // layout.
      auto It = MBBEntryLayoutMap.find(Target);
      return It == MBBEntryLayoutMap.end() ? Stack{} : It->second;
    }
    // If the current iteration has already visited the jump target,
    // start from its entry layout.
    if (Visited.count(Target))
      return MBBEntryLayoutMap.at(Target);
    // Otherwise stage the jump target for visit and defer the current
    // block.
    ToVisit.emplace_front(Target);
    return std::nullopt;
  }
  if (ExitType == MBBExitType::ConditionalBranch) {
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    bool FalseBBVisited = Visited.count(FalseBB);
    bool TrueBBVisited = Visited.count(TrueBB);

    if (FalseBBVisited && TrueBBVisited) {
      // If the current iteration has already Visited both jump targets,
      // start from its entry layout.
      Stack CombinedStack = combineStack(MBBEntryLayoutMap.at(FalseBB),
                                         MBBEntryLayoutMap.at(TrueBB));
      // Additionally, the jump condition has to be at the stack top at
      // exit.
      CombinedStack.emplace_back(StackModel.getStackSlot(*Condition));
      return CombinedStack;
    }

    // If one of the jump targets has not been visited, stage it for
    // visit and defer the current block.
    if (!FalseBBVisited)
      ToVisit.emplace_front(FalseBB);

    if (!TrueBBVisited)
      ToVisit.emplace_front(TrueBB);

    return std::nullopt;
  }
  if (ExitType == MBBExitType::FunctionReturn) {
    const MachineInstr &MI = Block->back();
    return StackModel.getReturnArguments(MI);
  }

  return Stack{};
}

Stack EVMStackLayoutGenerator::combineStack(Stack const &Stack1,
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

Stack EVMStackLayoutGenerator::compressStack(Stack CurStack) {
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
size_t llvm::EvaluateStackTransform(Stack Source, Stack const &Target) {
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

void EVMStackLayoutGenerator::addJunksToStackBottom(
    const MachineBasicBlock *Entry, size_t NumJunk) {
  for (const MachineBasicBlock *MBB : depth_first(Entry)) {
    Stack EntryTmp(NumJunk, EVMStackModel::getJunkSlot());
    EntryTmp.append(MBBEntryLayoutMap.at(MBB));
    MBBEntryLayoutMap[MBB] = std::move(EntryTmp);

    for (const Operation &Op : StackModel.getOperations(MBB)) {
      Stack OpEntryTmp(NumJunk, EVMStackModel::getJunkSlot());
      OpEntryTmp.append(OperationEntryLayoutMap.at(&Op));
      OperationEntryLayoutMap[&Op] = std::move(OpEntryTmp);
    }

    Stack ExitTmp(NumJunk, EVMStackModel::getJunkSlot());
    ExitTmp.append(MBBExitLayoutMap.at(MBB));
    MBBExitLayoutMap[MBB] = std::move(ExitTmp);
  }
}

#ifndef NDEBUG
void EVMStackLayoutGenerator::dump(raw_ostream &OS) {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot *ParamSlot : StackModel.getFunctionParameters()) {
    if (const auto *Slot = dyn_cast<RegisterSlot>(ParamSlot))
      OS << printReg(Slot->getReg(), nullptr, 0, nullptr) << ' ';
    else if (isa<JunkSlot>(ParamSlot))
      OS << "[unused param] ";
    else
      llvm_unreachable("Unexpected stack slot");
  }
  OS << ");\n";
  OS << "FunctionEntry "
     << " -> Block" << getBlockId(MF.front()) << ";\n";

  for (const auto &MBB : MF)
    printBlock(OS, MBB);
}

void EVMStackLayoutGenerator::printBlock(
    raw_ostream &OS, const MachineBasicBlock &Block) {
  OS << "Block" << getBlockId(Block) << " [\n";
  OS << MBBEntryLayoutMap.at(&Block).toString() << "\n";
  for (auto const &Op : StackModel.getOperations(&Block)) {
    OS << "\n";
    Stack EntryLayout = OperationEntryLayoutMap.at(&Op);
    OS << EntryLayout.toString() << "\n";
    OS << Op.toString() << "\n";
    assert(Op.getInput().size() <= EntryLayout.size());
    EntryLayout.resize(EntryLayout.size() - Op.getInput().size());
    EntryLayout.append(
        StackModel.getSlotsForInstructionDefs(Op.getMachineInstr()));
    OS << EntryLayout.toString() << "\n";
  }
  OS << "\n";
  OS << MBBExitLayoutMap.at(&Block).toString() << "\n";
  OS << "];\n";

  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(&Block);
  MBBExitType ExitType = TermInfo->getExitType();
  if (ExitType == MBBExitType::UnconditionalBranch) {
    auto [BranchInstr, Target] = TermInfo->getUnconditionalBranch();
    OS << "Block" << getBlockId(Block) << "Exit [label=\"";
    OS << "Jump\"];\n";
    OS << "Block" << getBlockId(Block) << "Exit -> Block" << getBlockId(*Target)
       << ";\n";
  } else if (ExitType == MBBExitType::ConditionalBranch) {
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
    OS << StackModel.getStackSlot(*Condition)->toString();
    OS << "| { <0> Zero | <1> NonZero }}\"];\n";
    OS << "Block" << getBlockId(Block);
    OS << "Exit:0 -> Block" << getBlockId(*FalseBB) << ";\n";
    OS << "Block" << getBlockId(Block);
    OS << "Exit:1 -> Block" << getBlockId(*TrueBB) << ";\n";
  } else if (ExitType == MBBExitType::FunctionReturn) {
    OS << "Block" << getBlockId(Block) << "Exit [label=\"FunctionReturn["
       << MF.getName() << "]\"];\n";
    const MachineInstr &MI = Block.back();
    OS << "Return values: " << StackModel.getReturnArguments(MI).toString()
       << ";\n";
  } else if (ExitType == MBBExitType::Terminate) {
    OS << "Block" << getBlockId(Block) << "Exit [label=\"Terminated\"];\n";
  }
  OS << "\n";
}

std::string
EVMStackLayoutGenerator::getBlockId(const MachineBasicBlock &Block) {
  std::string Name =
      std::to_string(Block.getNumber()) + "." + std::string(Block.getName());
  if (auto It = BlockIds.find(&Block); It != BlockIds.end())
    return std::to_string(It->second) + "(" + Name + ")";

  size_t Id = BlockIds[&Block] = BlockCount++;
  return std::to_string(Id) + "(" + Name + ")";
}
#endif // NDEBUG
