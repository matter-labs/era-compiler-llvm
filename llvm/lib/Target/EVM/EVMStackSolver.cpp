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
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>

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

/// Returns true if there are unreachable stack elements when shuffling
/// \p Source to \p Target.
bool hasUnreachableStackSlots(const Stack &Source, const Stack &Target,
                              unsigned StackDepthLimit) {
  Stack CurrentStack = Source;
  bool HasError = false;
  calculateStack(
      CurrentStack, Target, StackDepthLimit,
      [&](unsigned I) {
        if (I > StackDepthLimit)
          HasError = true;
      },
      [&](const StackSlot *Slot) {
        if (Slot->isRematerializable())
          return;

        if (std::optional<size_t> Depth = offset(reverse(CurrentStack), Slot);
            Depth && *Depth >= StackDepthLimit)
          HasError = true;
      },
      [&]() {});
  return HasError;
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
  // UnknownSlot{0}, ..., UnknownSlot{n}, [output<0>], ..., [output<m>]
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
  // "layout" has shuffled the UnknownSlot{x} to new places using minimal
  // operations to move the operation output in place. The resulting permutation
  // of the UnknownSlot yields the ideal positions of slots before the
  // operation, i.e. if UnknownSlot{2} is at a position at which Post contains
  // VariableSlot{"tmp"}, then we want the variable tmp in the slot at offset 2
  // in the layout before the operation.
  assert(Tmp.size() == AfterInst.size());
  SmallVector<StackSlot *> BeforeInst(BeforeInstSize, nullptr);
  for (unsigned Idx = 0; Idx < Tmp.size(); ++Idx) {
    if (const auto *Slot = dyn_cast<UnknownSlot>(Tmp[Idx]))
      BeforeInst[Slot->getIndex()] = AfterInst[Idx];
  }
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
                               EVMStackModel &StackModel,
                               const MachineLoopInfo *MLI,
                               const EVMMachineCFGInfo &CFGInfo)
    : MF(MF), StackModel(StackModel), MLI(MLI), CFGInfo(CFGInfo) {}

