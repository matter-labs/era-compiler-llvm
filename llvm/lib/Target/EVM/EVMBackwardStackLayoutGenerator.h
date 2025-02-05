//===---- EVMBackwardStackLayoutGenerator.h - Stack layout gen --*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_EVM_EVMBACKWARDSTACKLAYOUTGENERATOR_H
#define LLVM_LIB_TARGET_EVM_EVMBACKWARDSTACKLAYOUTGENERATOR_H

#include "EVMStackModel.h"
#include "llvm/ADT/DenseMap.h"
#include <deque>

namespace llvm {

class EVMMachineCFGInfo;
class MachineLoopInfo;

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

class EVMBackwardStackLayoutGenerator {
public:
  EVMBackwardStackLayoutGenerator(const MachineFunction &MF,
                                  const MachineLoopInfo *MLI,
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
  void runPropagation();

  /// Adds junks to the subgraph starting at \p Entry. It should only be
  /// called on cut-vertices, so the full subgraph retains proper stack balance.
  void addJunksToStackBottom(const MachineBasicBlock *Entry, size_t NumJunk);

#ifndef NDEBUG
  void dump(raw_ostream &OS);
  void printBlock(raw_ostream &OS, const MachineBasicBlock &Block);
  std::string getBlockId(const MachineBasicBlock &Block);
  DenseMap<const MachineBasicBlock *, size_t> BlockIds;
  size_t BlockCount = 0;
#endif

  /// Returns the best known exit layout of \p Block, if all dependencies are
  /// already \p Visited. If not, adds the dependencies to \p DependencyList and
  /// returns std::nullopt.
  std::optional<Stack> getExitLayoutOrStageDependencies(
      const MachineBasicBlock *Block,
      const DenseSet<const MachineBasicBlock *> &Visited,
      std::deque<const MachineBasicBlock *> &ToVisit) const;

  const MachineFunction &MF;
  const MachineLoopInfo *MLI;
  const EVMStackModel &StackModel;
  const EVMMachineCFGInfo &CFGInfo;

  DenseMap<const MachineBasicBlock *, Stack> MBBEntryLayoutMap;
  DenseMap<const MachineBasicBlock *, Stack> MBBExitLayoutMap;
  DenseMap<const Operation *, Stack> OperationEntryLayoutMap;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMBACKWARDSTACKLAYOUTGENERATOR_H
