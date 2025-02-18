//===--------------------- EVMStackSolver.cpp -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMStackSolver.h"
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

#define DEBUG_TYPE "evm-stack-solver"

namespace {

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
SmallVector<EVMStackSolver::StackTooDeep>
findStackTooDeep(const Stack &Source, const Stack &Target,
                 unsigned StackDepthLimit) {
  Stack CurrentStack = Source;
  SmallVector<EVMStackSolver::StackTooDeep> Errors;

  auto getVariableChoices = [](auto &&SlotRange) {
    SmallVector<Register> Result;
    for (auto const *Slot : SlotRange)
      if (auto const *RegSlot = dyn_cast<RegisterSlot>(Slot))
        if (!is_contained(Result, RegSlot->getReg()))
          Result.push_back(RegSlot->getReg());
    return Result;
  };

  calculateStack(
      CurrentStack, Target, StackDepthLimit,
      [&](unsigned I) {
        if (I > StackDepthLimit)
          Errors.emplace_back(EVMStackSolver::StackTooDeep{
              I - StackDepthLimit,
              getVariableChoices(take_back(CurrentStack, I + 1))});
      },
      [&](const StackSlot *Slot) {
        if (Slot->isRematerializable())
          return;

        if (auto Depth = offset(reverse(CurrentStack), Slot);
            Depth && *Depth >= StackDepthLimit)
          Errors.emplace_back(EVMStackSolver::StackTooDeep{
              *Depth - (StackDepthLimit - 1),
              getVariableChoices(take_back(CurrentStack, *Depth + 1))});
      },
      [&]() {});
  return Errors;
}

/// Returns the ideal stack to have before executing a machine instruction that
/// outputs \p InstDefs s.t. shuffling to \p AfterInst is cheap (excluding the
/// input of the instruction itself). If \p CompressStack is true,
/// rematerializable slots will not occur in the ideal stack, but rather be
/// generated during shuffling.
Stack calculateStackBeforeInst(const SmallVector<StackSlot *> &InstDefs,
                               const Stack &AfterInst, bool CompressStack,
                               unsigned StackDepthLimit) {
  // Determine the number of slots that have to be on stack before executing the
  // operation (excluding the inputs of the operation itself), i.e. slots that
  // cannot be rematerialized and that are not the instruction output.
  size_t BeforeInstSize = count_if(AfterInst, [&](const StackSlot *S) {
    return !is_contained(InstDefs, S) &&
           !(CompressStack && S->isRematerializable());
  });

  SmallVector<UnknownSlot> UnknownSlots;
  for (size_t Index = 0; Index < BeforeInstSize; ++Index)
    UnknownSlots.emplace_back(Index);

  // The symbolic layout directly after the operation has the form
  // PreviousSlot{0}, ..., PreviousSlot{n}, [output<0>], ..., [output<m>]
  Stack Tmp;
  for (auto &S : UnknownSlots)
    Tmp.push_back(&S);
  append_range(Tmp, InstDefs);

  // Shortcut for trivial case.
  if (Tmp.empty())
    return Stack{};

  EVMStackShuffler Shuffler(Tmp, AfterInst, StackDepthLimit);

  auto canSkipSlot = [&InstDefs, CompressStack](const StackSlot *Slot) {
    return count(InstDefs, Slot) ||
           (CompressStack && Slot->isRematerializable());
  };

  Shuffler.setIsCompatible(
      [&canSkipSlot](const StackSlot *CSlot, const StackSlot *TSlot) {
        return isa<UnknownSlot>(CSlot) ? !canSkipSlot(TSlot) : CSlot == TSlot;
      });

  Shuffler.setGetCurrentSignificantUses(
      [&canSkipSlot](const StackSlot *Slot, Stack &C, const Stack &T) {
        if (isa<UnknownSlot>(Slot))
          return 0;
        int CUses = -count(C, Slot);
        if (canSkipSlot(Slot))
          CUses = CUses + count(T, Slot);
        return CUses;
      });

  Shuffler.setGetTargetSignificantUses(
      [&canSkipSlot](const StackSlot *Slot, Stack &C, const Stack &T) {
        if (!canSkipSlot(Slot))
          return 0;
        int TUses = -count(C, Slot);
        if (canSkipSlot(Slot))
          TUses = TUses + count(T, Slot);
        return TUses;
      });

  Shuffler.setSwap([](size_t I, Stack &C) {
    assert(!isa<UnknownSlot>(C[C.size() - I - 1]) ||
           !isa<UnknownSlot>(C.back()));
  });

  Shuffler.shuffle();

  // Now we can construct the ideal layout before the operation.
  // "layout" has shuffled the PreviousSlot{x} to new places using minimal
  // operations to move the operation output in place. The resulting permutation
  // of the PreviousSlot yields the ideal positions of slots before the
  // operation, i.e. if PreviousSlot{2} is at a position at which Post contains
  // VariableSlot{"tmp"}, then we want the variable tmp in the slot at offset 2
  // in the layout before the operation.
  assert(Tmp.size() == AfterInst.size());
  SmallVector<StackSlot *> BeforeInst(AfterInst.size(), nullptr);
  for (unsigned Idx = 0; Idx < std::min(Tmp.size(), AfterInst.size()); ++Idx) {
    if (const auto *Slot = dyn_cast<UnknownSlot>(Tmp[Idx]))
      BeforeInst[Slot->getIndex()] = AfterInst[Idx];
  }

  // The tail of layout must have contained the operation outputs and will not
  // have been assigned slots in the last loop.
  while (!BeforeInst.empty() && !BeforeInst.back())
    BeforeInst.pop_back();

  assert(BeforeInst.size() == BeforeInstSize);
  assert(all_of(BeforeInst,
                [](const StackSlot *Slot) { return Slot != nullptr; }));

  return BeforeInst;
}

} // end anonymous namespace

