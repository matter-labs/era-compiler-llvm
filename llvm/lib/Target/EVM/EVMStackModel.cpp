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
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;

std::string llvm::stackToString(const Stack &S) {
  std::string Result("[ ");
  for (const auto *Slot : S)
    Result += Slot->toString() + ' ';
  Result += ']';
  return Result;
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
std::string FunctionCallReturnLabelSlot::toString() const {
  return "RET[" + std::string(getCalledFunction(*Call)->getName()) + "]";
}
std::string Operation::toString() const {
  if (isFunctionCall()) {
    const MachineOperand *Callee = MI->explicit_uses().begin();
    return Callee->getGlobal()->getName().str();
  }
  if (isBuiltinCall())
    return getInstName(MI);

  assert(isAssignment());
  SmallString<128> S;
  raw_svector_ostream OS(S);
  OS << "Assignment(";
  for (const auto &MO : MI->defs())
    OS << printReg(MO.getReg(), nullptr, 0, nullptr) << ", ";
  OS << ")";
  return std::string(S);
}

EVMStackModel::EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS,
                             unsigned StackDepthLimit)
    : MF(MF), LIS(LIS), StackDepthLimit(StackDepthLimit) {
  for (MachineBasicBlock &MBB : MF) {
    SmallVector<Operation> Ops;
    for (MachineInstr &MI : MBB)
      createOperation(MI, Ops);
    OperationsMap[&MBB] = std::move(Ops);
  }
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
    if (DefMI->getOpcode() == EVM::CONST_I256) {
      const APInt Imm = DefMI->getOperand(1).getCImm()->getValue();
      return getLiteralSlot(std::move(Imm));
    }
  }
  return getRegisterSlot(MO.getReg());
}

Stack EVMStackModel::getInstrInput(const MachineInstr &MI) const {
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

void EVMStackModel::createOperation(MachineInstr &MI,
                                    SmallVector<Operation> &Ops) const {
  unsigned Opc = MI.getOpcode();
  assert(Opc != EVM::STACK_LOAD && Opc != EVM::STACK_STORE &&
         "Unexpected stack memory instruction");
  // These instructions are handled separately.
  if (Opc == EVM::ARGUMENT || Opc == EVM::RET || Opc == EVM::JUMP ||
      Opc == EVM::JUMPI)
    return;
  // If the virtual register has the only definition, ignore this instruction,
  // as we create literal slots from the immediate value at the register uses.
  if (Opc == EVM::CONST_I256 &&
      LIS.getInterval(MI.getOperand(0).getReg()).containsOneValue())
    return;

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

  // Create FunctionCall or BuiltinCall operations.
  switch (Opc) {
  case EVM::FCALL: {
    Stack Input;
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isGlobal()) {
        const auto *Func = cast<Function>(MO.getGlobal());
        if (!Func->hasFnAttribute(Attribute::NoReturn))
          Input.push_back(getFunctionCallReturnLabelSlot(&MI));
        break;
      }
    }
    const Stack &Tmp = getInstrInput(MI);
    Input.insert(Input.end(), Tmp.begin(), Tmp.end());
    Ops.emplace_back(Operation::FunctionCall, std::move(Input), &MI);
  } break;
  case EVM::CONST_I256:
  case EVM::COPY_I256:
  case EVM::DATASIZE:
  case EVM::DATAOFFSET:
  case EVM::LOADIMMUTABLE:
  case EVM::LINKERSYMBOL: {
    // The copy/data instructions just represent an assignment.
  } break;
  default: {
    Ops.emplace_back(Operation::BuiltinCall, getInstrInput(MI), &MI);
  } break;
  }

  // Create Assignment operation for the MI.
  Stack Input;
  switch (MI.getOpcode()) {
  case EVM::CONST_I256: {
    const APInt Imm = MI.getOperand(1).getCImm()->getValue();
    Input.push_back(getLiteralSlot(std::move(Imm)));
  } break;
  case EVM::DATASIZE:
  case EVM::DATAOFFSET:
  case EVM::LINKERSYMBOL:
  case EVM::LOADIMMUTABLE: {
    MCSymbol *Sym = MI.getOperand(1).getMCSymbol();
    Input.push_back(getSymbolSlot(Sym, &MI));
  } break;
  case EVM::COPY_I256: {
    // Copy instruction corresponds to the assignment operator, so
    // we do not need to create intermediate TmpSlots.
    Input = getInstrInput(MI);
  } break;
  default: {
    Input = getSlotsForInstructionDefs(&MI);
  } break;
  }

  // Skip for the instructions that do not write results.
  if (!Input.empty() || MI.getNumExplicitDefs())
    Ops.emplace_back(Operation::Assignment, std::move(Input), &MI);
}

Stack EVMStackModel::getReturnArguments(const MachineInstr &MI) const {
  assert(MI.getOpcode() == EVM::RET);
  Stack Input = getInstrInput(MI);
  // We need to reverse input operands to restore original ordering,
  // in the instruction.
  // Calling convention: return values are passed in stack such that the
  // last one specified in the RET instruction is passed on the stack TOP.
  std::reverse(Input.begin(), Input.end());
  Input.push_back(getFunctionReturnLabelSlot(&MF));
  return Input;
}
