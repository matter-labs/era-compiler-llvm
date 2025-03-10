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

#include "EVMStackModel.h"
#include "EVMSubtarget.h"

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMStackifyCodeEmitter {
public:
  EVMStackifyCodeEmitter(const EVMStackModel &StackModel, MachineFunction &MF)
      : Emitter(MF), StackModel(StackModel), MF(MF) {}

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
    /// Remove all the instructions that are not in stack form.
    void finalize();

  private:
    MachineFunction &MF;
    const EVMInstrInfo *TII;
    size_t StackHeight = 0;
    MachineBasicBlock *CurMBB = nullptr;
    DenseMap<const MachineInstr *, MCSymbol *> CallReturnSyms;

    void verify(const MachineInstr *MI) const;
  };

  CodeEmitter Emitter;
  const EVMStackModel &StackModel;
  MachineFunction &MF;
  Stack CurrentStack;

  /// Shuffles CurrentStack to the desired \p TargetStack.
  void emitMBBExitStack(const Stack &TargetStack);

  /// Creates the MI's entry stack from the 'CurrentStack' taking into
  /// account commutative property of the instruction.
  void emitMIEntryStack(const MachineInstr &MI);

  /// Remove the arguments from the stack and push the return values.
  void adjustStackForInst(const MachineInstr *MI, size_t NumArgs);

  /// Generate code for the instruction.
  void emitMI(const MachineInstr &MI);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKIFYCODEEMITTER_H
