//===--- EVMOptimizedCodeTransform.h - Create stackified MIR  ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file transforms MIR to the 'stackified' MIR using CFG, StackLayout
// and EVMAssembly classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H
#define LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H

#include "EVMAssembly.h"
#include "EVMControlFlowGraph.h"
#include "EVMStackLayoutGenerator.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include <stack>

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMOptimizedCodeTransform {
public:
  static void run(EVMAssembly &Assembly, MachineFunction &MF,
                  const LiveIntervals &LIS, MachineLoopInfo *MLI);

  /// Generate code for the function call \p Call. Only public for using with
  /// std::visit.
  void operator()(CFG::FunctionCall const &Call);
  /// Generate code for the builtin call \p Call. Only public for using with
  /// std::visit.
  void operator()(CFG::BuiltinCall const &Call);
  /// Generate code for the assignment \p Assignment. Only public for using
  /// with std::visit.
  void operator()(CFG::Assignment const &Assignment);

private:
  EVMOptimizedCodeTransform(EVMAssembly &Assembly, const CFG &Cfg,
                            const StackLayout &Layout, MachineFunction &MF);

  /// Checks if it's valid to transition from \p SourceStack to \p
  /// TargetStack, that is \p SourceStack matches each slot in \p
  /// TargetStack that is not a JunkSlot exactly.
  static bool AreLayoutsCompatible(Stack const &SourceStack,
                                   Stack const &TargetStack);

  /// Shuffles CurrentStack to the desired \p TargetStack while emitting the
  /// shuffling code to Assembly.
  void createStackLayout(Stack TargetStack);

  /// Creates the Op.Input stack layout from the 'CurrentStack' taking into
  /// account commutative property of the operation.
  void createOperationEntryLayout(const CFG::Operation &Op);

  /// Generate code for the given block \p Block.
  /// Expects the current stack layout 'CurrentStack' to be a stack layout that
  /// is compatible with the entry layout expected by the block. Recursively
  /// generates code for blocks that are jumped to. The last emitted assembly
  /// instruction is always an unconditional jump or terminating. Always exits
  /// with an empty stack layout.
  void operator()(CFG::BasicBlock const &Block);

  /// Generate code for the given function.
  /// Resets CurrentStack.
  void operator()();

  EVMAssembly &Assembly;
  StackLayout const &Layout;
  CFG::FunctionInfo const *FuncInfo = nullptr;
  MachineFunction &MF;
  const CFG::BasicBlock &EntryBB;
  const std::vector<StackSlot> &Parameters;

  Stack CurrentStack;
  DenseMap<const MachineInstr *, MCSymbol *> CallToReturnMCSymbol;
  DenseMap<const CFG::BasicBlock *, MCSymbol *> BlockLabels;
  /// Set of blocks already generated. If any of the contained blocks is ever
  /// jumped to, BlockLabels should contain a jump label for it.
  std::set<CFG::BasicBlock const *> GeneratedBlocks;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H
