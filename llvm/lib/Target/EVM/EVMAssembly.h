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

#include "EVM.h"

#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMAssembly {
private:
  using MIIter = MachineBasicBlock::iterator;

  MachineFunction *MF;
  const EVMInstrInfo *TII;

  int StackHeight = 0;
  MIIter CurMIIt;
  MachineBasicBlock *CurMBB;
  DenseSet<const MachineInstr *> AssemblyInstrs;

public:
  EVMAssembly(MachineFunction *MF, const EVMInstrInfo *TII)
      : MF(MF), TII(TII) {}

  // Retrieve the current height of the stack.
  // This does not have to be zero at the MF beginning because of
  // possible arguments.
  int getStackHeight() const;

  void setStackHeight(int Height);

  void setCurrentLocation(MachineBasicBlock *MBB);

  void appendInstruction(MachineInstr *MI);

  void appendSWAPInstruction(unsigned Depth);

  void appendDUPInstruction(unsigned Depth);

  void appendPOPInstruction();

  void appendConstant(const APInt &Val);

  void appendSymbol(MCSymbol *Symbol, unsigned Opcode);

  void appendConstant(uint64_t Val);

  void appendLabel();

  void appendFuncCall(const MachineInstr *MI, const llvm::GlobalValue *Func,
                      int stackAdj, MCSymbol *RetSym = nullptr);

  void appendRet();

  void appendCondJump(MachineInstr *MI, MachineBasicBlock *Target);

  void appendUncondJump(MachineInstr *MI, MachineBasicBlock *Target);

  void appendLabelReference(MCSymbol *Label);

  MCSymbol *createFuncRetSymbol();

  // Erases unused codegen-only instructions and removes register operands
  // of the remaining ones.
  void finalize();

private:
  void stackifyInstruction(MachineInstr *MI);

#ifndef NDEBUG
  void dumpInst(const MachineInstr *MI) const;
#endif // NDEBUG
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H
