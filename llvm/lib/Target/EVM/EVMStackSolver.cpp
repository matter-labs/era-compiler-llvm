//===--------------------- EVMStackSolver.cpp -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMStackSolver.h"

#include "EVM.h"
#include "EVMInstrInfo.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMRegisterInfo.h"
#include "EVMStackShuffler.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "evm-stack-solver"

namespace {

/// \return index of \p Item in \p RangeOrContainer or std::nullopt.
template <typename T, typename V>
std::optional<size_t> offset(T &&RangeOrContainer, V &&Item) {
  auto &&Range = std::forward<T>(RangeOrContainer);
  auto It = find(Range, std::forward<V>(Item));
  return (It == adl_end(Range))
             ? std::nullopt
             : std::optional(std::distance(adl_begin(Range), It));
}

/// \return a range covering the last N elements of \p RangeOrContainer.
template <typename T> auto take_back(T &&RangeOrContainer, size_t N = 1) {
  return make_range(std::prev(adl_end(std::forward<T>(RangeOrContainer)), N),
                    adl_end(std::forward<T>(RangeOrContainer)));
}

template <typename T>
std::optional<T> add(std::optional<T> a, std::optional<T> b) {
  if (a && b)
    return *a + *b;
  return std::nullopt;
}

/// Given stack \p AfterInst, compute stack before the instruction excluding
/// instruction input operands.
/// \param CompressStack: remove duplicates and rematerializable slots.
Stack calculateStackBeforeInst(const Stack &InstDefs, const Stack &AfterInst,
                               bool CompressStack, unsigned StackDepthLimit) {
  // Number of slots on stack before the instruction excluding its inputs.
  size_t BeforeInstSize = count_if(AfterInst, [&](const StackSlot *S) {
    return !is_contained(InstDefs, S) &&
           !(CompressStack && S->isRematerializable());
  });

  // To use StackTransformer, the computed stack must be transformable to the
  // AfterInst stack. Initialize it as follows:
  //   UnknownSlot{0}, ..., UnknownSlot{BeforeInstSize - 1}, [def<0>], ...,
  //   [def<m>]
  // where UnknownSlot{I} denotes an element to be copied from the AfterInst
  // stack to the I-th position in the BeforeInst stack.
  SmallVector<UnknownSlot> UnknownSlots;
  for (size_t Index = 0; Index < BeforeInstSize; ++Index)
    UnknownSlots.emplace_back(Index);

  Stack Tmp;
  for (auto &S : UnknownSlots)
    Tmp.push_back(&S);
  append_range(Tmp, InstDefs);

  if (Tmp.empty())
    return Stack{};

  EVMStackShuffler Shuffler(Tmp, AfterInst, StackDepthLimit);

  auto canSkipSlot = [&InstDefs, CompressStack](const StackSlot *Slot) {
    return count(InstDefs, Slot) ||
           (CompressStack && Slot->isRematerializable());
  };
  auto countOccurences = [&canSkipSlot](const StackSlot *Slot, Stack &C,
                                        const Stack &T) {
    int Num = -count(C, Slot);
    if (canSkipSlot(Slot))
      Num = Num + count(T, Slot);
    return Num;
  };

  Shuffler.setMatch(
      [&canSkipSlot](const StackSlot *SrcSlot, const StackSlot *TgtSlot) {
        return isa<UnknownSlot>(SrcSlot) ? !canSkipSlot(TgtSlot)
                                         : SrcSlot == TgtSlot;
      });

  Shuffler.setGetCurrentNumOccurrences(
      [&countOccurences](const StackSlot *Slot, Stack &C, const Stack &T) {
        return isa<UnknownSlot>(Slot) ? 0 : countOccurences(Slot, C, T);
      });

  Shuffler.setGetTargetNumOccurrences(
      [&countOccurences, &canSkipSlot](const StackSlot *Slot, Stack &C,
                                       const Stack &T) {
        return !canSkipSlot(Slot) ? 0 : countOccurences(Slot, C, T);
      });

  Shuffler.setSwap([](size_t I, Stack &C) {
    assert(!isa<UnknownSlot>(C[C.size() - I - 1]) ||
           !isa<UnknownSlot>(C.back()));
  });

  Shuffler.shuffle();

  // After the stack transformation, for each index I, move the AfterInst slot
  // corresponding to UnknownSlot{I} from Tmp to the I-th position of the
  // BeforeInst.
  assert(Tmp.size() == AfterInst.size());
  SmallVector<const StackSlot *> BeforeInst(BeforeInstSize, nullptr);
  for (unsigned Idx = 0; Idx < Tmp.size(); ++Idx) {
    if (const auto *Slot = dyn_cast<UnknownSlot>(Tmp[Idx]))
      BeforeInst[Slot->getIndex()] = AfterInst[Idx];
  }
  assert(all_of(BeforeInst,
                [](const StackSlot *Slot) { return Slot != nullptr; }));

  return BeforeInst;
}

} // end anonymous namespace

