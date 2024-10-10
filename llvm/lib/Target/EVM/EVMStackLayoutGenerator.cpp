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

StackLayout StackLayoutGenerator::run(const CFG &Cfg) {
  StackLayout Layout;
  StackLayoutGenerator LayoutGenerator{Layout, &Cfg.FuncInfo};
  LayoutGenerator.processEntryPoint(*Cfg.FuncInfo.Entry, &Cfg.FuncInfo);

  LLVM_DEBUG({
    dbgs() << "************* Stack Layout *************\n";
    StackLayoutPrinter P(dbgs(), Layout);
    P(Cfg.FuncInfo);
  });

  return Layout;
}

StackLayoutGenerator::StackLayoutGenerator(
    StackLayout &Layout, CFG::FunctionInfo const *FunctionInfo)
    : Layout(Layout), CurrentFunctionInfo(FunctionInfo) {}

namespace {

/// Returns all stack too deep errors that would occur when shuffling \p Source
/// to \p Target.
std::vector<StackLayoutGenerator::StackTooDeep>
findStackTooDeep(Stack const &Source, Stack const &Target) {
  Stack CurrentStack = Source;
  std::vector<StackLayoutGenerator::StackTooDeep> Errors;

  auto getVariableChoices = [](auto &&SlotRange) {
    std::vector<Register> Result;
    for (auto const &Slot : SlotRange)
      if (auto const *VarSlot = std::get_if<VariableSlot>(&Slot))
        if (!EVMUtils::contains(Result, VarSlot->VirtualReg))
          Result.push_back(VarSlot->VirtualReg);
    return Result;
  };

  ::createStackLayout(
      CurrentStack, Target,
      [&](unsigned I) {
        if (I > 16)
          Errors.emplace_back(StackLayoutGenerator::StackTooDeep{
              I - 16,
              getVariableChoices(EVMUtils::take_last(CurrentStack, I + 1))});
      },
      [&](StackSlot const &Slot) {
        if (canBeFreelyGenerated(Slot))
          return;

        if (auto depth =
                EVMUtils::findOffset(EVMUtils::get_reverse(CurrentStack), Slot);
            depth && *depth >= 16)
          Errors.emplace_back(StackLayoutGenerator::StackTooDeep{
              *depth - 15, getVariableChoices(
                               EVMUtils::take_last(CurrentStack, *depth + 1))});
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
  using LayoutT = std::vector<std::variant<PreviousSlot, StackSlot>>;

  // Determine the number of slots that have to be on stack before executing the
  // operation (excluding the inputs of the operation itself). That is slots
  // that should not be generated on the fly and are not outputs of the
  // operation.
  size_t PreOperationLayoutSize = Post.size();
  for (auto const &Slot : Post)
    if (EVMUtils::contains(OperationOutput, Slot) || GenerateSlotOnTheFly(Slot))
      --PreOperationLayoutSize;

  // The symbolic layout directly after the operation has the form
  // PreviousSlot{0}, ..., PreviousSlot{n}, [output<0>], ..., [output<m>]
  LayoutT Layout;
  for (size_t Index = 0; Index < PreOperationLayoutSize; ++Index)
    Layout.emplace_back(PreviousSlot{Index});
  Layout += OperationOutput;

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
             (std::holds_alternative<JunkSlot>(Post.at(Target)) ||
              std::visit(Overload{[&](const PreviousSlot &) {
                                    return !Outputs.count(Post.at(Target)) &&
                                           !GenerateSlotOnTheFly(
                                               Post.at(Target));
                                  },
                                  [&](const StackSlot &S) {
                                    return S == Post.at(Target);
                                  }},
                         Layout.at(Source)));
    }

    bool sourceIsSame(size_t Lhs, size_t Rhs) {
      return std::visit(
          Overload{
              [&](PreviousSlot const &, PreviousSlot const &) { return true; },
              [&](StackSlot const &Lhs, StackSlot const &Rhs) {
                return Lhs == Rhs;
              },
              [&](auto const &, auto const &) { return false; }},
          Layout.at(Lhs), Layout.at(Rhs));
    }

    int sourceMultiplicity(size_t Offset) {
      return std::visit(
          Overload{[&](PreviousSlot const &) { return 0; },
                   [&](StackSlot const &S) { return Mult.at(S); }},
          Layout.at(Offset));
    }

    int targetMultiplicity(size_t Offset) {
      if (!Outputs.count(Post.at(Offset)) &&
          !GenerateSlotOnTheFly(Post.at(Offset)))
        return 0;
      return Mult.at(Post.at(Offset));
    }

    bool targetIsArbitrary(size_t Offset) {
      return Offset < Post.size() &&
             std::holds_alternative<JunkSlot>(Post.at(Offset));
    }

    void swap(size_t I) {
      assert(!std::holds_alternative<PreviousSlot>(
                 Layout.at(Layout.size() - I - 1)) ||
             !std::holds_alternative<PreviousSlot>(Layout.back()));
      std::swap(Layout.at(Layout.size() - I - 1), Layout.back());
    }

    size_t sourceSize() { return Layout.size(); }

    size_t targetSize() { return Post.size(); }

    void pop() { Layout.pop_back(); }

    void pushOrDupTarget(size_t Offset) { Layout.push_back(Post.at(Offset)); }
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
  std::vector<std::optional<StackSlot>> IdealLayout(Post.size(), std::nullopt);
  for (unsigned Idx = 0; Idx < std::min(Layout.size(), Post.size()); ++Idx) {
    auto &Slot = Post[Idx];
    auto &IdealPosition = Layout[Idx];
    if (PreviousSlot *PrevSlot = std::get_if<PreviousSlot>(&IdealPosition))
      IdealLayout.at(PrevSlot->slot) = Slot;
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

Stack StackLayoutGenerator::propagateStackThroughOperation(
    Stack ExitStack, const CFG::Operation &Operation,
    bool AggressiveStackCompression) {
  // Enable aggressive stack compression for recursive calls.
  if (auto const *functionCall =
          std::get_if<CFG::FunctionCall>(&Operation.Operation))
    // TODO: compress stack for recursive functions.
    AggressiveStackCompression = false;

  // This is a huge tradeoff between code size, gas cost and stack size.
  auto generateSlotOnTheFly = [&](StackSlot const &Slot) {
    return AggressiveStackCompression && canBeFreelyGenerated(Slot);
  };

  // Determine the ideal permutation of the slots in ExitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // `IdealStack + Operation.output` to ExitLayout is cheap.
  Stack IdealStack =
      createIdealLayout(Operation.Output, ExitStack, generateSlotOnTheFly);

  // Make sure the resulting previous slots do not overlap with any assignmed
  // variables.
  if (auto const *Assignment =
          std::get_if<CFG::Assignment>(&Operation.Operation))
    for (auto &StackSlot : IdealStack)
      if (auto const *VarSlot = std::get_if<VariableSlot>(&StackSlot))
        assert(!EVMUtils::contains(Assignment->Variables, *VarSlot));

  // Since stack+Operation.output can be easily shuffled to _exitLayout, the
  // desired layout before the operation is stack+Operation.input;
  IdealStack += Operation.Input;

  // Store the exact desired operation entry layout. The stored layout will be
  // recreated by the code transform before executing the operation. However,
  // this recreation can produce slots that can be freely generated or are
  // duplicated, i.e. we can compress the stack afterwards without causing
  // problems for code generation later.
  Layout.operationEntryLayout[&Operation] = IdealStack;

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!IdealStack.empty()) {
    if (canBeFreelyGenerated(IdealStack.back()))
      IdealStack.pop_back();
    else if (auto Offset = EVMUtils::findOffset(
                 EVMUtils::drop_first(EVMUtils::get_reverse(IdealStack), 1),
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

Stack StackLayoutGenerator::propagateStackThroughBlock(
    Stack ExitStack, CFG::BasicBlock const &Block,
    bool AggressiveStackCompression) {
  Stack CurrentStack = ExitStack;
  for (auto &Operation : EVMUtils::get_reverse(Block.Operations)) {
    Stack NewStack = propagateStackThroughOperation(CurrentStack, Operation,
                                                    AggressiveStackCompression);
    if (!AggressiveStackCompression &&
        !findStackTooDeep(NewStack, CurrentStack).empty())
      // If we had stack errors, run again with aggressive stack compression.
      return propagateStackThroughBlock(std::move(ExitStack), Block, true);
    CurrentStack = std::move(NewStack);
  }
  return CurrentStack;
}

void StackLayoutGenerator::processEntryPoint(
    CFG::BasicBlock const &Entry, CFG::FunctionInfo const *FunctionInfo) {
  std::list<CFG::BasicBlock const *> ToVisit{&Entry};
  std::set<CFG::BasicBlock const *> Visited;

  // TODO: check whether visiting only a subset of these in the outer iteration
  // below is enough.
  std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
      BackwardsJumps = collectBackwardsJumps(Entry);
  while (!ToVisit.empty()) {
    // First calculate stack layouts without walking backwards jumps, i.e.
    // assuming the current preliminary entry layout of the backwards jump
    // target as the initial exit layout of the backwards-jumping block.
    while (!ToVisit.empty()) {
      CFG::BasicBlock const *Block = *ToVisit.begin();
      ToVisit.pop_front();

      if (Visited.count(Block))
        continue;

      if (std::optional<Stack> ExitLayout =
              getExitLayoutOrStageDependencies(*Block, Visited, ToVisit)) {
        Visited.emplace(Block);
        auto &Info = Layout.blockInfos[Block];
        Info.exitLayout = *ExitLayout;
        Info.entryLayout = propagateStackThroughBlock(Info.exitLayout, *Block);
        for (auto Entry : Block->Entries)
          ToVisit.emplace_back(Entry);
      }
    }

    // Determine which backwards jumps still require fixing and stage revisits
    // of appropriate nodes.
    for (auto [JumpingBlock, Target] : BackwardsJumps) {
      // This block jumps backwards, but does not provide all slots required by
      // the jump target on exit. Therefore we need to visit the subgraph
      // between ``Target`` and ``JumpingBlock`` again.
      auto StartIt = std::begin(Layout.blockInfos[Target].entryLayout);
      auto EndIt = std::end(Layout.blockInfos[Target].entryLayout);
      if (std::any_of(StartIt, EndIt,
                      [exitLayout = Layout.blockInfos[JumpingBlock].exitLayout](
                          StackSlot const &Slot) {
                        return !EVMUtils::contains(exitLayout, Slot);
                      })) {
        // In particular we can visit backwards starting from ``JumpingBlock``
        // and mark all entries to-be-visited again until we hit ``Target``.
        ToVisit.emplace_front(JumpingBlock);
        // Since we are likely to permute the entry layout of ``Target``, we
        // also visit its entries again. This is not required for correctness,
        // since the set of stack slots will match, but it may move some
        // required stack shuffling from the loop condition to outside the loop.
        for (CFG::BasicBlock const *Entry : Target->Entries)
          Visited.erase(Entry);

        EVMUtils::BreadthFirstSearch<const CFG::BasicBlock *>{{JumpingBlock}}
            .run([&Visited, Target = Target](const CFG::BasicBlock *Block,
                                             auto AddChild) {
              Visited.erase(Block);
              if (Block == Target)
                return;
              for (auto const *Entry : Block->Entries)
                AddChild(Entry);
            });
        // While the shuffled layout for ``Target`` will be compatible, it can
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
  fillInJunk(Entry, FunctionInfo);
}

std::optional<Stack> StackLayoutGenerator::getExitLayoutOrStageDependencies(
    const CFG::BasicBlock &Block,
    const std::set<CFG::BasicBlock const *> &Visited,
    std::list<CFG::BasicBlock const *> &ToVisit) const {
  return std::visit(
      Overload{
          [&](CFG::BasicBlock::Jump const &Jump) -> std::optional<Stack> {
            if (Jump.Backwards) {
              // Choose the best currently known entry layout of the jump target
              // as initial exit. Note that this may not yet be the final
              // layout.
              auto It = Layout.blockInfos.find(Jump.Target);
              if (It == Layout.blockInfos.end())
                return Stack{};

              return It->second.entryLayout;
            }
            // If the current iteration has already visited the jump target,
            // start from its entry layout.
            if (Visited.count(Jump.Target))
              return Layout.blockInfos.at(Jump.Target).entryLayout;
            // Otherwise stage the jump target for visit and defer the current
            // block.
            ToVisit.emplace_front(Jump.Target);
            return std::nullopt;
          },
          [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump)
              -> std::optional<Stack> {
            bool ZeroVisited = Visited.count(ConditionalJump.Zero);
            bool NonZeroVisited = Visited.count(ConditionalJump.NonZero);

            if (ZeroVisited && NonZeroVisited) {
              // If the current iteration has already Visited both jump targets,
              // start from its entry layout.
              Stack CombonedStack = combineStack(
                  Layout.blockInfos.at(ConditionalJump.Zero).entryLayout,
                  Layout.blockInfos.at(ConditionalJump.NonZero).entryLayout);
              // Additionally, the jump condition has to be at the stack top at
              // exit.
              CombonedStack.emplace_back(ConditionalJump.Condition);
              return CombonedStack;
            }

            // If one of the jump targets has not been visited, stage it for
            // visit and defer the current block.
            if (!ZeroVisited)
              ToVisit.emplace_front(ConditionalJump.Zero);

            if (!NonZeroVisited)
              ToVisit.emplace_front(ConditionalJump.NonZero);

            return std::nullopt;
          },
          [&](CFG::BasicBlock::FunctionReturn const &FunctionReturn)
              -> std::optional<Stack> {
            // A function return needs the return variables and the function
            // return label slot on stack.
            assert(FunctionReturn.Info);
            Stack ReturnStack = FunctionReturn.RetValues;
            ReturnStack.emplace_back(
                FunctionReturnLabelSlot{FunctionReturn.Info->MF});
            return ReturnStack;
          },
          [&](CFG::BasicBlock::Terminated const &) -> std::optional<Stack> {
            // A terminating block can have an empty stack on exit.
            return Stack{};
          },
          [](CFG::BasicBlock::InvalidExit const &) -> std::optional<Stack> {
            llvm_unreachable("Unexpected BB terminator");
          }},
      Block.Exit);
}

std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
StackLayoutGenerator::collectBackwardsJumps(
    CFG::BasicBlock const &Entry) const {
  std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
      BackwardsJumps;
  EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *>{{&Entry}}.run(
      [&](CFG::BasicBlock const *Block, auto AddChild) {
        std::visit(
            Overload{
                [&](CFG::BasicBlock::InvalidExit const &) {
                  llvm_unreachable("Unexpected BB terminator");
                },
                [&](CFG::BasicBlock::Jump const &Jump) {
                  if (Jump.Backwards)
                    BackwardsJumps.emplace_back(Block, Jump.Target);
                  AddChild(Jump.Target);
                },
                [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump) {
                  AddChild(ConditionalJump.Zero);
                  AddChild(ConditionalJump.NonZero);
                },
                [&](CFG::BasicBlock::FunctionReturn const &) {},
                [&](CFG::BasicBlock::Terminated const &) {},
            },
            Block->Exit);
      });
  return BackwardsJumps;
}

void StackLayoutGenerator::stitchConditionalJumps(
    CFG::BasicBlock const &Block) {
  EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *> BFS{{&Block}};
  BFS.run([&](CFG::BasicBlock const *Block, auto AddChild) {
    auto &Info = Layout.blockInfos.at(Block);
    std::visit(
        Overload{
            [&](CFG::BasicBlock::InvalidExit const &) {
              llvm_unreachable("Unexpected BB terminator");
            },
            [&](CFG::BasicBlock::Jump const &Jump) {
              if (!Jump.Backwards)
                AddChild(Jump.Target);
            },
            [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump) {
              auto &ZeroTargetInfo = Layout.blockInfos.at(ConditionalJump.Zero);
              auto &NonZeroTargetInfo =
                  Layout.blockInfos.at(ConditionalJump.NonZero);
              Stack ExitLayout = Info.exitLayout;

              // The last block must have produced the condition at the stack
              // top.
              assert(!ExitLayout.empty());
              assert(ExitLayout.back() == ConditionalJump.Condition);
              // The condition is consumed by the jump.
              ExitLayout.pop_back();

              auto FixJumpTargetEntry =
                  [&](Stack const &OriginalEntryLayout) -> Stack {
                Stack NewEntryLayout = ExitLayout;
                // Whatever the block being jumped to does not actually require,
                // can be marked as junk.
                for (auto &Slot : NewEntryLayout)
                  if (!EVMUtils::contains(OriginalEntryLayout, Slot))
                    Slot = JunkSlot{};
                // Make sure everything the block being jumped to requires is
                // actually present or can be generated.
                for (auto const &Slot : OriginalEntryLayout)
                  assert(canBeFreelyGenerated(Slot) ||
                         EVMUtils::contains(NewEntryLayout, Slot));
                return NewEntryLayout;
              };

              ZeroTargetInfo.entryLayout =
                  FixJumpTargetEntry(ZeroTargetInfo.entryLayout);
              NonZeroTargetInfo.entryLayout =
                  FixJumpTargetEntry(NonZeroTargetInfo.entryLayout);
              AddChild(ConditionalJump.Zero);
              AddChild(ConditionalJump.NonZero);
            },
            [&](CFG::BasicBlock::FunctionReturn const &) {},
            [&](CFG::BasicBlock::Terminated const &) {},
        },
        Block->Exit);
  });
}

Stack StackLayoutGenerator::combineStack(Stack const &Stack1,
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
  for (auto Slot : EVMUtils::drop_first(Stack1, CommonPrefix.size()))
    Stack1Tail.emplace_back(Slot);

  for (auto Slot : EVMUtils::drop_first(Stack2, CommonPrefix.size()))
    Stack2Tail.emplace_back(Slot);

  if (Stack1Tail.empty())
    return CommonPrefix + compressStack(Stack2Tail);

  if (Stack2Tail.empty())
    return CommonPrefix + compressStack(Stack1Tail);

  Stack Candidate;
  for (auto Slot : Stack1Tail)
    if (!EVMUtils::contains(Candidate, Slot))
      Candidate.emplace_back(Slot);

  for (auto Slot : Stack2Tail)
    if (!EVMUtils::contains(Candidate, Slot))
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
      if (canBeFreelyGenerated(Slot))
        return;

      Stack Tmp = CommonPrefix;
      Tmp += TestStack;

      auto Depth = EVMUtils::findOffset(EVMUtils::get_reverse(Tmp), Slot);
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
  std::vector<size_t> C(N, 0);
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
      // need to revert back to ``I = 1.`` However, the incorrect implementation
      // produces decent result and the proper version would have N! complexity
      // and is thereby not feasible.
      ++I;
    } else {
      C[I] = 0;
      ++I;
    }
  }

  return CommonPrefix + BestCandidate;
}

Stack StackLayoutGenerator::compressStack(Stack CurStack) {
  std::optional<size_t> FirstDupOffset;
  do {
    if (FirstDupOffset) {
      if (*FirstDupOffset != (CurStack.size() - 1))
        std::swap(CurStack.at(*FirstDupOffset), CurStack.back());
      CurStack.pop_back();
      FirstDupOffset.reset();
    }

    auto I = CurStack.rbegin(), E = CurStack.rend();
    for (size_t Depth = 0; I < E; ++I, ++Depth) {
      StackSlot &Slot = *I;
      if (canBeFreelyGenerated(Slot)) {
        FirstDupOffset = CurStack.size() - Depth - 1;
        break;
      }

      if (auto DupDepth = EVMUtils::findOffset(
              EVMUtils::drop_first(EVMUtils::get_reverse(CurStack), Depth + 1),
              Slot)) {
        if (Depth + *DupDepth <= 16) {
          FirstDupOffset = CurStack.size() - Depth - 1;
          break;
        }
      }
    }
  } while (FirstDupOffset);
  return CurStack;
}

void StackLayoutGenerator::fillInJunk(CFG::BasicBlock const &Block,
                                      CFG::FunctionInfo const *FunctionInfo) {
  /// Recursively adds junk to the subgraph starting on \p Entry.
  /// Since it is only called on cut-vertices, the full subgraph retains proper
  /// stack balance.
  auto AddJunkRecursive = [&](CFG::BasicBlock const *Entry, size_t NumJunk) {
    EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *> BFS{{Entry}};

    BFS.run([&](CFG::BasicBlock const *Block, auto AddChild) {
      auto &BlockInfo = Layout.blockInfos.at(Block);
      BlockInfo.entryLayout =
          Stack{NumJunk, JunkSlot{}} + std::move(BlockInfo.entryLayout);

      for (auto const &Operation : Block->Operations) {
        auto &OpEntryLayout = Layout.operationEntryLayout.at(&Operation);
        OpEntryLayout = Stack{NumJunk, JunkSlot{}} + std::move(OpEntryLayout);
      }

      BlockInfo.exitLayout =
          Stack{NumJunk, JunkSlot{}} + std::move(BlockInfo.exitLayout);

      std::visit(
          Overload{
              [&](CFG::BasicBlock::InvalidExit const &) {
                llvm_unreachable("Unexpected BB terminator");
              },
              [&](CFG::BasicBlock::Jump const &Jump) { AddChild(Jump.Target); },
              [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump) {
                AddChild(ConditionalJump.Zero);
                AddChild(ConditionalJump.NonZero);
              },
              [&](CFG::BasicBlock::FunctionReturn const &) {
                llvm_unreachable("FunctionReturn : unexpected BB terminator");
              },
              [&](CFG::BasicBlock::Terminated const &) {},
          },
          Block->Exit);
    });
  };

  /// Returns the number of operations required to transform \p  Source to \p
  /// Target.
  auto EvaluateTransform = [&](Stack Source, Stack const &Target) -> size_t {
    size_t OpGas = 0;
    auto Swap = [&](unsigned SwapDepth) {
      if (SwapDepth > 16)
        OpGas += 1000;
      else
        OpGas += 3; // SWAP* gas price;
    };

    auto DupOrPush = [&](StackSlot const &Slot) {
      if (canBeFreelyGenerated(Slot))
        OpGas += 3;
      else {
        if (auto Depth =
                EVMUtils::findOffset(EVMUtils::get_reverse(Source), Slot)) {
          if (*Depth < 16)
            OpGas += 3; // gas price for DUP
          else
            OpGas += 1000;
        } else {
          // This has to be a previously unassigned return variable.
          // We at least sanity-check that it is among the return variables at
          // all.
#ifndef NDEBUG
          bool VarExists = false;
          assert(std::holds_alternative<VariableSlot>(Slot));
          for (CFG::BasicBlock *Exit : FunctionInfo->Exits) {
            const Stack &RetValues =
                std::get<CFG::BasicBlock::FunctionReturn>(Exit->Exit).RetValues;

            for (const StackSlot &Val : RetValues) {
              if (const VariableSlot *VarSlot = std::get_if<VariableSlot>(&Val))
                if (*VarSlot == std::get<VariableSlot>(Slot))
                  VarExists = true;
            }
          }
          assert(VarExists);
#endif // NDEBUG
       // Strictly speaking the cost of the
       // PUSH0 depends on the targeted EVM version, but the difference will
       // not matter here.
          OpGas += 2;
        }
      }
    };

    auto Pop = [&]() { OpGas += 2; };

    createStackLayout(Source, Target, Swap, DupOrPush, Pop);
    return OpGas;
  };

  /// Returns the number of junk slots to be prepended to \p TargetLayout for
  /// an optimal transition from \p EntryLayout to \p TargetLayout.
  auto GetBestNumJunk = [&](Stack const &EntryLayout,
                            Stack const &TargetLayout) -> size_t {
    size_t BestCost = EvaluateTransform(EntryLayout, TargetLayout);
    size_t BestNumJunk = 0;
    size_t MaxJunk = EntryLayout.size();
    for (size_t NumJunk = 1; NumJunk <= MaxJunk; ++NumJunk) {
      size_t Cost = EvaluateTransform(EntryLayout, Stack{NumJunk, JunkSlot{}} +
                                                       TargetLayout);
      if (Cost < BestCost) {
        BestCost = Cost;
        BestNumJunk = NumJunk;
      }
    }
    return BestNumJunk;
  };

  if (FunctionInfo && !FunctionInfo->CanContinue && Block.AllowsJunk()) {
    Stack Params;
    for (const auto &Param : FunctionInfo->Parameters)
      Params.emplace_back(Param);
    std::reverse(Params.begin(), Params.end());
    size_t BestNumJunk =
        GetBestNumJunk(Params, Layout.blockInfos.at(&Block).entryLayout);
    if (BestNumJunk > 0)
      AddJunkRecursive(&Block, BestNumJunk);
  }

  /// Traverses the CFG and at each block that allows junk, i.e. that is a
  /// cut-vertex that never leads to a function return, checks if adding junk
  /// reduces the shuffling cost upon entering and if so recursively adds junk
  /// to the spanned subgraph.
  EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *>{{&Block}}.run(
      [&](CFG::BasicBlock const *Block, auto AddChild) {
        if (Block->AllowsJunk()) {
          auto &BlockInfo = Layout.blockInfos.at(Block);
          Stack EntryLayout = BlockInfo.entryLayout;
          Stack const &NextLayout =
              Block->Operations.empty()
                  ? BlockInfo.exitLayout
                  : Layout.operationEntryLayout.at(&Block->Operations.front());
          if (EntryLayout != NextLayout) {
            size_t BestNumJunk = GetBestNumJunk(EntryLayout, NextLayout);
            if (BestNumJunk > 0) {
              AddJunkRecursive(Block, BestNumJunk);
              BlockInfo.entryLayout = EntryLayout;
            }
          }
        }

        std::visit(
            Overload{
                [&](CFG::BasicBlock::InvalidExit const &) {
                  llvm_unreachable("Invalid BB terminator");
                },
                [&](CFG::BasicBlock::Jump const &Jump) {
                  AddChild(Jump.Target);
                },
                [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump) {
                  AddChild(ConditionalJump.Zero);
                  AddChild(ConditionalJump.NonZero);
                },
                [&](CFG::BasicBlock::FunctionReturn const &) {},
                [&](CFG::BasicBlock::Terminated const &) {},
            },
            Block->Exit);
      });
}