void EVMStackSolver::run() {
  runPropagation();
  LLVM_DEBUG({
    dbgs() << "************* Stack *************\n";
    dump(dbgs());
  });
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

  // Determine the ideal permutation of the slots in ExitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // 'IdealStack + Operation.output' to ExitLayout is cheap.
  Stack BeforeInst = calculateStackBeforeInst(
      InstDefs, AfterInst, CompressStack, StackModel.stackDepthLimit());

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
  insertInstEntryStack(&Op, BeforeInst);

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!BeforeInst.empty()) {
    if (BeforeInst.back()->isRematerializable()) {
      BeforeInst.pop_back();
    } else if (auto Offset = offset(drop_begin(reverse(BeforeInst), 1),
                                    BeforeInst.back())) {
      if (*Offset + 2 < StackModel.stackDepthLimit())
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
        hasUnreachableStackSlots(AfterInst, CurrentStack,
                                 StackModel.stackDepthLimit()))
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
    // First calculate stack without walking backwards jumps, i.e.
    // assuming the current preliminary entry stack of the backwards jump
    // target as the initial exit stack of the backwards-jumping block.
    while (!ToVisit.empty()) {
      const MachineBasicBlock *MBB = *ToVisit.begin();
      ToVisit.pop_front();
      if (Visited.count(MBB))
        continue;

      // Get the MBB exit stack.
      std::optional<Stack> ExitStack = std::nullopt;
      const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(MBB);
      MBBExitType ExitType = TermInfo->getExitType();
      if (ExitType == MBBExitType::UnconditionalBranch) {
        const MachineBasicBlock *Target = MBB->getSingleSuccessor();
        if (MachineLoop *ML = MLI->getLoopFor(MBB);
            ML && ML->isLoopLatch(MBB)) {
          // Choose the best currently known entry stack of the jump target
          // as initial exit. Note that this may not yet be the final
          // stack.
          auto It = StackModel.getMBBEntryMap().find(Target);
          ExitStack =
              (It == StackModel.getMBBEntryMap().end() ? Stack{} : It->second);
        } else {
          // If the current iteration has already visited the jump target,
          // start from its entry stack. Otherwise stage the jump target
          // for visit and defer the current block.
          if (Visited.count(Target))
            ExitStack = StackModel.getMBBEntryStack(Target);
          else
            ToVisit.push_front(Target);
        }
      } else if (ExitType == MBBExitType::ConditionalBranch) {
        auto [CondBr, UncondBr, TBB, FBB, Condition] =
            TermInfo->getConditionalBranch();
        bool FBBVisited = Visited.count(FBB);
        bool TBBVisited = Visited.count(TBB);

        // If one of the jump targets has not been visited, stage it for
        // visit and defer the current block.
        // TODO: CPR-1847. Investigate how the order in which successors are put
        // into the deque affects the generated code.
        if (!FBBVisited)
          ToVisit.push_front(FBB);

        if (!TBBVisited)
          ToVisit.push_front(TBB);

        // If we have visited both successors, start from their entry stacks.
        if (FBBVisited && TBBVisited) {
          Stack CombinedStack = combineStack(StackModel.getMBBEntryStack(FBB),
                                             StackModel.getMBBEntryStack(TBB));
          // Additionally, the jump condition has to be at the stack top at
          // exit.
          CombinedStack.emplace_back(StackModel.getStackSlot(*Condition));
          ExitStack = std::move(CombinedStack);
        }
      } else if (ExitType == MBBExitType::FunctionReturn) {
        ExitStack = StackModel.getReturnArguments(MBB->back());
      } else
        ExitStack = Stack{};

      // If the MBB exit stack is known, we can back back propagate
      // it till the MBB entry.
      if (ExitStack) {
        Visited.insert(MBB);
        insertMBBExitStack(MBB, *ExitStack);
        insertMBBEntryStack(MBB, propagateStackThroughMBB(*ExitStack, MBB));
        append_range(ToVisit, MBB->predecessors());
      }
    }

    // Revisit these blocks again.
    for (auto [Latch, Header] : Backedges) {
      const Stack &HeaderEntryStack = StackModel.getMBBEntryStack(Header);
      const Stack &LatchExitStack = StackModel.getMBBExitStack(Latch);
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

  // At this point stacks at conditional jumps are merely
  // compatible, i.e. the exit layout of the jumping block is a superset of the
  // entry layout of the target block. We need to modify the entry stacks
  // of conditional jump targets, s.t., the entry layout of target blocks match
  // the exit layout of the jumping block exactly, except that slots not
  // required after the jump are marked as 'JunkSlot'.
  for (const MachineBasicBlock &MBB : MF) {
    const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(&MBB);
    if (TermInfo->getExitType() != MBBExitType::ConditionalBranch)
      continue;

    Stack ExitStack = StackModel.getMBBExitStack(&MBB);

#ifndef NDEBUG
    // The last block must have produced the condition at the stack top.
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    assert(ExitStack.back() == StackModel.getStackSlot(*Condition));
#endif // NDEBUG

    // The condition is consumed by the conditional jump.
    ExitStack.pop_back();
    for (const MachineBasicBlock *Succ : MBB.successors()) {
      const Stack &SuccEntryStack = StackModel.getMBBEntryStack(Succ);
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

      insertMBBEntryStack(Succ, NewSuccEntryStack);
    }
  }

  // Create the function entry stack.
  Stack EntryStack;
  if (!MF.getFunction().hasFnAttribute(Attribute::NoReturn))
    EntryStack.push_back(StackModel.getFunctionReturnLabelSlot(&MF));

  // Calling convention: input arguments are passed in stack such that the
  // first one specified in the function declaration is passed on the stack TOP.
  append_range(EntryStack, reverse(StackModel.getFunctionParameters()));
  insertMBBEntryStack(&MF.front(), EntryStack);

  // Traverse the CFG and at each block that allows junk, i.e. that is a
  // cut-vertex that never leads to a function return, checks if adding junk
  // reduces the shuffling cost upon entering and if so recursively adds junk
  // to the spanned subgraph. This is needed only for optimization purposes,
  // not for correctness.
  for (const MachineBasicBlock &MBB : MF) {
    if (!CFGInfo.isCutVertex(&MBB) || CFGInfo.isOnPathToFuncReturn(&MBB))
      continue;

    const Stack EntryStack = StackModel.getMBBEntryStack(&MBB);
    const Stack &ExitStack = StackModel.getMBBExitStack(&MBB);
    const SmallVector<Operation> &Ops = StackModel.getOperations(&MBB);
    const Stack &Next =
        Ops.empty() ? ExitStack : StackModel.getInstEntryStack(&Ops.front());

    if (EntryStack == Next)
      continue;

    size_t JunksNum =
        getOptimalNumberOfJunks(EntryStack, Next, StackModel.stackDepthLimit());

    if (!JunksNum)
      continue;

    for (const MachineBasicBlock *Succ : depth_first(&MBB)) {
      Stack JunkedEntry(JunksNum, EVMStackModel::getJunkSlot());
      JunkedEntry.append(StackModel.getMBBEntryStack(Succ));
      insertMBBEntryStack(Succ, std::move(JunkedEntry));

      for (const Operation &Op : StackModel.getOperations(Succ)) {
        Stack JunkedOpEntry(JunksNum, EVMStackModel::getJunkSlot());
        JunkedOpEntry.append(StackModel.getInstEntryStack(&Op));
        insertInstEntryStack(&Op, std::move(JunkedOpEntry));
      }

      Stack JunkedExit(JunksNum, EVMStackModel::getJunkSlot());
      JunkedExit.append(StackModel.getMBBExitStack(Succ));
      insertMBBExitStack(Succ, std::move(JunkedExit));
    }
    insertMBBEntryStack(&MBB, EntryStack);
  }
}

Stack EVMStackSolver::combineStack(const Stack &Stack1, const Stack &Stack2) {
  // TODO: it would be nicer to replace this by a constructive algorithm.
  // Currently it uses a reduced version of the Heap Algorithm to partly
  // brute-force, which seems to work decently well.

  Stack CommonPrefix;
  for (const auto [S1, S2] : zip(Stack1, Stack2)) {
    if (S1 != S2)
      break;
    CommonPrefix.push_back(S1);
  }

  Stack Stack1Tail, Stack2Tail;
  Stack1Tail.append(Stack1.begin() + CommonPrefix.size(), Stack1.end());
  Stack2Tail.append(Stack2.begin() + CommonPrefix.size(), Stack2.end());

  if (Stack1Tail.empty()) {
    CommonPrefix.append(compressStack(Stack2Tail));
    return CommonPrefix;
  }

  if (Stack2Tail.empty()) {
    CommonPrefix.append(compressStack(Stack1Tail));
    return CommonPrefix;
  }

  Stack Candidate;
  for (auto *Slot : concat<StackSlot *>(Stack1Tail, Stack2Tail))
    if (!is_contained(Candidate, Slot) && !Slot->isRematerializable())
      Candidate.push_back(Slot);

  auto evaluate = [&](const Stack &Candidate) -> size_t {
    size_t NumOps = 0;
    Stack TestStack = Candidate;
    auto Swap = [&](unsigned SwapDepth) {
      ++NumOps;
      if (SwapDepth > StackModel.stackDepthLimit())
        NumOps += 1000;
    };

    auto DupOrPush = [&](const StackSlot *Slot) {
      if (Slot->isRematerializable())
        return;

      Stack Tmp = CommonPrefix;
      Tmp.append(TestStack);
      auto Depth = offset(reverse(Tmp), Slot);
      if (Depth && *Depth >= StackModel.stackDepthLimit())
        NumOps += 1000;
    };
    calculateStack(TestStack, Stack1Tail, StackModel.stackDepthLimit(), Swap,
                   DupOrPush, [&]() {});
    TestStack = Candidate;
    calculateStack(TestStack, Stack2Tail, StackModel.stackDepthLimit(), Swap,
                   DupOrPush, [&]() {});
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

Stack EVMStackSolver::compressStack(Stack CurStack) {
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
        if (Depth + *DupDepth <= StackModel.stackDepthLimit()) {
          FirstDupOffset = CurStack.size() - Depth - 1;
          break;
        }
      }
    }
  } while (FirstDupOffset);
  return CurStack;
}

