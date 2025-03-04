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

bool llvm::isConstCopyOrLinkerMI(const MachineInstr &MI) {
  return MI.getOpcode() == EVM::CONST_I256 ||
         MI.getOpcode() == EVM::COPY_I256 || MI.getOpcode() == EVM::DATASIZE ||
         MI.getOpcode() == EVM::DATAOFFSET ||
         MI.getOpcode() == EVM::LOADIMMUTABLE ||
         MI.getOpcode() == EVM::LINKERSYMBOL;
}

static const Function *getCalledFunction(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isGlobal())
      continue;
    if (const auto *Func = dyn_cast<Function>(MO.getGlobal()))
      return Func;
  }
  return nullptr;
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
  return "RET[" + std::string(getCalledFunction(*Call)->getName()) + "]";
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
  SmallVector<StackSlot *> Parameters(MFI->getNumParams(),
                                      EVMStackModel::getJunkSlot());
  for (const MachineInstr &MI : MF.front()) {
    if (MI.getOpcode() == EVM::ARGUMENT) {
      int64_t ArgIdx = MI.getOperand(1).getImm();
      Parameters[ArgIdx] = getRegisterSlot(MI.getOperand(0).getReg());
    }
  }
  return Stack(Parameters);
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
    if (DefMI->getOpcode() == EVM::CONST_I256) {
      const APInt Imm = DefMI->getOperand(1).getCImm()->getValue();
      return getLiteralSlot(std::move(Imm));
    }
  }
  return getRegisterSlot(MO.getReg());
}

Stack EVMStackModel::getSlotsForInstructionUses(const MachineInstr &MI) const {
  Stack In;
  for (const auto &MO : reverse(MI.explicit_uses())) {
    // All the non-register operands are handled in instruction specific
    // handlers.
    if (!MO.isReg())
      continue;

    // SP is not used anyhow.
    if (MO.getReg() == EVM::SP)
      continue;

    In.push_back(getStackSlot(MO));
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

  switch (Opc) {
  case EVM::FCALL: {
    Stack Input;
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isGlobal()) {
        const auto *Func = cast<Function>(MO.getGlobal());
        if (!Func->hasFnAttribute(Attribute::NoReturn))
          Input.push_back(getCallerReturnSlot(&MI));
        break;
      }
    }
    append_range(Input, getSlotsForInstructionUses(MI));
    MIInputMap[&MI] = Input;
    return;
  }
  case EVM::CONST_I256: {
    const APInt Imm = MI.getOperand(1).getCImm()->getValue();
    MIInputMap[&MI] = Stack(1, getLiteralSlot(std::move(Imm)));
    return;
  }
  case EVM::DATASIZE:
  case EVM::DATAOFFSET:
  case EVM::LOADIMMUTABLE:
  case EVM::LINKERSYMBOL: {
    MCSymbol *Sym = MI.getOperand(1).getMCSymbol();
    MIInputMap[&MI] = Stack(1, getSymbolSlot(Sym, &MI));
    return;
  }
  default: {
    MIInputMap[&MI] = getSlotsForInstructionUses(MI);
    return;
  }
  }
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
