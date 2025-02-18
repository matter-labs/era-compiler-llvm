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
#include <deque>

namespace llvm {

class EVMMachineCFGInfo;
class MachineLoopInfo;

/// Returns the number of operations required to transform stack \p Source to
/// \p Target.
size_t calculateStackTransformCost(Stack Source, const Stack &Target,
                                   unsigned StackDepthLimit);

class EVMMIRToStack {
public:
  EVMMIRToStack(DenseMap<const MachineBasicBlock *, Stack> &MBBEntryMap,
                DenseMap<const MachineBasicBlock *, Stack> &MBBExitMap,
                DenseMap<const Operation *, Stack> &OpsEntryMap)
      : MBBEntryMap(MBBEntryMap), MBBExitMap(MBBExitMap),
        OperationEntryMap(OpsEntryMap) {}
  EVMMIRToStack(const EVMMIRToStack &) = delete;
  EVMMIRToStack &operator=(const EVMMIRToStack &) = delete;

  const Stack &getMBBEntryMap(const MachineBasicBlock *MBB) const {
    return MBBEntryMap.at(MBB);
  }

  const Stack &getMBBExitMap(const MachineBasicBlock *MBB) const {
    return MBBExitMap.at(MBB);
  }

  const Stack &getOperationEntryMap(const Operation *Op) const {
    return OperationEntryMap.at(Op);
  }

private:
  // Complete stack required at MBB entry.
  DenseMap<const MachineBasicBlock *, Stack> MBBEntryMap;
  // Complete stack required at MBB exit.
  DenseMap<const MachineBasicBlock *, Stack> MBBExitMap;
  // Complete stack that required for the instruction at the stack top.
  DenseMap<const Operation *, Stack> OperationEntryMap;
};

class EVMStackSolver {
public:
  struct StackTooDeep {
    /// Number of slots that need to be saved.
    size_t Deficit = 0;
    /// Set of variables, eliminating which would decrease the stack deficit.
    SmallVector<Register> VariableChoices;
  };

  EVMStackSolver(const MachineFunction &MF, const MachineLoopInfo *MLI,
                 const EVMStackModel &StackModel,
                 const EVMMachineCFGInfo &CFGInfo);

  EVMMIRToStack run();

private:
  /// Returns the optimal entry stack layout, s.t. \p Op can be applied
  /// to it and the result can be transformed to \p ExitStack with minimal stack
  /// shuffling. Simultaneously stores the entry layout required for executing
  /// the operation in the map.
  Stack propagateStackThroughInst(const Stack &ExitStack, const Operation &Op,
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
  void printMBB(raw_ostream &OS, const MachineBasicBlock *MBB);
  std::string getBlockId(const MachineBasicBlock *MBB);
  DenseMap<const MachineBasicBlock *, size_t> BlockIds;
  size_t BlockCount = 0;
#endif

  /// Returns the best known exit stack of \p MBB, if all dependencies are
  /// already \p Visited. If not, adds the dependencies to \p DependencyList and
  /// returns std::nullopt.
  std::optional<Stack> getExitStackOrStageDependencies(
      const MachineBasicBlock *MBB,
      const DenseSet<const MachineBasicBlock *> &Visited,
      std::deque<const MachineBasicBlock *> &DependencyList);

  /// Calculates the ideal stack, s.t., both \p Stack1 and \p Stack2 can
  /// be achieved with minimal stack shuffling.
  Stack combineStack(const Stack &Stack1, const Stack &Stack2);

  /// Walks through the CFG and reports any stack too deep errors that would
  /// occur when generating code for it without countermeasures.
  SmallVector<StackTooDeep>
  reportStackTooDeep(const MachineBasicBlock &Entry) const;

  /// Returns a copy of \p Stack stripped of all duplicates and slots that can
  /// be freely generated. Attempts to create a layout that requires a minimal
  /// amount of operations to reconstruct the original stack \p Stack.
  Stack compressStack(Stack Stack);

  const MachineFunction &MF;
  const MachineLoopInfo *MLI;
  const EVMStackModel &StackModel;
  const EVMMachineCFGInfo &CFGInfo;

  DenseMap<const MachineBasicBlock *, Stack> MBBEntryMap;
  DenseMap<const MachineBasicBlock *, Stack> MBBExitMap;
  DenseMap<const Operation *, Stack> OperationEntryMap;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKSOLVER_H
