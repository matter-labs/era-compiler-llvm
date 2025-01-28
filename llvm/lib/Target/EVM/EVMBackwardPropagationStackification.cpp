//===----- EVMBPStackification.cpp - BP stackification ---------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements backward propagation (BP) stackification.
// Original idea was taken from the Ethereum's compiler (solc) stackification
// algorithm.
// The algorithm is broken into following components:
//   - CFG (Control Flow Graph) and CFG builder. Stackification CFG has similar
//     structure to LLVM CFG one, but employs wider notion of instruction.
//   - Stack layout generator. Contains information about the stack layout at
//     entry and exit of each CFG::BasicBlock. It also contains input/output
//     stack layout for each operation.
//   - Code transformation into stakified form. This component uses both CFG
//     and the stack layout information to get stackified LLVM MIR.
//   - Stack shuffler. Finds optimal (locally) transformation between two stack
//     layouts using three primitives: POP, PUSHn, DUPn. The stack shuffler
//     is used by the components above.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineCFGInfo.h"
#include "EVMStackDebug.h"
#include "EVMStackLayout.h"
#include "EVMStackLayoutPermutations.h"
#include "EVMStackShuffler.h"
#include "EVMStackifyCodeEmitter.h"
#include "EVMSubtarget.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>

using namespace llvm;

#define DEBUG_TYPE "evm-backward-propagation-stackification"

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

class EVMBPStackification final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  EVMBPStackification() : MachineFunctionPass(ID) {}

private:
  StringRef getPassName() const override {
    return "EVM backward propagation stackification";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::TracksLiveness);
  }

  /// Returns the optimal entry stack layout, s.t. \p Op can be applied
  /// to it and the result can be transformed to \p ExitStack with minimal
  /// stack shuffling. Simultaneously stores the entry layout required for
  /// executing the operation in the map.
  Stack propagateStackThroughOperation(
      const Stack &ExitStack, const Operation &Op, bool CompressStack,
      OperationLayoutMapType &OpEntryLayoutMap) const;

  /// Returns stack layout at the entry of \p MBB, assuming the layout after
  /// executing the block should be \p ExitStack.
  Stack propagateStackThroughMBB(const Stack &ExitStack,
                                 const MachineBasicBlock *MBB,
                                 bool CompressStack,
                                 OperationLayoutMapType &OpEntryLayoutMap,
                                 const EVMStackModel &StackModel) const;

  /// Traverse the CFG backward propagating stack layouts between operations and
  /// syncing stack layout between MBBs.
  std::unique_ptr<EVMStackLayout>
  createStackLayout(const MachineFunction &MF, const EVMMachineCFGInfo &CFGInfo,
                    const EVMStackModel &StackModel) const;

  const MachineLoopInfo *MLI = nullptr;
};
} // end anonymous namespace

char EVMBPStackification::ID = 0;

INITIALIZE_PASS_BEGIN(EVMBPStackification, DEBUG_TYPE,
                      "Backward propagation stackification", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(EVMBPStackification, DEBUG_TYPE,
                    "Backward propagation stackification", false, false)

FunctionPass *llvm::createEVMBPStackification() {
  return new EVMBPStackification();
}

Stack EVMBPStackification::propagateStackThroughOperation(
    const Stack &ExitStack, const Operation &Op, bool CompressStack,
    OperationLayoutMapType &OpEntryLayoutMap) const {
  // Enable aggressive stack compression for recursive calls.
  if (std::holds_alternative<FunctionCall>(Op.Operation))
    // TODO: compress stack for recursive functions.
    CompressStack = false;

  // This is a huge tradeoff between code size, gas cost and stack size.
  auto generateSlotOnTheFly = [&](const StackSlot *Slot) {
    return CompressStack && Slot->isRematerializable();
  };

  // Determine the ideal permutation of the slots in ExitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // 'IdealStack + Op.output' to ExitLayout is cheap.
  Stack IdealStack = EVMStackLayoutPermutations::createIdealLayout(
      Op.Output, ExitStack, generateSlotOnTheFly);

  // Make sure the resulting previous slots do not overlap with any assignmed
  // variables.
  if (auto const *AssignOp = std::get_if<Assignment>(&Op.Operation))
    for (const StackSlot *Slot : IdealStack)
      if (const auto *VarSlot = dyn_cast<VariableSlot>(Slot))
        assert(!is_contained(AssignOp->Variables, VarSlot));

  // Since stack+Operation.output can be easily shuffled to ExitLayout, the
  // desired layout before the operation is stack+Operation.input;
  IdealStack.append(Op.Input);

  // Store the exact desired operation entry layout. The stored layout will be
  // recreated by the code transform before executing the operation. However,
  // this recreation can produce slots that can be freely generated or are
  // duplicated, i.e. we can compress the stack afterwards without causing
  // problems for code generation later.
  OpEntryLayoutMap[&Op] = IdealStack;

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!IdealStack.empty()) {
    if (IdealStack.back()->isRematerializable())
      IdealStack.pop_back();
    else if (std::optional<size_t> Offset = offset(
                 drop_begin(reverse(IdealStack), 1), IdealStack.back())) {
      if (*Offset + 2 < 16)
        IdealStack.pop_back();
      else
        break;
    } else
      break;
  }

  return IdealStack;
}

