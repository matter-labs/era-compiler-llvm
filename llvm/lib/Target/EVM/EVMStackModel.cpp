//===----- EVMEVMStackModel.cpp - EVM Stack Model ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "EVMStackModel.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;

bool llvm::isLinkerPseudoMI(const MachineInstr &MI) {
  return MI.getOpcode() == EVM::DATASIZE || MI.getOpcode() == EVM::DATAOFFSET ||
         MI.getOpcode() == EVM::LOADIMMUTABLE ||
         MI.getOpcode() == EVM::LINKERSYMBOL;
}
bool llvm::isPushOrDupLikeMI(const MachineInstr &MI) {
  return isLinkerPseudoMI(MI) || MI.getOpcode() == EVM::CONST_I256 ||
         MI.getOpcode() == EVM::COPY_I256;
}
bool llvm::isNoReturnCallMI(const MachineInstr &MI) {
  assert(MI.getOpcode() == EVM::FCALL && "Unexpected call instruction");
  const MachineOperand *FuncOp = MI.explicit_uses().begin();
  const auto *F = cast<Function>(FuncOp->getGlobal());
  return F->hasFnAttribute(Attribute::NoReturn);
}

static std::string getInstName(const MachineInstr *MI) {
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  return TII->getName(MI->getOpcode()).str();
}

std::string SymbolSlot::toString() const {
  return getInstName(MI) + ":" + std::string(Symbol->getName());
}
std::string CallerReturnSlot::toString() const {
  const MachineOperand *FuncOp = Call->explicit_uses().begin();
  const auto *F = cast<Function>(FuncOp->getGlobal());
  return "RET[" + std::string(F->getName()) + "]";
}

EVMStackModel::EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS,
                             unsigned StackDepthLimit)
    : MF(MF), LIS(LIS), StackDepthLimit(StackDepthLimit) {
  for (MachineBasicBlock &MBB : MF)
    for (const MachineInstr &MI : instructionsToProcess(&MBB))
      processMI(MI);
}

Stack EVMStackModel::getFunctionParameters() const {
  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  Stack Parameters(MFI->getNumParams(), EVMStackModel::getUnusedSlot());
  for (const MachineInstr &MI : MF.front()) {
    if (MI.getOpcode() == EVM::ARGUMENT) {
      int64_t ArgIdx = MI.getOperand(1).getImm();
      Parameters[ArgIdx] = getRegisterSlot(MI.getOperand(0).getReg());
    }
  }
  return Parameters;
}

StackSlot *EVMStackModel::getStackSlot(const MachineOperand &MO) const {
  // If the virtual register defines a constant and this is the only
  // definition, emit the literal slot as MI's input.
  const LiveInterval *LI = &LIS.getInterval(MO.getReg());
  if (LI->containsOneValue()) {
    SlotIndex Idx = LIS.getInstructionIndex(*MO.getParent());
    const VNInfo *VNI = LI->Query(Idx).valueIn();
    assert(VNI && "Use of non-existing value");
    assert(!VNI->isPHIDef());
    const MachineInstr *DefMI = LIS.getInstructionFromIndex(VNI->def);
    assert(DefMI && "Dead valno in interval");
    if (DefMI->getOpcode() == EVM::CONST_I256)
      return getLiteralSlot(DefMI->getOperand(1).getCImm()->getValue());
  }
  return getRegisterSlot(MO.getReg());
}

Stack EVMStackModel::getSlotsForInstructionUses(const MachineInstr &MI) const {
  Stack In;
  for (const auto &MO : reverse(MI.explicit_uses())) {
    // All the non-register operands are handled in instruction specific
    // handlers.
    // SP is not used anyhow.
    if (MO.isReg() && MO.getReg() != EVM::SP)
      In.push_back(getStackSlot(MO));
    else if (MO.isCImm())
      In.push_back(getLiteralSlot(MO.getCImm()->getValue()));
  }
  return In;
}

void EVMStackModel::processMI(const MachineInstr &MI) {
  unsigned Opc = MI.getOpcode();
  assert(Opc != EVM::STACK_LOAD && Opc != EVM::STACK_STORE &&
         "Unexpected stack memory instruction");
  assert(all_of(MI.implicit_operands(),
                [](const MachineOperand &MO) {
                  return MO.getReg() == EVM::VALUE_STACK ||
                         MO.getReg() == EVM::ARGUMENTS ||
                         MO.getReg() == EVM::SP;
                }) &&
         "Unexpected implicit def or use");

  assert(all_of(MI.explicit_operands(),
                [](const MachineOperand &MO) {
                  return !MO.isReg() ||
                         Register::isVirtualRegister(MO.getReg());
                }) &&
         "Unexpected explicit def or use");

  if (Opc == EVM::FCALL) {
    Stack Input;
    if (!isNoReturnCallMI(MI))
      Input.push_back(getCallerReturnSlot(&MI));

    append_range(Input, getSlotsForInstructionUses(MI));
    MIInputMap[&MI] = Input;
    return;
  }
  if (Opc == EVM::CONST_I256) {
    MIInputMap[&MI] =
        Stack(1, getLiteralSlot(MI.getOperand(1).getCImm()->getValue()));
    return;
  }
  if (isLinkerPseudoMI(MI)) {
    MCSymbol *Sym = MI.getOperand(1).getMCSymbol();
    MIInputMap[&MI] = Stack(1, getSymbolSlot(Sym, &MI));
    return;
  }

  MIInputMap[&MI] = getSlotsForInstructionUses(MI);
}

Stack EVMStackModel::getReturnArguments(const MachineInstr &MI) const {
  assert(MI.getOpcode() == EVM::RET);
  Stack Input = getSlotsForInstructionUses(MI);
  // We need to reverse input operands to restore original ordering,
  // in the instruction.
  // Calling convention: return values are passed in stack such that the
  // last one specified in the RET instruction is passed on the stack TOP.
  std::reverse(Input.begin(), Input.end());
  Input.push_back(getCalleeReturnSlot(&MF));
  return Input;
}