BranchInfoTy llvm::getBranchInfo(const MachineBasicBlock *MBB) {
  const auto *EVMTII = static_cast<const EVMInstrInfo *>(
      MBB->getParent()->getSubtarget().getInstrInfo());
  auto *UnConstMBB = const_cast<MachineBasicBlock *>(MBB);
  SmallVector<MachineOperand, 1> Cond;
  SmallVector<MachineInstr *, 2> BranchInstrs;
  MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
  auto BT = EVMTII->analyzeBranch(*UnConstMBB, TBB, FBB, Cond,
                                  /* AllowModify */ false, BranchInstrs);
  if (BT == EVMInstrInfo::BT_Cond && !FBB)
    FBB = UnConstMBB->getFallThrough();

  if (BT == EVMInstrInfo::BT_NoBranch && !TBB && !MBB->succ_empty())
    TBB = UnConstMBB->getFallThrough();

  assert((BT != EVMInstrInfo::BT_Cond && BT != EVMInstrInfo::BT_CondUncond) ||
         !Cond.empty() &&
             "Condition should be defined for a condition branch.");
  assert(BranchInstrs.size() <= 2);
  return {BT, TBB, FBB, BranchInstrs,
          Cond.empty() ? std::nullopt : std::optional(Cond[1])};
}

/// Return cost of transformation in gas from \p Source to \p Target or
/// nullopt if unreachable stack elements are detected during the
/// transformation.
/// An element is considered unreachable if the transformation requires a stack
/// manipulation on an element deeper than \p StackDepthLimit, which EVM does
/// not support.
/// \note The copy of \p Source is intentional since it is modified during
/// computation. We retain the original \p Source to reattempt the
/// transformation using a compressed stack.
std::optional<unsigned>
llvm::calculateStackTransformCost(Stack Source, const Stack &Target,
                                  unsigned StackDepthLimit) {
  unsigned Gas = 0;
  bool HasError = false;
  auto Swap = [&Gas, &HasError, StackDepthLimit](unsigned SwapDepth) {
    if (SwapDepth > StackDepthLimit)
      HasError = true;
    Gas += EVMCOST::SWAP;
  };

  auto Rematerialize = [&Gas, &HasError, &Source,
                        StackDepthLimit](const StackSlot *Slot) {
    if (Slot->isRematerializable())
      Gas += EVMCOST::PUSH;
    else {
      std::optional<size_t> Depth = offset(reverse(Source), Slot);
      if (Depth && *Depth >= StackDepthLimit)
        HasError = true;
      Gas += EVMCOST::DUP;
    }
  };
  auto Pop = [&Gas]() { Gas += EVMCOST::POP; };

  calculateStack(Source, Target, StackDepthLimit, Swap, Rematerialize, Pop);
  return HasError ? std::nullopt : std::optional(Gas);
}

