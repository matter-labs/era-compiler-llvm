//===--------- EVMMachineCFGInfo.cpp - Machine CFG info ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides some information about machine Control Flow Graph.
//
//===----------------------------------------------------------------------===//

#include "EVMMachineCFGInfo.h"
#include "EVMHelperUtilities.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

using namespace llvm;

static std::pair<MachineInstr *, MachineInstr *>
getBranchInstructions(MachineBasicBlock &MBB) {
  MachineInstr *ConditionalBranch = nullptr;
  MachineInstr *UnconditionalBranch = nullptr;
  MachineBasicBlock::reverse_iterator I = MBB.rbegin(), E = MBB.rend();
  while (I != E && I->isTerminator()) {
    if (I->isUnconditionalBranch())
      UnconditionalBranch = &*I;
    else if (I->isConditionalBranch())
      ConditionalBranch = &*I;
    ++I;
  }
  return {ConditionalBranch, UnconditionalBranch};
}

static bool isTerminate(const MachineInstr *MI) {
  switch (MI->getOpcode()) {
  default:
    return false;
  case EVM::REVERT:
  case EVM::RETURN:
  case EVM::STOP:
  case EVM::INVALID:
    return true;
  }
}

EVMMachineCFGInfo::EVMMachineCFGInfo(MachineFunction &MF,
                                     MachineLoopInfo *MLI) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  for (MachineBasicBlock &MBB : MF)
    collectTerminatorsInfo(TII, MLI, MBB);

  SmallVector<const MachineBasicBlock *> ReturnBlocks;
  for (const MachineBasicBlock &MBB : MF) {
    const EVMMBBTerminatorsInfo *TermInfo = getTerminatorsInfo(&MBB);
    if (TermInfo->getExitType() == MBBExitType::FunctionReturn)
      ReturnBlocks.emplace_back(&MBB);
  }
  collectBlocksLeadingToFunctionReturn(ReturnBlocks);
  collectCutVertexes(&MF.front());
}

const EVMMBBTerminatorsInfo *
EVMMachineCFGInfo::getTerminatorsInfo(const MachineBasicBlock *MBB) const {
  return MBBTerminatorsInfoMap.at(MBB).get();
}

void EVMMachineCFGInfo::collectTerminatorsInfo(const TargetInstrInfo *TII,
                                               const MachineLoopInfo *MLI,
                                               MachineBasicBlock &MBB) {
  assert(MBBTerminatorsInfoMap.count(&MBB) == 0);

  MBBTerminatorsInfoMap.try_emplace(&MBB,
                                    std::make_unique<EVMMBBTerminatorsInfo>());
  EVMMBBTerminatorsInfo *Info = MBBTerminatorsInfoMap.at(&MBB).get();
  SmallVector<MachineOperand, 1> Cond;
  MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
  if (TII->analyzeBranch(MBB, TBB, FBB, Cond)) {
    MachineInstr *LastMI = &MBB.back();
    Info->LastTerm = LastMI;
    if (LastMI->isReturn())
      Info->ExitType = MBBExitType::FunctionReturn;
    else if (isTerminate(LastMI))
      Info->ExitType = MBBExitType::Terminate;
    else
      llvm_unreachable("Unexpected MBB exit");
    return;
  }

  // A non-terminator instruction is followed by 'unreachable',
  // or a 'noreturn' function is at the end of MBB.
  if (!TBB && !FBB && MBB.succ_empty()) {
    Info->ExitType = MBBExitType::Terminate;
    return;
  }

  bool IsLatch = false;
  if (MachineLoop *ML = MLI->getLoopFor(&MBB))
    IsLatch = ML->isLoopLatch(&MBB);
  auto [CondBr, UncondBr] = getBranchInstructions(MBB);
  if (!TBB || (TBB && Cond.empty())) {
    // Fall through, or unconditional jump.
    assert(!CondBr);
    if (!TBB) {
      assert(!UncondBr);
      assert(MBB.getSingleSuccessor());
      TBB = MBB.getFallThrough();
      assert(TBB);
    }
    Info->ExitType = MBBExitType::UnconditionalBranch;
    Info->BranchInfo.Unconditional = {TBB, UncondBr, IsLatch};
  } else if (TBB && !Cond.empty()) {
    assert(!IsLatch);
    // Conditional jump + fallthrough, or
    // conditional jump followed by unconditional jump).
    if (!FBB) {
      FBB = MBB.getFallThrough();
      assert(FBB);
    }
    Info->ExitType = MBBExitType::ConditionalBranch;
    assert(Cond[0].isIdenticalTo(CondBr->getOperand(1)));
    Info->BranchInfo.Conditional = {&CondBr->getOperand(1), TBB, FBB, CondBr,
                                    UncondBr};
  }
}

// Mark basic blocks that have outgoing paths to function returns.
void EVMMachineCFGInfo::collectBlocksLeadingToFunctionReturn(
    const SmallVector<const MachineBasicBlock *> &Returns) {
  SmallPtrSet<const MachineBasicBlock *, 32> Visited;
  SmallVector<const MachineBasicBlock *> WorkList = Returns;
  while (!WorkList.empty()) {
    const MachineBasicBlock *MBB = WorkList.pop_back_val();
    if (!Visited.insert(MBB).second)
      continue;

    ToFuncReturnVertexes.insert(MBB);
    WorkList.append(MBB->pred_begin(), MBB->pred_end());
  }
}

// Collect cut-vertexes of the CFG, i.e. each blocks that begin a disconnected
// sub-graph of the CFG. Entering a cut-vertex block means that control flow
// will never return to a previously visited block.
void EVMMachineCFGInfo::collectCutVertexes(const MachineBasicBlock *Entry) {
  DenseSet<const MachineBasicBlock *> Visited;
  DenseMap<const MachineBasicBlock *, size_t> Disc;
  DenseMap<const MachineBasicBlock *, size_t> Low;
  DenseMap<const MachineBasicBlock *, const MachineBasicBlock *> Parent;
  size_t Time = 0;
  auto Dfs = [&](const MachineBasicBlock *U, auto Recurse) -> void {
    Visited.insert(U);
    Disc[U] = Low[U] = Time;
    Time++;

    SmallVector<const MachineBasicBlock *> Children(U->predecessors());
    const EVMMBBTerminatorsInfo *UTermInfo = getTerminatorsInfo(U);
    switch (UTermInfo->getExitType()) {
    case MBBExitType::Terminate:
      CutVertexes.insert(U);
      break;
    default:
      Children.append(U->succ_begin(), U->succ_end());
      break;
    }

    for (const MachineBasicBlock *V : Children) {
      // Ignore the loop edge, as it cannot be the bridge.
      if (V == U)
        continue;

      if (!Visited.count(V)) {
        Parent[V] = U;
        Recurse(V, Recurse);
        Low[U] = std::min(Low[U], Low[V]);
        if (Low[V] > Disc[U]) {
          // U <-> V is a cut edge in the undirected graph
          bool EdgeVtoU = std::count(U->pred_begin(), U->pred_end(), V);
          bool EdgeUtoV = std::count(V->pred_begin(), V->pred_end(), U);
          if (EdgeVtoU && !EdgeUtoV)
            // Cut edge V -> U
            CutVertexes.insert(U);
          else if (EdgeUtoV && !EdgeVtoU)
            // Cut edge U -> v
            CutVertexes.insert(V);
        }
      } else if (V != Parent[U])
        Low[U] = std::min(Low[U], Disc[V]);
    }
  };
  Dfs(Entry, Dfs);
}
