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

#include "EVMControlFlowGraph.h"
#include <map>
#include <set>

namespace llvm {

struct StackLayout {
  struct BlockInfo {
    /// Complete stack layout that is required for entering a block.
    Stack entryLayout;
    /// The resulting stack layout after executing the block.
    Stack exitLayout;
  };
  std::map<CFG::BasicBlock const *, BlockInfo> blockInfos;
  /// For each operation the complete stack layout that:
  /// - has the slots required for the operation at the stack top.
  /// - will have the operation result in a layout that makes it easy to achieve
  /// the next desired layout.
  std::map<CFG::Operation const *, Stack> operationEntryLayout;
};

class StackLayoutGenerator {
public:
  struct StackTooDeep {
    /// Number of slots that need to be saved.
    size_t deficit = 0;
    /// Set of variables, eliminating which would decrease the stack deficit.
    std::vector<Register> variableChoices;
  };

  static StackLayout run(CFG const &Cfg);
  /// Returns all stack too deep errors for the given CFG.
  static std::vector<StackTooDeep> reportStackTooDeep(CFG const &Cfg);

private:
  StackLayoutGenerator(StackLayout &Context,
                       CFG::FunctionInfo const *FunctionInfo);

  /// Returns the optimal entry stack layout, s.t. \p Operation can be applied
  /// to it and the result can be transformed to \p ExitStack with minimal stack
  /// shuffling. Simultaneously stores the entry layout required for executing
  /// the operation in Layout.
  Stack propagateStackThroughOperation(Stack ExitStack,
                                       CFG::Operation const &Operation,
                                       bool AggressiveStackCompression = false);

  /// Returns the desired stack layout at the entry of \p Block, assuming the
  /// layout after executing the block should be \p ExitStack.
  Stack propagateStackThroughBlock(Stack ExitStack,
                                   CFG::BasicBlock const &block,
                                   bool AggressiveStackCompression = false);

  /// Main algorithm walking the graph from entry to exit and propagating back
  /// the stack layouts to the entries. Iteratively reruns itself along
  /// backwards jumps until the layout is stabilized.
  void processEntryPoint(CFG::BasicBlock const &Entry,
                         CFG::FunctionInfo const *FunctionInfo = nullptr);

  /// Returns the best known exit layout of \p Block, if all dependencies are
  /// already \p Visited. If not, adds the dependencies to \p DependencyList and
  /// returns std::nullopt.
  std::optional<Stack> getExitLayoutOrStageDependencies(
      CFG::BasicBlock const &Block,
      std::set<CFG::BasicBlock const *> const &Visited,
      std::list<CFG::BasicBlock const *> &DependencyList) const;

  /// Returns a pair of ``{jumpingBlock, targetBlock}`` for each backwards jump
  /// in the graph starting at \p Eentry.
  std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
  collectBackwardsJumps(CFG::BasicBlock const &Entry) const;

  /// After the main algorithms, layouts at conditional jumps are merely
  /// compatible, i.e. the exit layout of the jumping block is a superset of the
  /// entry layout of the target block. This function modifies the entry layouts
  /// of conditional jump targets, s.t. the entry layout of target blocks match
  /// the exit layout of the jumping block exactly, except that slots not
  /// required after the jump are marked as `JunkSlot`s.
  void stitchConditionalJumps(CFG::BasicBlock const &Block);

  /// Calculates the ideal stack layout, s.t. both \p Stack1 and \p Stack2 can
  /// be achieved with minimal stack shuffling when starting from the returned
  /// layout.
  static Stack combineStack(Stack const &Stack1, Stack const &Stack2);

  /// Walks through the CFG and reports any stack too deep errors that would
  /// occur when generating code for it without countermeasures.
  std::vector<StackTooDeep>
  reportStackTooDeep(CFG::BasicBlock const &Entry) const;

  /// Returns a copy of \p Stack stripped of all duplicates and slots that can
  /// be freely generated. Attempts to create a layout that requires a minimal
  /// amount of operations to reconstruct the original stack \p Stack.
  static Stack compressStack(Stack Stack);

  //// Fills in junk when entering branches that do not need a clean stack in
  ///case the result is cheaper.
  void fillInJunk(CFG::BasicBlock const &Block,
                  CFG::FunctionInfo const *FunctionInfo = nullptr);

  StackLayout &Layout;
  CFG::FunctionInfo const *CurrentFunctionInfo = nullptr;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUTGENERATOR_H