EVMStackSolver::EVMStackSolver(const MachineFunction &MF,
                               EVMStackModel &StackModel,
                               const MachineLoopInfo *MLI)
    : MF(MF), StackModel(StackModel), MLI(MLI) {}

void EVMStackSolver::run() {
  runPropagation();
  LLVM_DEBUG({
    dbgs() << "************* Stack *************\n";
    dump(dbgs());
  });
}

Stack EVMStackSolver::propagateThroughMI(const Stack &ExitStack,
                                         const MachineInstr &MI,
                                         bool CompressStack) {
  const Stack MIDefs = StackModel.getSlotsForInstructionDefs(&MI);
  // Stack before MI except for MI inputs.
  Stack BeforeMI = calculateStackBeforeInst(MIDefs, ExitStack, CompressStack,
                                            StackModel.stackDepthLimit());

#ifndef NDEBUG
  // Ensure MI defs are not present in BeforeMI stack.
  for (const auto *StackSlot : BeforeMI)
    if (const auto *RegSlot = dyn_cast<RegisterSlot>(StackSlot))
      assert(!MI.definesRegister(RegSlot->getReg()));
#endif // NDEBUG

  BeforeMI.append(StackModel.getMIInput(MI));
  // Store computed stack to StackModel.
  insertInstEntryStack(&MI, BeforeMI);

  // If the top stack slots can be rematerialized just before MI, remove it
  // from propagation to reduce pressure.
  while (!BeforeMI.empty()) {
    if (BeforeMI.back()->isRematerializable()) {
      BeforeMI.pop_back();
    } else if (auto Offset =
                   offset(drop_begin(reverse(BeforeMI), 1), BeforeMI.back())) {
      if (*Offset + 2 < StackModel.stackDepthLimit())
        BeforeMI.pop_back();
      else
        break;
    } else
      break;
  }

  return BeforeMI;
}

Stack EVMStackSolver::propagateThroughMBB(const Stack &ExitStack,
                                          const MachineBasicBlock *MBB,
                                          bool CompressStack) {
  Stack CurrentStack = ExitStack;
  for (const auto &MI : StackModel.reverseInstructionsToProcess(MBB)) {
    Stack BeforeMI = propagateThroughMI(CurrentStack, MI, CompressStack);
    if (!CompressStack &&
        !calculateStackTransformCost(BeforeMI, CurrentStack,
                                     StackModel.stackDepthLimit()))
      return propagateThroughMBB(ExitStack, MBB,
                                 /*CompressStack*/ true);
    CurrentStack = std::move(BeforeMI);
  }
  return CurrentStack;
}

