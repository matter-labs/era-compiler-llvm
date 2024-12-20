//===--- EVMStackifyCodeEmitter.h - Create stackified MIR  ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file transforms MIR to the 'stackified' MIR.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKIFYCODEEMITTER_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKIFYCODEEMITTER_H

#include "EVMStackLayoutGenerator.h"
#include "EVMSubtarget.h"

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMStackifyCodeEmitter {
public:
  EVMStackifyCodeEmitter(const EVMStackLayout &Layout,
                         const EVMStackModel &StackModel,
                         const EVMMachineCFGInfo &CFGInfo, MachineFunction &MF)
      : Emitter(MF), Layout(Layout), StackModel(StackModel), CFGInfo(CFGInfo),
        MF(MF) {}

  /// Stackify instructions, starting from the first MF's MBB.
  void run();

private:
  class CodeEmitter {
  public:
    explicit CodeEmitter(MachineFunction &MF)
        : MF(MF), TII(MF.getSubtarget<EVMSubtarget>().getInstrInfo()) {}
    size_t stackHeight() const;
    void enterMBB(MachineBasicBlock *MBB, int Height);
    void emitInst(const MachineInstr *MI);
    void emitSWAP(unsigned Depth);
    void emitDUP(unsigned Depth);
    void emitPOP();
    void emitConstant(const APInt &Val);
    void emitConstant(uint64_t Val);
    void emitSymbol(const MachineInstr *MI, MCSymbol *Symbol);
    void emitFuncCall(const MachineInstr *MI);
    void emitRet(const MachineInstr *MI);
    void emitCondJump(const MachineInstr *MI, MachineBasicBlock *Target);
    void emitUncondJump(const MachineInstr *MI, MachineBasicBlock *Target);
    void emitLabelReference(const MachineInstr *Call);
    /// Remove all the instructions that are not stackified and set that all
    /// instructions are stackified in a function from now on. Also, invalidate
    /// the register liveness, as it has no meaning in a stackified code.
    void finalize();

  private:
    MachineFunction &MF;
    const EVMInstrInfo *TII;
    size_t StackHeight = 0;
    MachineBasicBlock *CurMBB{};
    DenseMap<const MachineInstr *, MCSymbol *> CallReturnSyms;

    void verify(const MachineInstr *MI) const;
  };

  CodeEmitter Emitter;
  const EVMStackLayout &Layout;
  const EVMStackModel &StackModel;
  const EVMMachineCFGInfo &CFGInfo;
  MachineFunction &MF;
  Stack CurrentStack;

  /// Checks if it's valid to transition from \p SourceStack to \p
  /// TargetStack, that is \p SourceStack matches each slot in \p
  /// TargetStack that is not a JunkSlot exactly.
  bool areLayoutsCompatible(const Stack &SourceStack, const Stack &TargetStack);

  /// Shuffles CurrentStack to the desired \p TargetStack.
  void createStackLayout(const Stack &TargetStack);

  /// Creates the Op.Input stack layout from the 'CurrentStack' taking into
  /// account commutative property of the operation.
  void createOperationLayout(const Operation &Op);

  /// Remove the arguments from the stack and push the return values.
  void adjustStackForInst(const MachineInstr *MI, size_t NumArgs);

  /// Generate code for the function call \p Call.
  void visitCall(const FunctionCall &Call);
  /// Generate code for the builtin call \p Call.
  void visitInst(const BuiltinCall &Call);
  /// Generate code for the assignment \p Assignment.
  void visitAssign(const Assignment &Assignment);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKIFYCODEEMITTER_H
