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

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTGENERATOR_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTGENERATOR_H

#include "EVMMachineCFGInfo.h"
#include "EVMStackModel.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {

/// Returns the number of operations required to transform stack \p Source to
/// \p Target.
size_t EvaluateStackTransform(Stack Source, Stack const &Target);

class EVMStackLayout {
public:
  EVMStackLayout(DenseMap<const MachineBasicBlock *, Stack> &MBBEntryLayout,
                 DenseMap<const MachineBasicBlock *, Stack> &MBBExitLayout,
                 DenseMap<const Operation *, Stack> &OpsEntryLayout)
      : MBBEntryLayoutMap(MBBEntryLayout), MBBExitLayoutMap(MBBExitLayout),
        OperationEntryLayoutMap(OpsEntryLayout) {}
  EVMStackLayout(const EVMStackLayout &) = delete;
  EVMStackLayout &operator=(const EVMStackLayout &) = delete;

  const Stack &getMBBEntryLayout(const MachineBasicBlock *MBB) const {
    return MBBEntryLayoutMap.at(MBB);
  }

  const Stack &getMBBExitLayout(const MachineBasicBlock *MBB) const {
    return MBBExitLayoutMap.at(MBB);
  }

  const Stack &getOperationEntryLayout(const Operation *Op) const {
    return OperationEntryLayoutMap.at(Op);
  }

private:
  // Complete stack layout required at MBB entry.
  DenseMap<const MachineBasicBlock *, Stack> MBBEntryLayoutMap;
  // Complete stack layout required at MBB exit.
  DenseMap<const MachineBasicBlock *, Stack> MBBExitLayoutMap;
  // Complete stack layout that
  // has the slots required for the operation at the stack top.
  DenseMap<const Operation *, Stack> OperationEntryLayoutMap;
};

class EVMStackLayoutGenerator {
public:
  struct StackTooDeep {
    /// Number of slots that need to be saved.
    size_t deficit = 0;
    /// Set of variables, eliminating which would decrease the stack deficit.
    SmallVector<Register> variableChoices;
  };

  EVMStackLayoutGenerator(const MachineFunction &MF,
                          const EVMStackModel &StackModel,
                          const EVMMachineCFGInfo &CFGInfo);

  std::unique_ptr<EVMStackLayout> run();

private:
  /// Returns the optimal entry stack layout, s.t. \p Operation can be applied
  /// to it and the result can be transformed to \p ExitStack with minimal stack
  /// shuffling. Simultaneously stores the entry layout required for executing
  /// the operation in the map.
  Stack propagateStackThroughOperation(Stack ExitStack,
                                       Operation const &Operation,
                                       bool AggressiveStackCompression = false);

  /// Returns the desired stack layout at the entry of \p Block, assuming the
  /// layout after executing the block should be \p ExitStack.
  Stack propagateStackThroughBlock(Stack ExitStack,
                                   const MachineBasicBlock *Block,
                                   bool AggressiveStackCompression = false);

  /// Main algorithm walking the graph from entry to exit and propagating back
  /// the stack layouts to the entries. Iteratively reruns itself along
  /// backwards jumps until the layout is stabilized.
  void processEntryPoint(const MachineBasicBlock *Entry);

  /// Returns the best known exit layout of \p Block, if all dependencies are
  /// already \p Visited. If not, adds the dependencies to \p DependencyList and
  /// returns std::nullopt.
  std::optional<Stack> getExitLayoutOrStageDependencies(
      const MachineBasicBlock *Block,
      const DenseSet<const MachineBasicBlock *> &Visited,
      std::list<const MachineBasicBlock *> &DependencyList) const;

  /// Returns a pair of '{jumpingBlock, targetBlock}' for each backwards jump
  /// in the graph starting at \p Entry.
  std::list<std::pair<const MachineBasicBlock *, const MachineBasicBlock *>>
  collectBackwardsJumps(const MachineBasicBlock *Entry) const;

  /// After the main algorithms, layouts at conditional jumps are merely
  /// compatible, i.e. the exit layout of the jumping block is a superset of the
  /// entry layout of the target block. This function modifies the entry layouts
  /// of conditional jump targets, s.t., the entry layout of target blocks match
  /// the exit layout of the jumping block exactly, except that slots not
  /// required after the jump are marked as 'JunkSlot's.
  void stitchConditionalJumps(const MachineBasicBlock *Block);

  /// Calculates the ideal stack layout, s.t., both \p Stack1 and \p Stack2 can
  /// be achieved with minimal stack shuffling when starting from the returned
  /// layout.
  static Stack combineStack(const Stack &Stack1, const Stack &Stack2);

  /// Walks through the CFG and reports any stack too deep errors that would
  /// occur when generating code for it without countermeasures.
  SmallVector<StackTooDeep>
  reportStackTooDeep(const MachineBasicBlock &Entry) const;

  /// Returns a copy of \p Stack stripped of all duplicates and slots that can
  /// be freely generated. Attempts to create a layout that requires a minimal
  /// amount of operations to reconstruct the original stack \p Stack.
  static Stack compressStack(Stack Stack);

  /// Fills in junk when entering branches that do not need a clean stack in
  /// case the result is cheaper.
  void fillInJunk(const MachineBasicBlock *Block);

  const MachineFunction &MF;
  const EVMStackModel &StackModel;
  const EVMMachineCFGInfo &CFGInfo;

  DenseMap<const MachineBasicBlock *, Stack> MBBEntryLayoutMap;
  DenseMap<const MachineBasicBlock *, Stack> MBBExitLayoutMap;
  DenseMap<const Operation *, Stack> OperationEntryLayoutMap;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTGENERATOR_H