void EVMStackSolver::runPropagation() {
  std::deque<const MachineBasicBlock *> Worklist{&MF.front()};
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

  // Propagate stack information until no new information is discovered.
  while (!Worklist.empty()) {
    // Process blocks in the worklist without following backedges first.
    while (!Worklist.empty()) {
      const MachineBasicBlock *MBB = *Worklist.begin();
      Worklist.pop_front();
      if (Visited.count(MBB))
        continue;

      // Get the MBB exit stack.
      std::optional<Stack> ExitStack = std::nullopt;
      auto [BranchTy, TBB, FBB, BrInsts, Condition] = getBranchInfo(MBB);

      switch (BranchTy) {
      case EVMInstrInfo::BT_None: {
        ExitStack = MBB->isReturnBlock()
                        ? StackModel.getReturnArguments(MBB->back())
                        : Stack{};
      } break;
      case EVMInstrInfo::BT_Uncond:
      case EVMInstrInfo::BT_NoBranch: {
        const MachineBasicBlock *Target = MBB->getSingleSuccessor();
        // Currently a basic block could end with FCALL and have no successors,
        // but FCALL is not a terminator, so we fall into BT_NoBranch.
        // TODO: #788 address it
        if (!Target) { // No successors.
          ExitStack = Stack{};
          break;
        }
        if (MachineLoop *ML = MLI->getLoopFor(MBB);
            ML && ML->isLoopLatch(MBB)) {
          // If the loop header's entry stack has been computed,
          // use it as the latch's exit stack; otherwise, assume an empty
          // exit stack to avoid non-termination.
          auto It = StackModel.getMBBEntryMap().find(Target);
          ExitStack =
              (It == StackModel.getMBBEntryMap().end() ? Stack{} : It->second);
        } else {
          if (Visited.count(Target))
            ExitStack = StackModel.getMBBEntryStack(Target);
          else
            Worklist.push_front(Target);
        }
      } break;
      case EVMInstrInfo::BT_Cond:
      case EVMInstrInfo::BT_CondUncond: {
        bool FBBVisited = Visited.count(FBB);
        bool TBBVisited = Visited.count(TBB);

        if (!FBBVisited)
          Worklist.push_front(FBB);
        if (!TBBVisited)
          Worklist.push_front(TBB);

        if (FBBVisited && TBBVisited) {
          // The order of arguments in 'combineStack' influences the
          // resulting combined stack, which is unexpected. Additionally,
          // passing TBB's stack resolves many 'stack-too-deep' errors in CI.
          // TODO: #781. Investigate the cause of this behavior.
          Stack CombinedStack = combineStack(StackModel.getMBBEntryStack(TBB),
                                             StackModel.getMBBEntryStack(FBB));
          // Retain the jump condition in ExitStack because StackSolver doesn't
          // propagate stacks through branch instructions.
          CombinedStack.emplace_back(StackModel.getStackSlot(*Condition));
          ExitStack = std::move(CombinedStack);
        }
      }
      }

      if (ExitStack) {
        Visited.insert(MBB);
        insertMBBExitStack(MBB, *ExitStack);
        insertMBBEntryStack(MBB, propagateThroughMBB(*ExitStack, MBB));
        append_range(Worklist, MBB->predecessors());
      }
    }

    // Revisit blocks connected by loop backedges.
    for (auto [Latch, Header] : Backedges) {
      const Stack &HeaderEntryStack = StackModel.getMBBEntryStack(Header);
      const Stack &LatchExitStack = StackModel.getMBBExitStack(Latch);
      if (all_of(HeaderEntryStack, [LatchExitStack](const StackSlot *Slot) {
            return is_contained(LatchExitStack, Slot);
          }))
        continue;

      // The latch block does not provide all the slots required by the loop
      // header. Mark the subgraph between the latch and header for
      // reprocessing.
      assert(Latch);
      Worklist.emplace_back(Latch);

      // Since the loop header's entry may be permuted, revisit its predecessors
      // to minimize stack transformation costs.
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
    }
  }

  // For an MBB with multiple successors the exit stack is a union of the entry
  // stack slots from all successors. Each successor's entry stack may be
  // missing some slots present in the exit stack. Adjust each successor's entry
  // stack to mirror the basic block's exit stack. For any slot that is dead in
  // a successor, mark it as 'unused'.
  for (const MachineBasicBlock &MBB : MF) {
    auto [BranchTy, TBB, FBB, BrInsts, Condition] = getBranchInfo(&MBB);

    if (BranchTy != EVMInstrInfo::BT_Cond &&
        BranchTy != EVMInstrInfo::BT_CondUncond)
      continue;

    Stack ExitStack = StackModel.getMBBExitStack(&MBB);

    // The condition must be on top of ExitStack, but a conditional jump
    // consumes it.
    assert(ExitStack.back() == StackModel.getStackSlot(*Condition));
    ExitStack.pop_back();

    for (const MachineBasicBlock *Succ : MBB.successors()) {
      const Stack &SuccEntryStack = StackModel.getMBBEntryStack(Succ);
      Stack NewSuccEntryStack = ExitStack;
      for (const StackSlot *&Slot : NewSuccEntryStack)
        if (!is_contained(SuccEntryStack, Slot))
          Slot = EVMStackModel::getUnusedSlot();

#ifndef NDEBUG
      for (const StackSlot *Slot : SuccEntryStack)
        assert(Slot->isRematerializable() ||
               is_contained(NewSuccEntryStack, Slot));
#endif // NDEBUG

      insertMBBEntryStack(Succ, NewSuccEntryStack);
    }
  }

  Stack EntryStack;
  bool IsNoReturn = MF.getFunction().hasFnAttribute(Attribute::NoReturn);
  Stack FunctionParameters = StackModel.getFunctionParameters();

  // We assume the function containing PUSHDEPLOYADDRESS instruction has the
  // following properties:
  //   - It is unique (verified in AsmPrinter)
  //   - It appears first in the module layout. TODO: #778
  //   - It does not return and has no arguments
  if (const auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
      MFI->getHasPushDeployAddress()) {
    MachineBasicBlock::const_iterator I = MF.front().getFirstNonDebugInstr();
    assert(I != MF.front().end() && I->getOpcode() == EVM::PUSHDEPLOYADDRESS &&
           "MF has no PUSHDEPLOYADDRESS");
    assert(IsNoReturn && FunctionParameters.empty() &&
           "PUSHDEPLOYADDRESS in a non - void/nullary function");
    EntryStack.push_back(StackModel.getRegisterSlot(I->getOperand(0).getReg()));
  }
  // The entry MBB's stack contains the function parameters, which cannot be
  // inferred; put them to the stack.
  if (!IsNoReturn)
    EntryStack.push_back(StackModel.getCalleeReturnSlot(&MF));

  // Calling convention: input arguments are passed in stack such that the
  // first one specified in the function declaration is passed on the stack TOP.
  append_range(EntryStack, reverse(FunctionParameters));
  insertMBBEntryStack(&MF.front(), EntryStack);
}