/// Returns the number of operations required to transform stack \p Source to
/// \p Target.
size_t llvm::calculateStackTransformCost(Stack Source, Stack const &Target,
                                         unsigned StackDepthLimit) {
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

  calculateStack(Source, Target, StackDepthLimit, Swap, DupOrPush, Pop);
  return OpGas;
}

EVMStackSolver::EVMStackSolver(const MachineFunction &MF,
                               const MachineLoopInfo *MLI,
                               const EVMStackModel &StackModel,
                               const EVMMachineCFGInfo &CFGInfo)
    : MF(MF), MLI(MLI), StackModel(StackModel), CFGInfo(CFGInfo) {}

EVMMIRToStack EVMStackSolver::run() {
  runPropagation();
  LLVM_DEBUG({
    dbgs() << "************* Stack *************\n";
    dump(dbgs());
  });

  return EVMMIRToStack(MBBEntryMap, MBBExitMap, OperationEntryMap);
}

Stack EVMStackSolver::propagateStackThroughInst(const Stack &AfterInst,
                                                const Operation &Op,
                                                bool CompressStack) {
  // Enable aggressive stack compression for recursive calls.
  if (Op.isFunctionCall())
    // TODO: compress stack for recursive functions.
    CompressStack = false;

  const SmallVector<StackSlot *> InstDefs =
      StackModel.getSlotsForInstructionDefs(Op.getMachineInstr());
  const unsigned StackDepthLimit = StackModel.stackDepthLimit();

  // Determine the ideal permutation of the slots in ExitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // 'IdealStack + Operation.output' to ExitLayout is cheap.
  Stack BeforeInst = calculateStackBeforeInst(InstDefs, AfterInst,
                                              CompressStack, StackDepthLimit);

#ifndef NDEBUG
  // Make sure the resulting previous slots do not overlap with any assigned
  // variables.
  if (Op.isAssignment())
    for (auto *StackSlot : BeforeInst)
      if (const auto *RegSlot = dyn_cast<RegisterSlot>(StackSlot))
        assert(!Op.getMachineInstr()->definesRegister(RegSlot->getReg()));
#endif // NDEBUG

  // Since stack+Operation.output can be easily shuffled to ExitLayout, the
  // desired layout before the operation is stack+Operation.input;
  BeforeInst.append(Op.getInput());

  // Store the exact desired operation entry layout. The stored layout will be
  // recreated by the code transform before executing the operation. However,
  // this recreation can produce slots that can be freely generated or are
  // duplicated, i.e. we can compress the stack afterwards without causing
  // problems for code generation later.
  OperationEntryMap[&Op] = BeforeInst;

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!BeforeInst.empty()) {
    if (BeforeInst.back()->isRematerializable()) {
      BeforeInst.pop_back();
    } else if (auto Offset = offset(drop_begin(reverse(BeforeInst), 1),
                                    BeforeInst.back())) {
      if (*Offset + 2 < StackDepthLimit)
        BeforeInst.pop_back();
      else
        break;
    } else
      break;
  }

  return BeforeInst;
}

