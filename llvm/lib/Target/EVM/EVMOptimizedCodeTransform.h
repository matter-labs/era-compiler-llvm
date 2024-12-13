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
#include "EVMStackLayoutGenerator.h"

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMOptimizedCodeTransform {
public:
  EVMOptimizedCodeTransform(const StackLayout &Layout, MachineFunction &MF)
      : Assembly(MF), Layout(Layout), MF(MF) {}

  /// Stackify instructions, starting from the \p EntryBB.
  void run(CFG::BasicBlock &EntryBB);

private:
  EVMAssembly Assembly;
  const StackLayout &Layout;
  const MachineFunction &MF;
  Stack CurrentStack;

  /// Checks if it's valid to transition from \p SourceStack to \p
  /// TargetStack, that is \p SourceStack matches each slot in \p
  /// TargetStack that is not a JunkSlot exactly.
  bool areLayoutsCompatible(const Stack &SourceStack, const Stack &TargetStack);

  /// Shuffles CurrentStack to the desired \p TargetStack.
  void createStackLayout(const Stack &TargetStack);

  /// Creates the Op.Input stack layout from the 'CurrentStack' taking into
  /// account commutative property of the operation.
  void createOperationLayout(const CFG::Operation &Op);

  /// Generate code for the function call \p Call.
  void visitCall(const CFG::FunctionCall &Call);
  /// Generate code for the builtin call \p Call.
  void visitInst(const CFG::BuiltinCall &Call);
  /// Generate code for the assignment \p Assignment.
  void visitAssign(const CFG::Assignment &Assignment);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H