Stack EVMStackSolver::combineStack(const Stack &Stack1, const Stack &Stack2) {
  auto [it1, it2] =
      std::mismatch(Stack1.begin(), Stack1.end(), Stack2.begin(), Stack2.end());

  Stack CommonPrefix;
  CommonPrefix.append(Stack1.begin(), it1);

  Stack Stack1Tail, Stack2Tail;
  Stack1Tail.append(it1, Stack1.end());
  Stack2Tail.append(it2, Stack2.end());

  if (Stack1Tail.empty()) {
    CommonPrefix.append(compressStack(Stack2Tail));
    return CommonPrefix;
  }

  if (Stack2Tail.empty()) {
    CommonPrefix.append(compressStack(Stack1Tail));
    return CommonPrefix;
  }

  Stack Candidate;
  for (const auto *Slot : concat<const StackSlot *>(Stack1Tail, Stack2Tail))
    if (!is_contained(Candidate, Slot) && !Slot->isRematerializable())
      Candidate.push_back(Slot);

  auto evaluate = [&CommonPrefix, &Stack1, &Stack2,
                   this](const Stack &Candidate) {
    Stack Test = CommonPrefix;
    Test.append(Candidate);
    return add(calculateStackTransformCost(Test, Stack1,
                                           StackModel.stackDepthLimit()),
               calculateStackTransformCost(Test, Stack2,
                                           StackModel.stackDepthLimit()))
        .value_or(std::numeric_limits<unsigned>::max());
  };

  // This is a modified implementation of the Heap algorithm that does
  // not restart I at 1 for each swap.
  size_t N = Candidate.size();
  Stack BestCandidate = Candidate;
  size_t BestCost = evaluate(Candidate);

  // Initialize counters for Heap's algorithm.
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
      // Note: A proper implementation would reset I to 1 here,
      // but doing so would generate all N! permutations,
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
  auto ShouldRemove =
      [&CurStack, this](SmallVector<const StackSlot *>::reverse_iterator I) {
        size_t RIdx = std::distance(CurStack.rbegin(), I);
        if ((*I)->isRematerializable())
          return true;
        if (auto DistanceToCopy =
                offset(make_range(std::next(I), CurStack.rend()), *I))
          return (RIdx + *DistanceToCopy < StackModel.stackDepthLimit());
        return false;
      };
  for (auto I = CurStack.rbegin(); I != CurStack.rend();) {
    if (ShouldRemove(I)) {
      std::swap(*I, CurStack.back());
      ++I; /* In case I == rbegin(), pop_back() will invalidate it */
      CurStack.pop_back();
      continue;
    }
    ++I;
  }
  return CurStack;
}

