//===----------- EVMAssembly.cpp - EVM Assembly generator -------*- C++ -*-===//
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

#include "EVMAssembly.h"
#include "EVMMachineFunctionInfo.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "evm-assembly"

int EVMAssembly::getStackHeight() const { return StackHeight; }

void EVMAssembly::init(MachineBasicBlock *MBB, int Height) {
  StackHeight = Height;
  CurMBB = MBB;
  LLVM_DEBUG(dbgs() << "\n"
                    << "Set stack height: " << StackHeight << "\n");
  LLVM_DEBUG(dbgs() << "Setting current location to: " << MBB->getNumber()
                    << "." << MBB->getName() << "\n");
}

void EVMAssembly::emitInst(const MachineInstr *MI) {
  unsigned Opc = MI->getOpcode();
  assert(Opc != EVM::JUMP && Opc != EVM::JUMPI && Opc != EVM::ARGUMENT &&
         Opc != EVM::RET && Opc != EVM::CONST_I256 && Opc != EVM::COPY_I256 &&
         Opc != EVM::FCALL && "Unexpected instruction");

  // The stack height is increased by the number of defs and decreased
  // by the number of inputs. To get the number of inputs, we subtract
  // the total number of operands from the number of defs, so the
  // calculation is as follows:
  // Defs - (Ops - Defs) = 2 * Defs - Ops
  int StackAdj = (2 * static_cast<int>(MI->getNumExplicitDefs())) -
                 static_cast<int>(MI->getNumExplicitOperands());
  StackHeight += StackAdj;

  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMAssembly::emitSWAP(unsigned Depth) {
  unsigned Opc = EVM::getSWAPOpcode(Depth);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMAssembly::emitDUP(unsigned Depth) {
  StackHeight += 1;
  unsigned Opc = EVM::getDUPOpcode(Depth);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMAssembly::emitPOP() {
  StackHeight -= 1;
  assert(StackHeight >= 0);
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::POP_S));
  verify(NewMI);
}

void EVMAssembly::emitConstant(const APInt &Val) {
  StackHeight += 1;
  unsigned Opc = EVM::getPUSHOpcode(Val);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  if (Opc != EVM::PUSH0)
    NewMI.addCImm(ConstantInt::get(MF.getFunction().getContext(), Val));
  verify(NewMI);
}

void EVMAssembly::emitSymbol(const MachineInstr *MI, MCSymbol *Symbol) {
  unsigned Opc = MI->getOpcode();
  assert(Opc == EVM::DATASIZE ||
         Opc == EVM::DATAOFFSET && "Unexpected symbol instruction");
  StackHeight += 1;
  // This is codegen-only instruction, that will be converted into PUSH4.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)))
                   .addSym(Symbol);
  verify(NewMI);
}

void EVMAssembly::emitConstant(uint64_t Val) { emitConstant(APInt(256, Val)); }

void EVMAssembly::emitLabelReference(const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  StackHeight += 1;
  auto [It, Inserted] = CallReturnSyms.try_emplace(Call);
  if (Inserted)
    It->second = MF.getContext().createTempSymbol("FUNC_RET", true);
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::PUSH_LABEL))
          .addSym(It->second);
  verify(NewMI);
}

void EVMAssembly::emitFuncCall(const MachineInstr *MI, const GlobalValue *Func,
                               int StackAdj, bool WillReturn) {
  assert(MI->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  assert(CurMBB == MI->getParent());

  // PUSH_LABEL technically increases the stack height on 1, but we don't
  // increase it explicitly here, as the label will be consumed by the following
  // JUMP.
  StackHeight += StackAdj;

  // Create pseudo jump to the callee, that will be expanded into PUSH and JUMP
  // instructions in the AsmPrinter.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoCALL))
                   .addGlobalAddress(Func);

  // If this function returns, we need to create a label after JUMP instruction
  // that is followed by JUMPDEST and this is taken care in the AsmPrinter.
  // In case we use setPostInstrSymbol here, the label will be created
  // after the JUMPDEST instruction, which is not what we want.
  if (WillReturn)
    NewMI.addSym(CallReturnSyms.at(MI));
  verify(NewMI);
}

void EVMAssembly::emitRet(const MachineInstr *MI) {
  assert(MI->getOpcode() == EVM::RET && "Unexpected ret instruction");
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoRET));
  verify(NewMI);
}

void EVMAssembly::emitUncondJump(const MachineInstr *MI,
                                 MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMP &&
         "Unexpected unconditional jump instruction");
  assert(StackHeight >= 0);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoJUMP))
                   .addMBB(Target);
  verify(NewMI);
}

void EVMAssembly::emitCondJump(const MachineInstr *MI,
                               MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMPI &&
         "Unexpected conditional jump instruction");
  StackHeight -= 1;
  assert(StackHeight >= 0);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoJUMPI))
                   .addMBB(Target);
  verify(NewMI);
}

// Verify that a stackified instruction doesn't have registers and dump it.
void EVMAssembly::verify(const MachineInstr *MI) const {
  assert(EVMInstrInfo::isStack(MI) &&
         "Only stackified instructions are allowed");
  assert(all_of(MI->operands(),
                [](const MachineOperand &MO) { return !MO.isReg(); }) &&
         "Registers are not allowed in stackified instructions");

  LLVM_DEBUG(dbgs() << "Adding: " << *MI << "stack height: " << StackHeight
                    << "\n");
}

void EVMAssembly::finalize() {
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : make_early_inc_range(MBB))
      // Remove all the instructions that are not stackified.
      // FIXME: Fix debug info for stackified instructions and don't
      // remove debug instructions.
      if (!EVMInstrInfo::isStack(&MI))
        MI.eraseFromParent();

  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  MFI->setIsStackified();

  // In a stackified code register liveness has no meaning.
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MRI.invalidateLiveness();
}
