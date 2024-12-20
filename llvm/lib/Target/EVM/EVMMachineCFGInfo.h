//===-------- EVMMachineCFGInfo.h - Machine CFG info -----------*- C++ --*-===//
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

#ifndef LLVM_LIB_TARGET_EVM_EVMMACHINE_CFG_INFO_H
#define LLVM_LIB_TARGET_EVM_EVMMACHINE_CFG_INFO_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/CodeGen/MachineOperand.h"

#include <memory>

namespace llvm {

class LiveIntervals;
class MachineFunction;
class MachineBasicBlock;
class MachineLoopInfo;
class TargetInstrInfo;

enum class MBBExitType {
  Invalid,
  ConditionalBranch,
  UnconditionalBranch,
  FunctionReturn,
  Terminate
};

class EVMMBBTerminatorsInfo {
  friend class EVMMachineCFGInfo;

  union BranchInfoUnion {
    BranchInfoUnion() {}
    struct {
      const MachineOperand *Condition;
      MachineBasicBlock *TrueBB;
      MachineBasicBlock *FalseBB;
      MachineInstr *CondBr;
      MachineInstr *UncondBr;
    } Conditional;

    struct {
      MachineBasicBlock *TargetBB;
      MachineInstr *Br;
      bool IsLatch;
    } Unconditional;
  } BranchInfo;

  MBBExitType ExitType = MBBExitType::Invalid;
  MachineInstr *LastTerm = nullptr;

public:
  MBBExitType getExitType() const {
    assert(ExitType != MBBExitType::Invalid);
    return ExitType;
  }

  std::tuple<MachineInstr *, MachineInstr *, MachineBasicBlock *,
             MachineBasicBlock *, const MachineOperand *>
  getConditionalBranch() const {
    assert(ExitType == MBBExitType::ConditionalBranch);
    return {BranchInfo.Conditional.CondBr, BranchInfo.Conditional.UncondBr,
            BranchInfo.Conditional.TrueBB, BranchInfo.Conditional.FalseBB,
            BranchInfo.Conditional.Condition};
  }

  std::tuple<MachineInstr *, MachineBasicBlock *, bool>
  getUnconditionalBranch() const {
    assert(ExitType == MBBExitType::UnconditionalBranch);
    return {BranchInfo.Unconditional.Br, BranchInfo.Unconditional.TargetBB,
            BranchInfo.Unconditional.IsLatch};
  }

  MachineInstr *getFunctionReturn() const {
    assert(ExitType == MBBExitType::FunctionReturn);
    return LastTerm;
  }
};

class EVMMachineCFGInfo {
public:
  EVMMachineCFGInfo(const EVMMachineCFGInfo &) = delete;
  EVMMachineCFGInfo &operator=(const EVMMachineCFGInfo &) = delete;
  EVMMachineCFGInfo(MachineFunction &MF, MachineLoopInfo *MLI);

  const EVMMBBTerminatorsInfo *
  getTerminatorsInfo(const MachineBasicBlock *MBB) const;

  bool isCutVertex(const MachineBasicBlock *MBB) const {
    return CutVertexes.count(MBB) > 0;
  }

  bool isOnPathToFuncReturn(const MachineBasicBlock *MBB) const {
    return ToFuncReturnVertexes.count(MBB) > 0;
  }

private:
  DenseMap<const MachineBasicBlock *, std::unique_ptr<EVMMBBTerminatorsInfo>>
      MBBTerminatorsInfoMap;
  DenseSet<const MachineBasicBlock *> ToFuncReturnVertexes;
  DenseSet<const MachineBasicBlock *> CutVertexes;

  void collectTerminatorsInfo(const TargetInstrInfo *TII,
                              const MachineLoopInfo *MLI,
                              MachineBasicBlock &MBB);
  void collectBlocksLeadingToFunctionReturn(
      const SmallVector<const MachineBasicBlock *> &Returns);
  void collectCutVertexes(const MachineBasicBlock *Entry);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMMACHINE_CFG_INFO_H
