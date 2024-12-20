//===-- EVMStackDebug.cpp - Debugging of the stackification -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares classes for dumping a state of stackifcation related data
// structures and algorithms.
//
//===----------------------------------------------------------------------===//

#include "EVMStackDebug.h"
#include "EVMHelperUtilities.h"
#include "EVMStackLayoutGenerator.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include <variant>

using namespace llvm;

static std::string getInstName(const MachineInstr *MI) {
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  return TII->getName(MI->getOpcode()).str();
}

const Function *llvm::getCalledFunction(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isGlobal())
      continue;
    const Function *Func = dyn_cast<Function>(MO.getGlobal());
    if (Func != nullptr)
      return Func;
  }
  return nullptr;
}

std::string llvm::stackToString(const Stack &S) {
  std::string Result("[ ");
  for (auto const &Slot : S)
    Result += stackSlotToString(Slot) + ' ';
  Result += ']';
  return Result;
}

std::string llvm::stackSlotToString(const StackSlot &Slot) {
  return std::visit(
      Overload{
          [](const FunctionCallReturnLabelSlot &Ret) -> std::string {
            return "RET[" +
                   std::string(getCalledFunction(*Ret.Call)->getName()) + "]";
          },
          [](const FunctionReturnLabelSlot &) -> std::string { return "RET"; },
          [](const VariableSlot &Var) -> std::string {
            SmallString<64> S;
            raw_svector_ostream OS(S);
            OS << printReg(Var.VirtualReg, nullptr, 0, nullptr);
            return std::string(S);
            ;
          },
          [](const LiteralSlot &Literal) -> std::string {
            SmallString<64> S;
            Literal.Value.toStringSigned(S);
            return std::string(S);
          },
          [](const SymbolSlot &Symbol) -> std::string {
            return getInstName(Symbol.MI) + ":" +
                   std::string(Symbol.Symbol->getName());
          },
          [](const TemporarySlot &Tmp) -> std::string {
            SmallString<128> S;
            raw_svector_ostream OS(S);
            OS << "TMP[" << getInstName(Tmp.MI) << ", ";
            OS << std::to_string(Tmp.Index) + "]";
            return std::string(S);
          },
          [](const JunkSlot &Junk) -> std::string { return "JUNK"; }},
      Slot);
  ;
}

#ifndef NDEBUG

void StackLayoutPrinter::operator()() {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot &ParamSlot : StackModel.getFunctionParameters()) {
    if (const auto *Slot = std::get_if<VariableSlot>(&ParamSlot))
      OS << printReg(Slot->VirtualReg, nullptr, 0, nullptr) << ' ';
    else if (std::holds_alternative<JunkSlot>(ParamSlot))
      OS << "[unused param] ";
    else
      llvm_unreachable("Unexpected stack slot");
  }
  OS << ");\n";
  OS << "FunctionEntry "
     << " -> Block" << getBlockId(MF.front()) << ";\n";

  for (const auto &MBB : MF) {
    printBlock(MBB);
  }
}

void StackLayoutPrinter::printBlock(MachineBasicBlock const &Block) {
  OS << "Block" << getBlockId(Block) << " [\n";
  OS << stackToString(Layout.getMBBEntryLayout(&Block)) << "\n";
  for (auto const &Operation : StackModel.getOperations(&Block)) {
    OS << "\n";
    Stack EntryLayout = Layout.getOperationEntryLayout(&Operation);
    OS << stackToString(EntryLayout) << "\n";
    std::visit(Overload{[&](FunctionCall const &Call) {
                          const MachineOperand *Callee =
                              Call.MI->explicit_uses().begin();
                          OS << Callee->getGlobal()->getName();
                        },
                        [&](BuiltinCall const &Call) {
                          OS << getInstName(Call.MI);
                        },
                        [&](Assignment const &Assignment) {
                          OS << "Assignment(";
                          for (const auto &Var : Assignment.Variables)
                            OS << printReg(Var.VirtualReg, nullptr, 0, nullptr)
                               << ", ";
                          OS << ")";
                        }},
               Operation.Operation);
    OS << "\n";

    assert(Operation.Input.size() <= EntryLayout.size());
    for (size_t i = 0; i < Operation.Input.size(); ++i)
      EntryLayout.pop_back();
    EntryLayout += Operation.Output;
    OS << stackToString(EntryLayout) << "\n";
  }
  OS << "\n";
  OS << stackToString(Layout.getMBBExitLayout(&Block)) << "\n";
  OS << "];\n";

  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(&Block);
  MBBExitType ExitType = TermInfo->getExitType();
  if (ExitType == MBBExitType::UnconditionalBranch) {
    auto [BranchInstr, Target, IsLatch] = TermInfo->getUnconditionalBranch();
    OS << "Block" << getBlockId(Block) << "Exit [label=\"";
    OS << "Jump\"];\n";
    if (IsLatch)
      OS << "Backwards";
    OS << "Block" << getBlockId(Block) << "Exit -> Block" << getBlockId(*Target)
       << ";\n";
  } else if (ExitType == MBBExitType::ConditionalBranch) {
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
    OS << stackSlotToString(StackModel.getStackSlot(*Condition));
    OS << "| { <0> Zero | <1> NonZero }}\"];\n";
    OS << "Block" << getBlockId(Block);
    OS << "Exit:0 -> Block" << getBlockId(*FalseBB) << ";\n";
    OS << "Block" << getBlockId(Block);
    OS << "Exit:1 -> Block" << getBlockId(*TrueBB) << ";\n";
  } else if (ExitType == MBBExitType::FunctionReturn) {
    OS << "Block" << getBlockId(Block) << "Exit [label=\"FunctionReturn["
       << MF.getName() << "]\"];\n";
    const MachineInstr &MI = Block.back();
    OS << "Return values: " << stackToString(StackModel.getReturnArguments(MI))
       << ";\n";
  } else if (ExitType == MBBExitType::Terminate) {
    OS << "Block" << getBlockId(Block) << "Exit [label=\"Terminated\"];\n";
  }
  OS << "\n";
}

std::string StackLayoutPrinter::getBlockId(MachineBasicBlock const &Block) {
  std::string Name =
      std::to_string(Block.getNumber()) + "." + std::string(Block.getName());
  if (auto It = BlockIds.find(&Block); It != BlockIds.end())
    return std::to_string(It->second) + "(" + Name + ")";

  size_t Id = BlockIds[&Block] = BlockCount++;
  return std::to_string(Id) + "(" + Name + ")";
}
#endif // NDEBUG
