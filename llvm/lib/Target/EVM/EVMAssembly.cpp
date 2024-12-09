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
#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "evm-assembly"

#ifndef NDEBUG
void EVMAssembly::dumpInst(const MachineInstr *MI) const {
  LLVM_DEBUG(dbgs() << "Adding: " << *MI << "stack height: " << StackHeight
                    << "\n");
}
#endif // NDEBUG

int EVMAssembly::getStackHeight() const { return StackHeight; }

void EVMAssembly::setStackHeight(int Height) {
  StackHeight = Height;
  LLVM_DEBUG(dbgs() << "Set stack height: " << StackHeight << "\n");
}

void EVMAssembly::setCurrentLocation(MachineBasicBlock *MBB) {
  CurMBB = MBB;
  CurMIIt = MBB->begin();
  LLVM_DEBUG(dbgs() << "Setting current location to: " << MBB->getNumber()
                    << "." << MBB->getName() << "\n");
}

void EVMAssembly::appendInstruction(MachineInstr *MI) {
#ifndef NDEBUG
  unsigned Opc = MI->getOpcode();
  assert(Opc != EVM::JUMP && Opc != EVM::JUMPI && Opc != EVM::ARGUMENT &&
         Opc != EVM::RET && Opc != EVM::CONST_I256 && Opc != EVM::COPY_I256 &&
         Opc != EVM::FCALL);
#endif // NDEBUG

  [[maybe_unused]] auto Ret = AssemblyInstrs.insert(MI);
  assert(Ret.second);
  int StackAdj = (2 * static_cast<int>(MI->getNumExplicitDefs())) -
                 static_cast<int>(MI->getNumExplicitOperands());
  StackHeight += StackAdj;
  LLVM_DEBUG(dumpInst(MI));
  CurMIIt = std::next(MIIter(MI));
}

void EVMAssembly::appendSWAPInstruction(unsigned Depth) {
  unsigned Opc = EVM::getSWAPOpcode(Depth);
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opc));
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendDUPInstruction(unsigned Depth) {
  unsigned Opc = EVM::getDUPOpcode(Depth);
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opc));
  StackHeight += 1;
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendPOPInstruction() {
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::POP));
  assert(StackHeight > 0);
  StackHeight -= 1;
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendConstant(const APInt &Val) {
  unsigned Opc = EVM::getPUSHOpcode(Val);
  MachineInstrBuilder Builder =
      BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opc));
  if (Opc != EVM::PUSH0) {
    LLVMContext &Ctx = MF->getFunction().getContext();
    Builder.addCImm(ConstantInt::get(Ctx, Val));
  }
  StackHeight += 1;
  CurMIIt = Builder;
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendSymbol(MCSymbol *Symbol, unsigned Opcode) {
  // This is codegen-only instruction, that will be converted into PUSH4.
  CurMIIt =
      BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opcode)).addSym(Symbol);
  StackHeight += 1;
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendConstant(uint64_t Val) {
  appendConstant(APInt(256, Val));
}

void EVMAssembly::appendLabelReference(MCSymbol *Label) {
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::PUSH_LABEL))
                .addSym(Label);
  StackHeight += 1;
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

MCSymbol *EVMAssembly::createFuncRetSymbol() {
  return MF->getContext().createTempSymbol("FUNC_RET", true);
}

void EVMAssembly::appendFuncCall(const MachineInstr *MI,
                                 const llvm::GlobalValue *Func, int StackAdj,
                                 MCSymbol *RetSym) {
  // Push the function label
  assert(CurMBB == MI->getParent());

  // Create pseudo jump to the callee, that will be expanded into PUSH and JUMP
  // instructions in the AsmPrinter.
  CurMIIt =
      BuildMI(*CurMBB, CurMIIt, MI->getDebugLoc(), TII->get(EVM::PseudoCALL))
          .addGlobalAddress(Func);

  // If this function returns, we need to create a label after JUMP instruction
  // that is followed by JUMPDEST and this is taken care in the AsmPrinter.
  // In case we use setPostInstrSymbol here, the label will be created
  // after the JUMPDEST instruction, which is not what we want.
  if (RetSym)
    MachineInstrBuilder(*CurMIIt->getParent()->getParent(), CurMIIt)
        .addSym(RetSym);

  // PUSH_LABEL technically increases the stack height on 1, but we don't
  // increase it explicitly here, as the label will be consumed by the following
  // JUMP.
  StackHeight += StackAdj;
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendRet() {
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::PseudoRET));
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendUncondJump(MachineInstr *MI,
                                   MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMP);
  CurMIIt =
      BuildMI(*CurMBB, CurMIIt, MI->getDebugLoc(), TII->get(EVM::PseudoJUMP))
          .addMBB(Target);
  assert(StackHeight >= 0);
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendCondJump(MachineInstr *MI, MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMPI);
  CurMIIt =
      BuildMI(*CurMBB, CurMIIt, MI->getDebugLoc(), TII->get(EVM::PseudoJUMPI))
          .addMBB(Target);
  StackHeight -= 1;
  assert(StackHeight >= 0);
  AssemblyInstrs.insert(&*CurMIIt);
  LLVM_DEBUG(dumpInst(&*CurMIIt));
  CurMIIt = std::next(CurMIIt);
}

// Remove all registers operands of the \p MI and repaces the opcode with
// the stack variant variant.
void EVMAssembly::stackifyInstruction(MachineInstr *MI) {
  if (MI->isDebugInstr() || MI->isLabel() || MI->isInlineAsm())
    return;

  unsigned RegOpcode = MI->getOpcode();
  if (RegOpcode == EVM::PUSH_LABEL || RegOpcode == EVM::PseudoJUMP ||
      RegOpcode == EVM::PseudoJUMPI || RegOpcode == EVM::PseudoCALL ||
      RegOpcode == EVM::PseudoRET)
    return;

  // Remove register operands.
  for (unsigned I = MI->getNumOperands(); I > 0; --I) {
    auto &MO = MI->getOperand(I - 1);
    if (MO.isReg()) {
      MI->removeOperand(I - 1);
    }
  }

  // Transform 'register' instruction to the 'stack' one.
  unsigned StackOpcode = EVM::getStackOpcode(RegOpcode);
  MI->setDesc(TII->get(StackOpcode));
}

void EVMAssembly::finalize() {
  // Collect and erase instructions that are not required in
  // a stackified code. These are auxiliary codegn-only instructions.
  SmallVector<MachineInstr *, 128> ToRemove;
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (!AssemblyInstrs.count(&MI))
        ToRemove.emplace_back(&MI);
    }
  }

  for (MachineInstr *MI : ToRemove)
    MI->eraseFromParent();

  // Remove register operands and replace instruction opcode with 'stack' one.
  for (MachineBasicBlock &MBB : *MF)
    for (MachineInstr &MI : MBB)
      stackifyInstruction(&MI);

  auto *MFI = MF->getInfo<EVMMachineFunctionInfo>();
  MFI->setIsStackified();

  // In a stackified code register liveness has no meaning.
  MachineRegisterInfo &MRI = MF->getRegInfo();
  MRI.invalidateLiveness();
}
