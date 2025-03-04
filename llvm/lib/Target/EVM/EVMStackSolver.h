//===--------- EVMStackSolver.h - Calculate stack states --------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H

#include "EVMStackModel.h"
#include "EVMInstrInfo.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {

class MachineLoopInfo;

/// Returns the number of operations required to transform stack \p Source to
/// \p Target.
size_t calculateStackTransformCost(Stack Source, const Stack &Target,
                                   unsigned StackDepthLimit);

// TODO
using BranchInfoTy =
    std::tuple<EVMInstrInfo::BranchType, MachineBasicBlock *,
               MachineBasicBlock *, SmallVector<MachineInstr *, 2>,
               std::optional<MachineOperand>>;

BranchInfoTy getBranchInfo(const MachineBasicBlock *MBB);

class EVMStackSolver {
public:
  EVMStackSolver(const MachineFunction &MF, EVMStackModel &StackModel,
                 const MachineLoopInfo *MLI);

public:
  void run();

private:
  /// Returns the optimal entry stack, s.t. \p MI can be applied to it and the
  /// result can be transformed to \p ExitStack with minimal stack shuffling.
  /// Simultaneously stores the entry stack required for executing the MI.
  Stack propagateStackThroughMI(const Stack &ExitStack, const MachineInstr &MI,
                                bool CompressStack = false);

  /// Given \p ExitStack, compute the stack at the entry of \p MBB.
  /// \par CompressStack: remove duplicates and rematerializable slots.
  Stack propagateStackThroughMBB(const Stack &ExitStack,
                                 const MachineBasicBlock *MBB,
                                 bool CompressStack = false);

  /// Main algorithm walking the graph from entry to exit and propagating back
  /// the stack layouts to the entries. Iteratively reruns itself along
  /// backwards jumps until the layout is stabilized.
  void runPropagation();

#ifndef NDEBUG
  void dump(raw_ostream &OS);
  void dumpMBB(raw_ostream &OS, const MachineBasicBlock *MBB);
#endif

  /// Compute a stack S that minimizes the number of permutations
  /// needed to transform S into \p Stack1 and \p Stack2.
  /// TODO: Compute weighted sum based on branch probabilities.
  Stack combineStack(const Stack &Stack1, const Stack &Stack2);

  /// Returns a copy of \p Stack with duplicates and rematerializable
  /// slots removed.
  /// Used when stackification faces slot accessibility issues.
  /// Otherwise, copies and rematerializable entities are kept on stack.
  Stack compressStack(Stack Stack);

  // Manage StackModel.
  void insertMBBEntryStack(const MachineBasicBlock *MBB, const Stack &S) {
    StackModel.getMBBEntryMap()[MBB] = S;
  }
  void insertMBBExitStack(const MachineBasicBlock *MBB, const Stack &S) {
    StackModel.getMBBExitMap()[MBB] = S;
  }
  void insertInstEntryStack(const MachineInstr *MI, const Stack &S) {
    StackModel.getInstEntryMap()[MI] = S;
  }
  void insertMBBEntryStack(const MachineBasicBlock *MBB, Stack &&S) {
    StackModel.getMBBEntryMap()[MBB] = std::move(S);
  }
  void insertMBBExitStack(const MachineBasicBlock *MBB, Stack &&S) {
    StackModel.getMBBExitMap()[MBB] = std::move(S);
  }
  void insertInstEntryStack(const MachineInstr *MI, Stack &&S) {
    StackModel.getInstEntryMap()[MI] = std::move(S);
  }

  const MachineFunction &MF;
  EVMStackModel &StackModel;
  const MachineLoopInfo *MLI;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H