Stack EVMStackSolver::propagateStackThroughMBB(const Stack &ExitStack,
                                               const MachineBasicBlock *MBB,
                                               bool CompressStack) {
  Stack CurrentStack = ExitStack;
  for (const Operation &Op : reverse(StackModel.getOperations(MBB))) {
    Stack AfterInst =
        propagateStackThroughInst(CurrentStack, Op, CompressStack);
    if (!CompressStack &&
        !findStackTooDeep(AfterInst, CurrentStack, StackModel.stackDepthLimit())
             .empty())
      // If we had stack errors, run again with stack compression enabled.
      return propagateStackThroughMBB(ExitStack, MBB,
                                      /*CompressStack*/ true);
    CurrentStack = std::move(AfterInst);
  }
  return CurrentStack;
}

// Returns the number of junk slots to be prepended to \p Target for
// an optimal transition from \p Source to \p Target.
static size_t getOptimalNumberOfJunks(const Stack &Source, const Stack &Target,
                                      unsigned StackDepthLimit) {
  size_t BestCost =
      calculateStackTransformCost(Source, Target, StackDepthLimit);
  size_t BestNumJunk = 0;
  size_t MaxJunk = Source.size();
  for (size_t NumJunk = 1; NumJunk <= MaxJunk; ++NumJunk) {
    Stack JunkedTarget(NumJunk, EVMStackModel::getJunkSlot());
    JunkedTarget.append(Target);
    size_t Cost =
        calculateStackTransformCost(Source, JunkedTarget, StackDepthLimit);
    if (Cost < BestCost) {
      BestCost = Cost;
      BestNumJunk = NumJunk;
    }
  }
  return BestNumJunk;
}

