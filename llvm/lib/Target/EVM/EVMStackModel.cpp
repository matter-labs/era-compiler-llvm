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
#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

#include <ostream>
#include <variant>

using namespace llvm;

EVMStackModel::EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS)
    : MF(MF), LIS(LIS) {
  for (MachineBasicBlock &MBB : MF) {
    std::vector<Operation> Ops;
    for (MachineInstr &MI : MBB)
      createOperation(MI, Ops);
    OperationsMap[&MBB] = std::move(Ops);
  }
}

Stack EVMStackModel::getFunctionParameters() const {
  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  std::vector<StackSlot> Parameters(MFI->getNumParams(), JunkSlot{});
  for (const MachineInstr &MI : MF.front()) {
    if (MI.getOpcode() == EVM::ARGUMENT) {
      int64_t ArgIdx = MI.getOperand(1).getImm();
      Parameters[ArgIdx] = VariableSlot{MI.getOperand(0).getReg()};
    }
  }
  return Parameters;
}

StackSlot EVMStackModel::getStackSlot(const MachineOperand &MO) const {
  const MachineInstr *MI = MO.getParent();
  StackSlot Slot = VariableSlot{MO.getReg()};
  SlotIndex Idx = LIS.getInstructionIndex(*MI);
  const LiveInterval *LI = &LIS.getInterval(MO.getReg());
  LiveQueryResult LRQ = LI->Query(Idx);
  const VNInfo *VNI = LRQ.valueIn();
  assert(VNI && "Use of non-existing value");
  // If the virtual register defines a constant and this is the only
  // definition, emit the literal slot as MI's input.
  if (LI->containsOneValue()) {
    assert(!VNI->isPHIDef());
    const MachineInstr *DefMI = LIS.getInstructionFromIndex(VNI->def);
    assert(DefMI && "Dead valno in interval");
    if (DefMI->getOpcode() == EVM::CONST_I256) {
      const APInt Imm = DefMI->getOperand(1).getCImm()->getValue();
      Slot = LiteralSlot{std::move(Imm)};
    }
  }

  return Slot;
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

    In.emplace_back(getStackSlot(MO));
  }
  return In;
}

Stack EVMStackModel::getInstrOutput(const MachineInstr &MI) const {
  Stack Out;
  unsigned ArgNumber = 0;
  for (const auto &MO : MI.defs())
    Out.push_back(TemporarySlot{&MI, MO.getReg(), ArgNumber++});
  return Out;
}

void EVMStackModel::createOperation(MachineInstr &MI,
                                    std::vector<Operation> &Ops) const {
  unsigned Opc = MI.getOpcode();
  switch (Opc) {
  case EVM::STACK_LOAD:
  case EVM::STACK_STORE:
    llvm_unreachable("Unexpected stack memory instruction");
    return;
  case EVM::ARGUMENT:
    // Is handled above.
    return;
  case EVM::FCALL: {
    Stack Input;
    bool IsNoReturn = false;
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isGlobal()) {
        const auto *Func = dyn_cast<Function>(MO.getGlobal());
        assert(Func);
        IsNoReturn = Func->hasFnAttribute(Attribute::NoReturn);
        if (!IsNoReturn)
          Input.push_back(FunctionCallReturnLabelSlot{&MI});
        break;
      }
    }
    const Stack &Tmp = getInstrInput(MI);
    Input.insert(Input.end(), Tmp.begin(), Tmp.end());
    size_t NumArgs = Input.size() - (IsNoReturn ? 0 : 1);
    Ops.emplace_back(Operation{std::move(Input), getInstrOutput(MI),
                               FunctionCall{&MI, NumArgs}});
  } break;
  case EVM::RET:
  case EVM::JUMP:
  case EVM::JUMPI:
    // These instructions are handled separately.
    return;
  case EVM::COPY_I256:
  case EVM::DATASIZE:
  case EVM::DATAOFFSET:
    // The copy/data instructions just represent an assignment. This case is
    // handled below.
    break;
  case EVM::CONST_I256: {
    const LiveInterval *LI = &LIS.getInterval(MI.getOperand(0).getReg());
    // If the virtual register has the only definition, ignore this instruction,
    // as we create literal slots from the immediate value at the register uses.
    if (LI->containsOneValue())
      return;
  } break;
  default: {
    Ops.emplace_back(
        Operation{getInstrInput(MI), getInstrOutput(MI), BuiltinCall{&MI}});
  } break;
  }

  // Cretae CFG::Assignment object for the MI.
  Stack Input, Output;
  std::vector<VariableSlot> Variables;
  switch (MI.getOpcode()) {
  case EVM::CONST_I256: {
    const Register DefReg = MI.getOperand(0).getReg();
    const APInt Imm = MI.getOperand(1).getCImm()->getValue();
    Input.push_back(LiteralSlot{std::move(Imm)});
    Output.push_back(VariableSlot{DefReg});
    Variables.push_back(VariableSlot{DefReg});
  } break;
  case EVM::DATASIZE:
  case EVM::DATAOFFSET: {
    const Register DefReg = MI.getOperand(0).getReg();
    MCSymbol *Sym = MI.getOperand(1).getMCSymbol();
    Input.push_back(SymbolSlot{Sym, &MI});
    Output.push_back(VariableSlot{DefReg});
    Variables.push_back(VariableSlot{DefReg});
  } break;
  case EVM::COPY_I256: {
    // Copy instruction corresponds to the assignment operator, so
    // we do not need to create intermediate TmpSlots.
    Input = getInstrInput(MI);
    const Register DefReg = MI.getOperand(0).getReg();
    Output.push_back(VariableSlot{DefReg});
    Variables.push_back(VariableSlot{DefReg});
  } break;
  default: {
    unsigned ArgsNumber = 0;
    for (const auto &MO : MI.defs()) {
      assert(MO.isReg());
      const Register Reg = MO.getReg();
      Input.push_back(TemporarySlot{&MI, Reg, ArgsNumber++});
      Output.push_back(VariableSlot{Reg});
      Variables.push_back(VariableSlot{Reg});
    }
  } break;
  }
  // We don't need an assignment part of the instructions that do not write
  // results.
  if (!Input.empty() || !Output.empty())
    Ops.emplace_back(Operation{std::move(Input), std::move(Output),
                               Assignment{std::move(Variables)}});
}

Stack EVMStackModel::getReturnArguments(const MachineInstr &MI) const {
  assert(MI.getOpcode() == EVM::RET);
  Stack Input = getInstrInput(MI);
  // We need to reverse input operands to restore original ordering,
  // in the instruction.
  // Calling convention: return values are passed in stack such that the
  // last one specified in the RET instruction is passed on the stack TOP.
  std::reverse(Input.begin(), Input.end());
  Input.emplace_back(FunctionReturnLabelSlot{&MF});
  return Input;
}