Stack EVMBPStackification::propagateStackThroughMBB(
    const Stack &ExitStack, const MachineBasicBlock *MBB, bool CompressStack,
    OperationLayoutMapType &OpEntryLayoutMap,
    const EVMStackModel &StackModel) const {
  Stack CurrentStack = ExitStack;
  for (const Operation &Op : reverse(StackModel.getOperations(MBB))) {
    Stack NewStack = propagateStackThroughOperation(
        CurrentStack, Op, CompressStack, OpEntryLayoutMap);
    if (!CompressStack &&
        !EVMStackLayoutPermutations::findStackTooDeep(NewStack, CurrentStack)
             .empty())
      // If we had stack errors, run again with aggressive stack compression.
      return propagateStackThroughMBB(std::move(ExitStack), MBB, true,
                                      OpEntryLayoutMap, StackModel);
    CurrentStack = std::move(NewStack);
  }
  return CurrentStack;
}

// Returns the number of junk slots to be prepended to \p TargetLayout for
// an optimal transition from \p EntryLayout to \p TargetLayout.
static size_t getOptimalNumberOfJunks(const Stack &EntryLayout,
                                      const Stack &TargetLayout) {
  size_t BestCost = EVMStackLayoutPermutations::evaluateStackTransform(
      EntryLayout, TargetLayout);
  size_t BestNumJunk = 0;
  size_t MaxJunk = EntryLayout.size();
  for (size_t NumJunk = 1; NumJunk <= MaxJunk; ++NumJunk) {
    Stack JunkedTarget(NumJunk, EVMStackModel::getJunkSlot());
    JunkedTarget.append(TargetLayout);
    size_t Cost = EVMStackLayoutPermutations::evaluateStackTransform(
        EntryLayout, JunkedTarget);
    if (Cost < BestCost) {
      BestCost = Cost;
      BestNumJunk = NumJunk;
    }
  }
  return BestNumJunk;
}