void EVMStackSolver::runPropagation() {
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
      const MachineBasicBlock *MBB = *ToVisit.begin();
      ToVisit.pop_front();
      if (Visited.count(MBB))
        continue;

      if (std::optional<Stack> ExitStack =
              getExitStackOrStageDependencies(MBB, Visited, ToVisit)) {
        Visited.insert(MBB);
        MBBExitMap[MBB] = *ExitStack;
        MBBEntryMap[MBB] = propagateStackThroughMBB(*ExitStack, MBB);
        for (auto Pred : MBB->predecessors())
          ToVisit.emplace_back(Pred);
      }
    }

    // Revisit these blocks again.
    for (auto [Latch, Header] : Backedges) {
      const Stack &HeaderEntryStack = MBBEntryMap[Header];
      const Stack &LatchExitStack = MBBExitMap[Latch];
      if (all_of(HeaderEntryStack, [LatchExitStack](const StackSlot *Slot) {
            return is_contained(LatchExitStack, Slot);
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

    Stack ExitStack = MBBExitMap.at(&MBB);

#ifndef NDEBUG
    // The last block must have produced the condition at the stack top.
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    assert(ExitStack.back() == StackModel.getStackSlot(*Condition));
#endif // NDEBUG

    // The condition is consumed by the conditional jump.
    ExitStack.pop_back();
    for (const MachineBasicBlock *Succ : MBB.successors()) {
      const Stack &SuccEntryStack = MBBEntryMap.at(Succ);
      Stack NewSuccEntryStack = ExitStack;
      // Whatever the block being jumped to does not actually require,
      // can be marked as junk.
      for (StackSlot *&Slot : NewSuccEntryStack)
        if (!is_contained(SuccEntryStack, Slot))
          Slot = EVMStackModel::getJunkSlot();

#ifndef NDEBUG
      // Make sure everything the block being jumped to requires is
      // actually present or can be generated.
      for (const StackSlot *Slot : SuccEntryStack)
        assert(Slot->isRematerializable() ||
               is_contained(NewSuccEntryStack, Slot));
#endif // NDEBUG

      MBBEntryMap[Succ] = NewSuccEntryStack;
    }
  }

  // Create the function entry stack.
  Stack EntryStack;
  bool IsNoReturn = MF.getFunction().hasFnAttribute(Attribute::NoReturn);
  if (!IsNoReturn)
    EntryStack.push_back(StackModel.getFunctionReturnLabelSlot(&MF));

  // Calling convention: input arguments are passed in stack such that the
  // first one specified in the function declaration is passed on the stack TOP.
  EntryStack.append(StackModel.getFunctionParameters());
  std::reverse(IsNoReturn ? EntryStack.begin() : std::next(EntryStack.begin()),
               EntryStack.end());
  MBBEntryMap[&MF.front()] = std::move(EntryStack);

  // Traverse the CFG and at each block that allows junk, i.e. that is a
  // cut-vertex that never leads to a function return, checks if adding junk
  // reduces the shuffling cost upon entering and if so recursively adds junk
  // to the spanned subgraph. This is needed only for optimization purposes,
  // not for correctness.
  for (const MachineBasicBlock &MBB : MF) {
    if (!CFGInfo.isCutVertex(&MBB) || CFGInfo.isOnPathToFuncReturn(&MBB))
      continue;

    const Stack EntryStack = MBBEntryMap.at(&MBB);
    const Stack &ExitStack = MBBExitMap.at(&MBB);
    const SmallVector<Operation> &Ops = StackModel.getOperations(&MBB);
    const Stack &Next =
        Ops.empty() ? ExitStack : OperationEntryMap.at(&Ops.front());
    if (EntryStack != Next) {
      size_t OptimalNumJunks = getOptimalNumberOfJunks(
          EntryStack, Next, StackModel.stackDepthLimit());
      if (OptimalNumJunks > 0) {
        appendJunks(&MBB, OptimalNumJunks);
        MBBEntryMap[&MBB] = EntryStack;
      }
    }
  }
}

std::optional<Stack> EVMStackSolver::getExitStackOrStageDependencies(
    const MachineBasicBlock *MBB,
    const DenseSet<const MachineBasicBlock *> &Visited,
    std::deque<const MachineBasicBlock *> &ToVisit) const {
  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(MBB);
  MBBExitType ExitType = TermInfo->getExitType();
  if (ExitType == MBBExitType::UnconditionalBranch) {
    auto [_, Target] = TermInfo->getUnconditionalBranch();
    if (MachineLoop *ML = MLI->getLoopFor(MBB); ML && ML->isLoopLatch(MBB)) {
      // Choose the best currently known entry stack of the jump target
      // as initial exit. Note that this may not yet be the final
      // stack state.
      auto It = MBBEntryMap.find(Target);
      return It == MBBEntryMap.end() ? Stack{} : It->second;
    }
    // If the current iteration has already visited the jump target,
    // start from its entry stack.
    if (Visited.count(Target))
      return MBBEntryMap.at(Target);
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
      Stack CombinedStack =
          combineStack(MBBEntryMap.at(FalseBB), MBBEntryMap.at(TrueBB),
                       StackModel.stackDepthLimit());
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
  if (ExitType == MBBExitType::FunctionReturn)
    return StackModel.getReturnArguments(MBB->back());

  return Stack{};
}

Stack EVMStackSolver::combineStack(const Stack &Stack1, const Stack &Stack2,
                                   unsigned StackDepthLimit) {
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
    calculateStack(TestStack, Stack1Tail, StackDepthLimit, Swap, DupOrPush,
                   [&]() {});
    TestStack = Candidate;
    calculateStack(TestStack, Stack2Tail, StackDepthLimit, Swap, DupOrPush,
                   [&]() {});
    return NumOps;
  };

  // See https://en.wikipedia.org/wiki/Heap's_algorithm.
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

Stack EVMStackSolver::compressStack(Stack CurStack, unsigned StackDepthLimit) {
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

void EVMStackSolver::appendJunks(const MachineBasicBlock *Entry,
                                 size_t NumJunk) {
  for (const MachineBasicBlock *MBB : depth_first(Entry)) {
    Stack EntryTmp(NumJunk, EVMStackModel::getJunkSlot());
    EntryTmp.append(MBBEntryMap.at(MBB));
    MBBEntryMap[MBB] = std::move(EntryTmp);

    for (const Operation &Op : StackModel.getOperations(MBB)) {
      Stack OpEntryTmp(NumJunk, EVMStackModel::getJunkSlot());
      OpEntryTmp.append(OperationEntryMap.at(&Op));
      OperationEntryMap[&Op] = std::move(OpEntryTmp);
    }

    Stack ExitTmp(NumJunk, EVMStackModel::getJunkSlot());
    ExitTmp.append(MBBExitMap.at(MBB));
    MBBExitMap[MBB] = std::move(ExitTmp);
  }
}

#ifndef NDEBUG
void EVMStackSolver::dump(raw_ostream &OS) {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot *ParamSlot : StackModel.getFunctionParameters()) {
    if (isa<JunkSlot>(ParamSlot))
      OS << "[unused param] ";
    else
      OS << ParamSlot->toString();
  }
  OS << ");\n";
  OS << "FunctionEntry "
     << " -> Block" << getBlockId(&MF.front()) << ";\n";

  for (const auto &MBB : MF)
    printMBB(OS, &MBB);
}

void EVMStackSolver::printMBB(raw_ostream &OS, const MachineBasicBlock *MBB) {
  std::string MBBId = getBlockId(MBB);
  OS << "Block" << MBBId << " [\n";
  OS << MBBEntryMap.at(MBB).toString() << "\n";
  for (auto const &Op : StackModel.getOperations(MBB)) {
    OS << "\n";
    Stack EntryStack = OperationEntryMap.at(&Op);
    OS << EntryStack.toString() << "\n";
    OS << Op.toString() << "\n";
    assert(Op.getInput().size() <= EntryStack.size());
    EntryStack.resize(EntryStack.size() - Op.getInput().size());
    EntryStack.append(
        StackModel.getSlotsForInstructionDefs(Op.getMachineInstr()));
    OS << EntryStack.toString() << "\n";
  }
  OS << "\n";
  OS << MBBExitMap.at(MBB).toString() << "\n";
  OS << "];\n";

  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(MBB);
  switch (TermInfo->getExitType()) {
  case MBBExitType::UnconditionalBranch: {
    auto [BranchInstr, Target] = TermInfo->getUnconditionalBranch();
    OS << "Block" << MBBId << "Exit [label=\"Jump\"];\n";
    OS << "Block" << MBBId << "Exit -> Block" << getBlockId(Target) << ";\n";
  } break;
  case MBBExitType::ConditionalBranch: {
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    OS << "Block" << MBBId << "Exit [label=\"{ "
       << StackModel.getStackSlot(*Condition)->toString()
       << "| { <0> Zero | <1> NonZero }}\"];\n";
    OS << "Block" << MBBId << "Exit:0 -> Block" << getBlockId(FalseBB) << ";\n";
    OS << "Block" << MBBId << "Exit:1 -> Block" << getBlockId(TrueBB) << ";\n";
  } break;
  case MBBExitType::FunctionReturn: {
    OS << "Block" << MBBId << "Exit [label=\"FunctionReturn[" << MF.getName()
       << "]\"];\n";
    const MachineInstr &MI = MBB->back();
    OS << "Return values: " << StackModel.getReturnArguments(MI).toString()
       << ";\n";
  } break;
  case MBBExitType::Terminate: {
    OS << "Block" << MBBId << "Exit [label=\"Terminated\"];\n";
  } break;
  default:
    break;
  }
  OS << "\n";
}

std::string EVMStackSolver::getBlockId(const MachineBasicBlock *MBB) {
  std::string Name =
      std::to_string(MBB->getNumber()) + "." + std::string(MBB->getName());
  if (auto It = BlockIds.find(MBB); It != BlockIds.end())
    return std::to_string(It->second) + "(" + Name + ")";

  size_t Id = BlockIds[MBB] = BlockCount++;
  return std::to_string(Id) + "(" + Name + ")";
}
#endif // NDEBUG
