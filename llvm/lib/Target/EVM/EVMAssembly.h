//===------------- EVMAssembly.h - EVM Assembly generator -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file creates Machine IR in stackified form. It provides different
// callbacks when the EVMOptimizedCodeTransform needs to emit operation,
// stack manipulation instruction, and so on. It the end, it walks over MIR
// instructions removing register operands.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H
#define LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H

#include "EVMSubtarget.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMAssembly {
private:
  MachineFunction &MF;
  const EVMInstrInfo *TII;
  int StackHeight = 0;
  MachineBasicBlock *CurMBB{};
  DenseMap<const MachineInstr *, MCSymbol *> CallReturnSyms;

public:
  explicit EVMAssembly(MachineFunction &MF)
      : MF(MF), TII(MF.getSubtarget<EVMSubtarget>().getInstrInfo()) {}

  // Retrieve the current height of the stack.
  // This does not have to be zero at the MF beginning because of
  // possible arguments.
  int getStackHeight() const;

  void init(MachineBasicBlock *MBB, int Height);

  void emitInst(const MachineInstr *MI);

  void emitSWAP(unsigned Depth);

  void emitDUP(unsigned Depth);

  void emitPOP();

  void emitConstant(const APInt &Val);

  void emitSymbol(const MachineInstr *MI, MCSymbol *Symbol);

  void emitConstant(uint64_t Val);

  void emitFuncCall(const MachineInstr *MI, const GlobalValue *Func,
                    int stackAdj, bool WillReturn);

  void emitRet(const MachineInstr *MI);

  void emitCondJump(const MachineInstr *MI, MachineBasicBlock *Target);

  void emitUncondJump(const MachineInstr *MI, MachineBasicBlock *Target);

  void emitLabelReference(const MachineInstr *Call);

  // Erases unused codegen-only instructions.
  void finalize();

private:
  void verify(const MachineInstr *MI) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H
