//===------------- EVMAssembly.h - EVM Assembly generator -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVMAssembly class that generates machine IR
// with all the required stack manipulation instructions.
// Resulting machine instructions still have explicit operands, but some of the
// auxiliary instructions (ARGUMENT, RET, EVM::CONST_I256, COPY_I256
// FCALLARGUMENT) are removed after this step, beaking use-def chains. So, the
// resulting Machine IR breaks the MachineVerifier checks.
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
  // This does not have to be zero at the beginning.
  int getStackHeight() const;

  void setStackHeight(int Height);

  void setCurrentLocation(MachineBasicBlock *MBB);

  void appendInstruction(MachineInstr *MI);

  void appendSWAPInstruction(unsigned Depth);

  void appendDUPInstruction(unsigned Depth);

  void appendPOPInstruction();

  void appendConstant(const APInt &Val);

  void appendSymbol(MCSymbol *Symbol);

  void appendConstant(uint64_t Val);

  void appendLabel();

  void appendFuncCall(const MachineInstr *MI, const llvm::GlobalValue *Func,
                      int stackAdj, MCSymbol *RetSym = nullptr);

  void appendJump(int stackAdj);

  void appendCondJump(MachineInstr *MI, MachineBasicBlock *Target);

  void appendUncondJump(MachineInstr *MI, MachineBasicBlock *Target);

  void appendLabelReference(MCSymbol *Label);

  void appendMBBReference(MachineBasicBlock *MBB);

  MCSymbol *createFuncRetSymbol();

  // Removes unused codegen-only instructions and
  // stackifies remaining ones.
  void finalize();

private:
  void stackifyInstruction(MachineInstr *MI);

#ifndef NDEBUG
  void dumpInst(const MachineInstr *MI) const;
#endif // NDEBUG
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H
