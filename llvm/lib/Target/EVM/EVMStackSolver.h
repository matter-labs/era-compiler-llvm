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
#include "llvm/ADT/DenseMap.h"

namespace llvm {

class EVMMachineCFGInfo;
class MachineLoopInfo;

/// Returns the number of operations required to transform stack \p Source to
/// \p Target.
size_t calculateStackTransformCost(Stack Source, const Stack &Target,
                                   unsigned StackDepthLimit);

class EVMStackSolver {
public:
  EVMStackSolver(const MachineFunction &MF, EVMStackModel &StackModel,
                 const MachineLoopInfo *MLI, const EVMMachineCFGInfo &CFGInfo);

public:
  void run();

private:
  /// Returns the optimal entry stack, s.t. \p MI can be applied to it and the
  /// result can be transformed to \p ExitStack with minimal stack shuffling.
  /// Simultaneously stores the entry stack required for executing the MI.
  Stack propagateStackThroughMI(const Stack &ExitStack, const MachineInstr &MI,
                                bool CompressStack = false);

  /// Returns the desired stack layout at the entry of \p MBB, assuming the
  /// layout after executing the block should be \p ExitStack.
  Stack propagateStackThroughMBB(const Stack &ExitStack,
                                 const MachineBasicBlock *MBB,
                                 bool CompressStack = false);

  /// Main algorithm walking the graph from entry to exit and propagating back
  /// the stack layouts to the entries. Iteratively reruns itself along
  /// backwards jumps until the layout is stabilized.
  void runPropagation();

  /// Adds junks to the subgraph starting at \p Entry. It should only be
  /// called on cut-vertices, so the full subgraph retains proper stack balance.
  void appendJunks(const MachineBasicBlock *Entry, size_t NumJunk);

#ifndef NDEBUG
  void dump(raw_ostream &OS);
  void dumpMBB(raw_ostream &OS, const MachineBasicBlock *MBB);
#endif

  /// Calculates the ideal stack, s.t., both \p Stack1 and \p Stack2 can
  /// be achieved with minimal stack shuffling.
  Stack combineStack(const Stack &Stack1, const Stack &Stack2);

  /// Returns a copy of \p Stack stripped of all duplicates and slots that can
  /// be freely generated. Attempts to create a layout that requires a minimal
  /// amount of operations to reconstruct the original stack \p Stack.
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
  const EVMMachineCFGInfo &CFGInfo;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H