#ifndef NDEBUG
static std::string getMBBId(const MachineBasicBlock *MBB) {
  return std::to_string(MBB->getNumber()) + "." + std::string(MBB->getName());
}

void EVMStackSolver::dump(raw_ostream &OS) {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot *ParamSlot : StackModel.getFunctionParameters()) {
    if (isa<JunkSlot>(ParamSlot))
      OS << "[unused param] ";
    else
      OS << ParamSlot->toString();
  }
  OS << ");\n";
  OS << "Entry MBB: " << getMBBId(&MF.front()) << ";\n";

  for (const MachineBasicBlock &MBB : MF)
    dumpMBB(OS, &MBB);
}

void EVMStackSolver::dumpMBB(raw_ostream &OS, const MachineBasicBlock *MBB) {
  OS << getMBBId(MBB) << ":\n";
  OS << '\t' << StackModel.getMBBEntryStack(MBB).toString() << '\n';
  for (const Operation &Op : StackModel.getOperations(MBB)) {
    const Stack &OpOutput =
        StackModel.getSlotsForInstructionDefs(Op.getMachineInstr());
    if (Op.isAssignment() && Op.getInput() == OpOutput)
      continue;

    OS << '\n';
    Stack OpEntry = StackModel.getInstEntryStack(&Op);
    OS << '\t' << OpEntry.toString() << '\n';
    OS << '\t' << Op.toString() << '\n';
    assert(Op.getInput().size() <= OpEntry.size());
    OpEntry.resize(OpEntry.size() - Op.getInput().size());
    OpEntry.append(OpOutput);
    OS << '\t' << OpEntry.toString() << "\n";
  }
  OS << '\n';
  OS << '\t' << StackModel.getMBBExitStack(MBB).toString() << "\n";

  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(MBB);
  switch (TermInfo->getExitType()) {
  case MBBExitType::UnconditionalBranch: {
    auto [BranchInstr, Target] = TermInfo->getUnconditionalBranch();
    OS << "Exit type: unconditional branch\n";
    OS << "Target: " << getMBBId(Target) << '\n';
  } break;
  case MBBExitType::ConditionalBranch: {
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    OS << "Exit type: conditional branch, ";
    OS << "cond: " << StackModel.getStackSlot(*Condition)->toString() << '\n';
    OS << "False branch: " << getMBBId(FalseBB) << '\n';
    OS << "True branch: " << getMBBId(TrueBB) << '\n';
  } break;
  case MBBExitType::FunctionReturn: {
    OS << "Exit type: function return, " << MF.getName() << '\n';
    const MachineInstr &MI = MBB->back();
    OS << "Return values: " << StackModel.getReturnArguments(MI).toString()
       << '\n';
  } break;
  case MBBExitType::Terminate:
    OS << "Exit type: terminate\n";
    break;
  default:
    break;
  }
  OS << '\n';
}
#endif // NDEBUG
