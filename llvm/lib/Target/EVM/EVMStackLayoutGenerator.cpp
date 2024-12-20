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
#include "EVMHelperUtilities.h"
#include "EVMRegisterInfo.h"
#include "EVMStackDebug.h"
#include "EVMStackShuffler.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-stack-layout-generator"

namespace {
/// Returns all stack too deep errors that would occur when shuffling \p Source
/// to \p Target.
SmallVector<EVMStackLayoutGenerator::StackTooDeep>
findStackTooDeep(Stack const &Source, Stack const &Target) {
  Stack CurrentStack = Source;
  SmallVector<EVMStackLayoutGenerator::StackTooDeep> Errors;

  auto getVariableChoices = [](auto &&SlotRange) {
    SmallVector<Register> Result;
    for (auto const &Slot : SlotRange)
      if (auto const *VarSlot = std::get_if<VariableSlot>(&Slot))
        if (!is_contained(Result, VarSlot->VirtualReg))
          Result.push_back(VarSlot->VirtualReg);
    return Result;
  };

  ::createStackLayout(
      CurrentStack, Target,
      [&](unsigned I) {
        if (I > 16)
          Errors.emplace_back(EVMStackLayoutGenerator::StackTooDeep{
              I - 16,
              getVariableChoices(EVMUtils::take_back(CurrentStack, I + 1))});
      },
      [&](StackSlot const &Slot) {
        if (isRematerializable(Slot))
          return;

        if (auto Depth = EVMUtils::offset(reverse(CurrentStack), Slot);
            Depth && *Depth >= 16)
          Errors.emplace_back(EVMStackLayoutGenerator::StackTooDeep{
              *Depth - 15, getVariableChoices(
                               EVMUtils::take_back(CurrentStack, *Depth + 1))});
      },
      [&]() {});
  return Errors;
}

/// Returns the ideal stack to have before executing an operation that outputs
/// \p OperationOutput, s.t. shuffling to \p Post is cheap (excluding the
/// input of the operation itself). If \p GenerateSlotOnTheFly returns true for
/// a slot, this slot should not occur in the ideal stack, but rather be
/// generated on the fly during shuffling.
template <typename Callable>
Stack createIdealLayout(const Stack &OperationOutput, const Stack &Post,
                        Callable GenerateSlotOnTheFly) {
  struct PreviousSlot {
    size_t slot;
  };
  using LayoutT = SmallVector<std::variant<PreviousSlot, StackSlot>>;

  // Determine the number of slots that have to be on stack before executing the
  // operation (excluding the inputs of the operation itself). That is slots
  // that should not be generated on the fly and are not outputs of the
  // operation.
  size_t PreOperationLayoutSize = Post.size();
  for (auto const &Slot : Post)
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
    std::set<StackSlot> Outputs;
    Multiplicity Mult;
    Callable GenerateSlotOnTheFly;
    ShuffleOperations(LayoutT &Layout, Stack const &Post,
                      Callable GenerateSlotOnTheFly)
        : Layout(Layout), Post(Post),
          GenerateSlotOnTheFly(GenerateSlotOnTheFly) {
      for (auto const &LayoutSlot : Layout)
        if (const StackSlot *Slot = std::get_if<StackSlot>(&LayoutSlot))
          Outputs.insert(*Slot);

      for (auto const &LayoutSlot : Layout)
        if (const StackSlot *Slot = std::get_if<StackSlot>(&LayoutSlot))
          --Mult[*Slot];

      for (auto &&Slot : Post)
        if (Outputs.count(Slot) || GenerateSlotOnTheFly(Slot))
          ++Mult[Slot];
    }

    bool isCompatible(size_t Source, size_t Target) {
      return Source < Layout.size() && Target < Post.size() &&
             (std::holds_alternative<JunkSlot>(Post[Target]) ||
              std::visit(Overload{[&](const PreviousSlot &) {
                                    return !Outputs.count(Post[Target]) &&
                                           !GenerateSlotOnTheFly(Post[Target]);
                                  },
                                  [&](const StackSlot &S) {
                                    return S == Post[Target];
                                  }},
                         Layout[Source]));
    }

    bool sourceIsSame(size_t Lhs, size_t Rhs) {
      return std::visit(
          Overload{
              [&](PreviousSlot const &, PreviousSlot const &) { return true; },
              [&](StackSlot const &Lhs, StackSlot const &Rhs) {
                return Lhs == Rhs;
              },
              [&](auto const &, auto const &) { return false; }},
          Layout[Lhs], Layout[Rhs]);
    }

    int sourceMultiplicity(size_t Offset) {
      return std::visit(
          Overload{[&](PreviousSlot const &) { return 0; },
                   [&](StackSlot const &S) { return Mult.at(S); }},
          Layout[Offset]);
    }

    int targetMultiplicity(size_t Offset) {
      if (!Outputs.count(Post[Offset]) && !GenerateSlotOnTheFly(Post[Offset]))
        return 0;
      return Mult.at(Post[Offset]);
    }

    bool targetIsArbitrary(size_t Offset) {
      return Offset < Post.size() &&
             std::holds_alternative<JunkSlot>(Post[Offset]);
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
  SmallVector<std::optional<StackSlot>> IdealLayout(Post.size(), std::nullopt);
  for (unsigned Idx = 0; Idx < std::min(Layout.size(), Post.size()); ++Idx) {
    auto &Slot = Post[Idx];
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
  for (const auto &Item : IdealLayout) {
    assert(Item);
    Result.emplace_back(*Item);
  }

  return Result;
}

} // end anonymous namespace

EVMStackLayoutGenerator::EVMStackLayoutGenerator(
    const MachineFunction &MF, const EVMStackModel &StackModel,
    const EVMMachineCFGInfo &CFGInfo)
    : MF(MF), StackModel(StackModel), CFGInfo(CFGInfo) {}

std::unique_ptr<EVMStackLayout> EVMStackLayoutGenerator::run() {
  processEntryPoint(&MF.front());

  auto Layout = std::make_unique<EVMStackLayout>(
      MBBEntryLayoutMap, MBBExitLayoutMap, OperationEntryLayoutMap);

  LLVM_DEBUG({
    dbgs() << "************* Stack Layout *************\n";
    StackLayoutPrinter P(dbgs(), MF, *Layout, CFGInfo, StackModel);
    P();
  });
  return Layout;
}

Stack EVMStackLayoutGenerator::propagateStackThroughOperation(
    Stack ExitStack, const Operation &Operation,
    bool AggressiveStackCompression) {
  // Enable aggressive stack compression for recursive calls.
  if (std::holds_alternative<FunctionCall>(Operation.Operation))
    // TODO: compress stack for recursive functions.
    AggressiveStackCompression = false;

  // This is a huge tradeoff between code size, gas cost and stack size.
  auto generateSlotOnTheFly = [&](StackSlot const &Slot) {
    return AggressiveStackCompression && isRematerializable(Slot);
  };

  // Determine the ideal permutation of the slots in ExitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // 'IdealStack + Operation.output' to ExitLayout is cheap.
  Stack IdealStack =
      createIdealLayout(Operation.Output, ExitStack, generateSlotOnTheFly);

  // Make sure the resulting previous slots do not overlap with any assignmed
  // variables.
  if (auto const *Assign = std::get_if<Assignment>(&Operation.Operation))
    for (auto &StackSlot : IdealStack)
      if (auto const *VarSlot = std::get_if<VariableSlot>(&StackSlot))
        assert(!is_contained(Assign->Variables, *VarSlot));

  // Since stack+Operation.output can be easily shuffled to ExitLayout, the
  // desired layout before the operation is stack+Operation.input;
  IdealStack.append(Operation.Input);

  // Store the exact desired operation entry layout. The stored layout will be
  // recreated by the code transform before executing the operation. However,
  // this recreation can produce slots that can be freely generated or are
  // duplicated, i.e. we can compress the stack afterwards without causing
  // problems for code generation later.
  OperationEntryLayoutMap[&Operation] = IdealStack;

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!IdealStack.empty()) {
    if (isRematerializable(IdealStack.back()))
      IdealStack.pop_back();
    else if (auto Offset = EVMUtils::offset(drop_begin(reverse(IdealStack), 1),
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
    Stack ExitStack, const MachineBasicBlock *Block,
    bool AggressiveStackCompression) {
  Stack CurrentStack = ExitStack;
  for (const Operation &Op : reverse(StackModel.getOperations(Block))) {
    Stack NewStack = propagateStackThroughOperation(CurrentStack, Op,
                                                    AggressiveStackCompression);
    if (!AggressiveStackCompression &&
        !findStackTooDeep(NewStack, CurrentStack).empty())
      // If we had stack errors, run again with aggressive stack compression.
      return propagateStackThroughBlock(std::move(ExitStack), Block, true);
    CurrentStack = std::move(NewStack);
  }
  return CurrentStack;
}

void EVMStackLayoutGenerator::processEntryPoint(
    const MachineBasicBlock *Entry) {
  std::list<const MachineBasicBlock *> ToVisit{Entry};
  DenseSet<const MachineBasicBlock *> Visited;

  // TODO: check whether visiting only a subset of these in the outer iteration
  // below is enough.
  std::list<std::pair<const MachineBasicBlock *, const MachineBasicBlock *>>
      BackwardsJumps = collectBackwardsJumps(Entry);
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

    // Determine which backwards jumps still require fixing and stage revisits
    // of appropriate nodes.
    for (auto [JumpingBlock, Target] : BackwardsJumps) {
      // This block jumps backwards, but does not provide all slots required by
      // the jump target on exit. Therefore we need to visit the subgraph
      // between 'Target' and 'JumpingBlock' again.
      const Stack &EntryLayout = MBBEntryLayoutMap[Target];
      auto B = EntryLayout.begin(), E = EntryLayout.end();
      const Stack &ExitLayout = MBBExitLayoutMap[JumpingBlock];
      if (std::any_of(B, E, [ExitLayout](const StackSlot &Slot) {
            return find(ExitLayout, Slot) == ExitLayout.end();
          })) {
        // In particular we can visit backwards starting from 'JumpingBlock'
        // and mark all entries to-be-visited again until we hit 'Target'.
        ToVisit.emplace_front(JumpingBlock);
        // Since we are likely to permute the entry layout of 'Target', we
        // also visit its entries again. This is not required for correctness,
        // since the set of stack slots will match, but it may move some
        // required stack shuffling from the loop condition to outside the loop.
        for (const MachineBasicBlock *Pred : Target->predecessors())
          Visited.erase(Pred);

        EVMUtils::BreadthFirstSearch<const MachineBasicBlock *>{{JumpingBlock}}
            .run([&Visited, Target = Target](const MachineBasicBlock *Block,
                                             auto VisitPred) {
              Visited.erase(Block);
              if (Block == Target)
                return;
              for (auto const *Pred : Block->predecessors())
                VisitPred(Pred);
            });
        // While the shuffled layout for 'Target' will be compatible, it can
        // be worthwhile propagating it further up once more. This would mean
        // not stopping at Block == Target above, resp. even doing
        // Visited.clear() here, revisiting the entire graph. This is a tradeoff
        // between the runtime of this process and the optimality of the result.
        // Also note that while visiting the entire graph again *can* be
        // helpful, it can also be detrimental.
      }
    }
  }

  stitchConditionalJumps(Entry);
  fillInJunk(Entry);

  // Create function entry layout.
  Stack EntryStack;
  bool IsNoReturn = MF.getFunction().hasFnAttribute(Attribute::NoReturn);
  if (!IsNoReturn)
    EntryStack.emplace_back(FunctionReturnLabelSlot{&MF});

  // Calling convention: input arguments are passed in stack such that the
  // first one specified in the function declaration is passed on the stack TOP.
  EntryStack.append(StackModel.getFunctionParameters());
  std::reverse(IsNoReturn ? EntryStack.begin() : std::next(EntryStack.begin()),
               EntryStack.end());
  MBBEntryLayoutMap[Entry] = std::move(EntryStack);
}

std::optional<Stack> EVMStackLayoutGenerator::getExitLayoutOrStageDependencies(
    const MachineBasicBlock *Block,
    const DenseSet<const MachineBasicBlock *> &Visited,
    std::list<const MachineBasicBlock *> &ToVisit) const {
  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(Block);
  MBBExitType ExitType = TermInfo->getExitType();
  if (ExitType == MBBExitType::UnconditionalBranch) {
    auto [_, Target, IsLatch] = TermInfo->getUnconditionalBranch();
    if (IsLatch) {
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

std::list<std::pair<const MachineBasicBlock *, const MachineBasicBlock *>>
EVMStackLayoutGenerator::collectBackwardsJumps(
    const MachineBasicBlock *Entry) const {
  std::list<std::pair<const MachineBasicBlock *, const MachineBasicBlock *>>
      BackwardsJumps;
  EVMUtils::BreadthFirstSearch<MachineBasicBlock const *>{{Entry}}.run(
      [&](const MachineBasicBlock *Block, auto VisitSucc) {
        const EVMMBBTerminatorsInfo *TermInfo =
            CFGInfo.getTerminatorsInfo(Block);
        MBBExitType ExitType = TermInfo->getExitType();
        if (ExitType == MBBExitType::UnconditionalBranch) {
          auto [_, Target, IsLatch] = TermInfo->getUnconditionalBranch();
          if (IsLatch)
            BackwardsJumps.emplace_back(Block, Target);
        }
        for (const MachineBasicBlock *Succ : Block->successors())
          VisitSucc(Succ);
      });
  return BackwardsJumps;
}

void EVMStackLayoutGenerator::stitchConditionalJumps(
    const MachineBasicBlock *Block) {
  EVMUtils::BreadthFirstSearch<MachineBasicBlock const *> BFS{{Block}};
  BFS.run([&](const MachineBasicBlock *Block, auto VisitSucc) {
    const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(Block);
    MBBExitType ExitType = TermInfo->getExitType();
    if (ExitType == MBBExitType::UnconditionalBranch) {
      auto [_, Target, IsLatch] = TermInfo->getUnconditionalBranch();
      if (!IsLatch)
        VisitSucc(Target);
      return;
    }
    if (ExitType == MBBExitType::ConditionalBranch) {
      auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
          TermInfo->getConditionalBranch();
      Stack ExitLayout = MBBExitLayoutMap.at(Block);
      // The last block must have produced the condition at the stack
      // top.
      assert(!ExitLayout.empty());
      assert(ExitLayout.back() == StackModel.getStackSlot(*Condition));
      // The condition is consumed by the jump.
      ExitLayout.pop_back();

      auto FixJumpTargetEntry = [&](const Stack &OriginalEntryLayout) -> Stack {
        Stack NewEntryLayout = ExitLayout;
        // Whatever the block being jumped to does not actually require,
        // can be marked as junk.
        for (StackSlot &Slot : NewEntryLayout) {
          if (find(OriginalEntryLayout, Slot) == OriginalEntryLayout.end())
            Slot = JunkSlot{};
        }
#ifndef NDEBUG
        // Make sure everything the block being jumped to requires is
        // actually present or can be generated.
        for (StackSlot const &Slot : OriginalEntryLayout)
          assert(isRematerializable(Slot) ||
                 find(NewEntryLayout, Slot) != NewEntryLayout.end());
#endif // NDEBUG
        return NewEntryLayout;
      };

      const Stack &ZeroTargetEntryLayout = MBBEntryLayoutMap.at(FalseBB);
      MBBEntryLayoutMap[FalseBB] = FixJumpTargetEntry(ZeroTargetEntryLayout);
      const Stack &NonZeroTargetEntryLayout = MBBEntryLayoutMap.at(TrueBB);
      MBBEntryLayoutMap[TrueBB] = FixJumpTargetEntry(NonZeroTargetEntryLayout);
      VisitSucc(FalseBB);
      VisitSucc(TrueBB);
    }
  });
}

Stack EVMStackLayoutGenerator::combineStack(Stack const &Stack1,
                                            Stack const &Stack2) {
  // TODO: it would be nicer to replace this by a constructive algorithm.
  // Currently it uses a reduced version of the Heap Algorithm to partly
  // brute-force, which seems to work decently well.

  Stack CommonPrefix;
  for (unsigned Idx = 0; Idx < std::min(Stack1.size(), Stack2.size()); ++Idx) {
    const StackSlot &Slot1 = Stack1[Idx];
    const StackSlot &Slot2 = Stack2[Idx];
    if (!(Slot1 == Slot2))
      break;
    CommonPrefix.emplace_back(Slot1);
  }

  Stack Stack1Tail, Stack2Tail;
  for (const auto &Slot : drop_begin(Stack1, CommonPrefix.size()))
    Stack1Tail.emplace_back(Slot);

  for (const auto &Slot : drop_begin(Stack2, CommonPrefix.size()))
    Stack2Tail.emplace_back(Slot);

  if (Stack1Tail.empty()) {
    CommonPrefix.append(compressStack(Stack2Tail));
    return CommonPrefix;
  }

  if (Stack2Tail.empty()) {
    CommonPrefix.append(compressStack(Stack1Tail));
    return CommonPrefix;
  }

  Stack Candidate;
  for (auto Slot : Stack1Tail)
    if (!is_contained(Candidate, Slot))
      Candidate.emplace_back(Slot);

  for (auto Slot : Stack2Tail)
    if (!is_contained(Candidate, Slot))
      Candidate.emplace_back(Slot);

  {
    auto RemIt = std::remove_if(
        Candidate.begin(), Candidate.end(), [](StackSlot const &Slot) {
          return std::holds_alternative<LiteralSlot>(Slot) ||
                 std::holds_alternative<SymbolSlot>(Slot) ||
                 std::holds_alternative<FunctionCallReturnLabelSlot>(Slot);
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

    auto DupOrPush = [&](StackSlot const &Slot) {
      if (isRematerializable(Slot))
        return;

      Stack Tmp = CommonPrefix;
      Tmp.append(TestStack);
      auto Depth = EVMUtils::offset(reverse(Tmp), Slot);
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
      StackSlot &Slot = *I;
      if (isRematerializable(Slot)) {
        FirstDupOffset = CurStack.size() - Depth - 1;
        break;
      }

      if (auto DupDepth = EVMUtils::offset(
              drop_begin(reverse(CurStack), Depth + 1), Slot)) {
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

  auto DupOrPush = [&](StackSlot const &Slot) {
    if (isRematerializable(Slot))
      OpGas += 3;
    else {
      auto Depth = EVMUtils::offset(reverse(Source), Slot);
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

void EVMStackLayoutGenerator::fillInJunk(const MachineBasicBlock *Block) {
  /// Recursively adds junk to the subgraph starting on \p Entry.
  /// Since it is only called on cut-vertices, the full subgraph retains proper
  /// stack balance.
  auto AddJunkRecursive = [&](const MachineBasicBlock *Entry, size_t NumJunk) {
    EVMUtils::BreadthFirstSearch<const MachineBasicBlock *> BFS{{Entry}};

    BFS.run([&](const MachineBasicBlock *Block, auto VisitSucc) {
      Stack EntryTmp(NumJunk, JunkSlot{});
      EntryTmp.append(MBBEntryLayoutMap.at(Block));
      MBBEntryLayoutMap[Block] = std::move(EntryTmp);

      for (const Operation &Operation : StackModel.getOperations(Block)) {
        Stack OpEntryTmp(NumJunk, JunkSlot{});
        OpEntryTmp.append(OperationEntryLayoutMap.at(&Operation));
        OperationEntryLayoutMap[&Operation] = std::move(OpEntryTmp);
      }

      Stack ExitTmp(NumJunk, JunkSlot{});
      ExitTmp.append(MBBExitLayoutMap.at(Block));
      MBBExitLayoutMap[Block] = std::move(ExitTmp);

      for (const MachineBasicBlock *Succ : Block->successors())
        VisitSucc(Succ);
    });
  };

  /// Returns the number of junk slots to be prepended to \p TargetLayout for
  /// an optimal transition from \p EntryLayout to \p TargetLayout.
  auto GetBestNumJunk = [&](const Stack &EntryLayout,
                            const Stack &TargetLayout) -> size_t {
    size_t BestCost = EvaluateStackTransform(EntryLayout, TargetLayout);
    size_t BestNumJunk = 0;
    size_t MaxJunk = EntryLayout.size();
    for (size_t NumJunk = 1; NumJunk <= MaxJunk; ++NumJunk) {
      Stack JunkedTarget(NumJunk, JunkSlot{});
      JunkedTarget.append(TargetLayout);
      size_t Cost = EvaluateStackTransform(EntryLayout, JunkedTarget);
      if (Cost < BestCost) {
        BestCost = Cost;
        BestNumJunk = NumJunk;
      }
    }
    return BestNumJunk;
  };

  if (MF.getFunction().hasFnAttribute(Attribute::NoReturn) &&
      (&MF.front() == Block)) {
    Stack Params = StackModel.getFunctionParameters();
    std::reverse(Params.begin(), Params.end());
    size_t BestNumJunk = GetBestNumJunk(Params, MBBEntryLayoutMap.at(Block));
    if (BestNumJunk > 0)
      AddJunkRecursive(Block, BestNumJunk);
  }
  /// Traverses the CFG and at each block that allows junk, i.e. that is a
  /// cut-vertex that never leads to a function return, checks if adding junk
  /// reduces the shuffling cost upon entering and if so recursively adds junk
  /// to the spanned subgraph.
  EVMUtils::BreadthFirstSearch<MachineBasicBlock const *>{{Block}}.run(
      [&](MachineBasicBlock const *Block, auto VisitSucc) {
        if (CFGInfo.isCutVertex(Block) &&
            !CFGInfo.isOnPathToFuncReturn(Block)) {
          const Stack EntryLayout = MBBEntryLayoutMap.at(Block);
          const Stack &ExitLayout = MBBExitLayoutMap.at(Block);
          const SmallVector<Operation> &Ops = StackModel.getOperations(Block);
          Stack const &NextLayout =
              Ops.empty() ? ExitLayout
                          : OperationEntryLayoutMap.at(&Ops.front());
          if (EntryLayout != NextLayout) {
            size_t BestNumJunk = GetBestNumJunk(EntryLayout, NextLayout);
            if (BestNumJunk > 0) {
              AddJunkRecursive(Block, BestNumJunk);
              MBBEntryLayoutMap[Block] = EntryLayout;
            }
          }
        }
        for (const MachineBasicBlock *Succ : Block->successors())
          VisitSucc(Succ);
      });
}