#ifndef NDEBUG
static std::string getMBBId(const MachineBasicBlock *MBB) {
  return std::to_string(MBB->getNumber()) + "." + std::string(MBB->getName());
}

void EVMStackSolver::dump(raw_ostream &OS) {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot *ParamSlot : StackModel.getFunctionParameters())
    OS << ParamSlot->toString();
  OS << ");\n";
  OS << "Entry MBB: " << getMBBId(&MF.front()) << ";\n";

  for (const MachineBasicBlock &MBB : MF)
    dumpMBB(OS, &MBB);
}

static std::string getMIName(const MachineInstr *MI) {
  if (MI->getOpcode() == EVM::FCALL) {
    const MachineOperand *Callee = MI->explicit_uses().begin();
    return Callee->getGlobal()->getName().str();
  }
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  return TII->getName(MI->getOpcode()).str();
}

void EVMStackSolver::dumpMBB(raw_ostream &OS, const MachineBasicBlock *MBB) {
  OS << getMBBId(MBB) << ":\n";
  OS << '\t' << StackModel.getMBBEntryStack(MBB).toString() << '\n';
  for (const auto &MI : StackModel.instructionsToProcess(MBB)) {
    const Stack &MIOutput = StackModel.getSlotsForInstructionDefs(&MI);
    const Stack &MIInput = StackModel.getMIInput(MI);
    if (isPushOrDupLikeMI(MI) && MIInput == MIOutput)
      continue;

    OS << '\n';
    Stack MIEntry = StackModel.getInstEntryStack(&MI);
    OS << '\t' << MIEntry.toString() << '\n';
    OS << '\t' << getMIName(&MI) << '\n';
    assert(MIInput.size() <= MIEntry.size());
    MIEntry.resize(MIEntry.size() - MIInput.size());
    MIEntry.append(MIOutput);
    OS << '\t' << MIEntry.toString() << "\n";
  }
  OS << '\n';
  OS << '\t' << StackModel.getMBBExitStack(MBB).toString() << "\n";

  auto [BranchTy, TBB, FBB, BrInsts, Condition] = getBranchInfo(MBB);
  switch (BranchTy) {
  case EVMInstrInfo::BT_None: {
    if (MBB->isReturnBlock()) {
      OS << "Exit type: function return, " << MF.getName() << '\n';
      OS << "Return values: "
         << StackModel.getReturnArguments(MBB->back()).toString() << '\n';
    } else {
      OS << "Exit type: terminate\n";
    }
  } break;
  case EVMInstrInfo::BT_Uncond:
  case EVMInstrInfo::BT_NoBranch: {
    if (TBB) {
      OS << "Exit type: unconditional branch\n";
      OS << "Target: " << getMBBId(TBB) << '\n';
    } else {
      OS << "Exit type: terminate\n";
    }
  } break;
  case EVMInstrInfo::BT_Cond:
  case EVMInstrInfo::BT_CondUncond: {
    OS << "Exit type: conditional branch, ";
    OS << "cond: " << StackModel.getStackSlot(*Condition)->toString() << '\n';
    OS << "False branch: " << getMBBId(FBB) << '\n';
    OS << "True branch: " << getMBBId(TBB) << '\n';
  } break;
  }
  OS << '\n';
}
#endif // NDEBUG
