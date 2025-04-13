//===--------- EVMStackSolver.h - Calculate stack states --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H

#include "EVMInstrInfo.h"
#include "EVMStackModel.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {

class MachineLoopInfo;

/// Compute the gas cost to transform \p Source into \p Target.
/// \note The copy of \p Source is intentional because the function modifies
/// it during computation.
std::optional<unsigned> calculateStackTransformCost(Stack Source,
                                                    const Stack &Target,
                                                    unsigned StackDepthLimit);

using BranchInfoTy =
    std::tuple<EVMInstrInfo::BranchType, MachineBasicBlock *,
               MachineBasicBlock *, SmallVector<MachineInstr *, 2>,
               std::optional<MachineOperand>>;

BranchInfoTy getBranchInfo(const MachineBasicBlock *MBB);

class EVMStackSolver {
public:
  EVMStackSolver(const MachineFunction &MF, EVMStackModel &StackModel,
                 const MachineLoopInfo *MLI);
  void run();

private:
  /// Given \p MI's \p ExitStack, compute the entry stack so that after
  /// executing the instruction the cost to transform to \p ExitStack is
  /// minimal.
  /// Side effect: the computed entry stack is stored in StackModel.
  /// \param CompressStack: remove duplicates and rematerializable slots.
  std::pair<Stack, bool> propagateThroughMI(const Stack &ExitStack,
                                            const MachineInstr &MI,
                                            bool CompressStack = false);

  /// Given \p ExitStack, compute the stack at the entry of \p MBB.
  /// \param CompressStack: remove duplicates and rematerializable slots.
  Stack propagateThroughMBB(const Stack &ExitStack,
                            const MachineBasicBlock *MBB,
                            bool CompressStack = false);

  /// Main algorithm walking the graph from entry to exit and propagating stack
  /// states back to the entries. Iteratively reruns itself along backward jumps
  /// until the state is stabilized.
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