std::unique_ptr<EVMStackLayout>
EVMBPStackification::createStackLayout(const MachineFunction &MF,
                                       const EVMMachineCFGInfo &CFGInfo,
                                       const EVMStackModel &StackModel) const {
  std::deque<const MachineBasicBlock *> ToVisit{&MF.front()};
  DenseSet<const MachineBasicBlock *> Visited;
  MBBLayoutMapType MBBEntryLayoutMap, MBBExitLayoutMap;
  OperationLayoutMapType OpEntryLayoutMap;

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
    // assuming the current preliminary entry layout of a loop header
    // as the initial exit layout of the latch block.
    while (!ToVisit.empty()) {
      const MachineBasicBlock *MBB = *ToVisit.begin();
      ToVisit.pop_front();
      if (Visited.count(MBB))
        continue;

      // Get the MBB exit layout.
      std::optional<Stack> ExitLayout = std::nullopt;
      const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(MBB);
      MBBExitType ExitType = TermInfo->getExitType();
      if (ExitType == MBBExitType::UnconditionalBranch) {
        const MachineBasicBlock *Target = MBB->getSingleSuccessor();
        if (MachineLoop *ML = MLI->getLoopFor(MBB);
            ML && ML->isLoopLatch(MBB)) {
          // Choose the best currently known entry layout of the jump target
          // as initial exit. Note that this may not yet be the final
          // layout.
          auto It = MBBEntryLayoutMap.find(Target);
          ExitLayout = (It == MBBEntryLayoutMap.end() ? Stack{} : It->second);
        } else {
          // If the current iteration has already visited the jump target,
          // start from its entry layout. Otherwise stage the jump target
          // for visit and defer the current block.
          if (Visited.count(Target))
            ExitLayout = MBBEntryLayoutMap.at(Target);
          else
            ToVisit.emplace_front(Target);
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
          ToVisit.emplace_front(FBB);

        if (!TBBVisited)
          ToVisit.emplace_front(TBB);

        // If we have visited both jump targets, start from its entry layout.
        if (FBBVisited && TBBVisited) {
          Stack CombinedStack = EVMStackLayoutPermutations::combineStack(
              MBBEntryLayoutMap.at(FBB), MBBEntryLayoutMap.at(TBB));
          // Additionally, the jump condition has to be at the stack top at
          // exit.
          CombinedStack.emplace_back(StackModel.getStackSlot(*Condition));
          ExitLayout = std::move(CombinedStack);
        }
      } else if (ExitType == MBBExitType::FunctionReturn)
        ExitLayout = StackModel.getReturnArguments(MBB->back());
      else
        ExitLayout = Stack{};

      // If the MBB exit layout is known, we can back back propagate
      // stack layout till the MBB entry.
      if (ExitLayout) {
        Visited.insert(MBB);
        MBBExitLayoutMap[MBB] = *ExitLayout;
        MBBEntryLayoutMap[MBB] = propagateStackThroughMBB(
            *ExitLayout, MBB, false, OpEntryLayoutMap, StackModel);
        for (auto Pred : MBB->predecessors())
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
        Ops.empty() ? ExitLayout : OpEntryLayoutMap.at(&Ops.front());

    if (EntryLayout == NextLayout)
      continue;

    size_t JunksNum = getOptimalNumberOfJunks(EntryLayout, NextLayout);
    if (!JunksNum)
      continue;

    for (const MachineBasicBlock *Succ : depth_first(&MBB)) {
      Stack JunkedEntry(JunksNum, EVMStackModel::getJunkSlot());
      JunkedEntry.append(MBBEntryLayoutMap.at(Succ));
      MBBEntryLayoutMap[Succ] = std::move(JunkedEntry);

      for (const Operation &Op : StackModel.getOperations(Succ)) {
        Stack JunkedOpEntry(JunksNum, EVMStackModel::getJunkSlot());
        JunkedOpEntry.append(OpEntryLayoutMap.at(&Op));
        OpEntryLayoutMap[&Op] = std::move(JunkedOpEntry);
      }

      Stack JunkedExit(JunksNum, EVMStackModel::getJunkSlot());
      JunkedExit.append(MBBExitLayoutMap.at(Succ));
      MBBExitLayoutMap[Succ] = std::move(JunkedExit);
    }
    MBBEntryLayoutMap[&MBB] = EntryLayout;
  }

  return std::make_unique<EVMStackLayout>(MBBEntryLayoutMap, MBBExitLayoutMap,
                                          OpEntryLayoutMap);
}

bool EVMBPStackification::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Backward propagation stackification **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto &LIS = getAnalysis<LiveIntervals>();
  MLI = &getAnalysis<MachineLoopInfo>();

  // We don't preserve SSA form.
  MRI.leaveSSA();

  assert(MRI.tracksLiveness() && "Stackification expects liveness");
  EVMMachineCFGInfo CFGInfo(MF, MLI);
  EVMStackModel StackModel(MF, LIS);
  std::unique_ptr<EVMStackLayout> Layout =
      createStackLayout(MF, CFGInfo, StackModel);

  LLVM_DEBUG({
    dbgs() << "************* Stack Layout *************\n";
    StackLayoutPrinter P(dbgs(), MF, *Layout, CFGInfo, StackModel);
    P();
  });

  EVMStackifyCodeEmitter(*Layout, StackModel, CFGInfo, MF).run();
  return true;
}
