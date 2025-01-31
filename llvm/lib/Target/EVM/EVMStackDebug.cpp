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
#include "EVMStackLayoutGenerator.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include <variant>

using namespace llvm;

std::string llvm::stackToString(const Stack &S) {
  std::string Result("[ ");
  for (const auto *Slot : S)
    Result += Slot->toString() + ' ';
  Result += ']';
  return Result;
}

#ifndef NDEBUG

void StackLayoutPrinter::operator()() {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot *ParamSlot : StackModel.getFunctionParameters()) {
    if (const auto *Slot = dyn_cast<VariableSlot>(ParamSlot))
      OS << printReg(Slot->getReg(), nullptr, 0, nullptr) << ' ';
    else if (isa<JunkSlot>(ParamSlot))
      OS << "[unused param] ";
    else
      llvm_unreachable("Unexpected stack slot");
  }
  OS << ");\n";
  OS << "FunctionEntry " << " -> Block" << getBlockId(MF.front()) << ";\n";

  for (const auto &MBB : MF) {
    printBlock(MBB);
  }
}

void StackLayoutPrinter::printBlock(MachineBasicBlock const &Block) {
  OS << "Block" << getBlockId(Block) << " [\n";
  OS << stackToString(Layout.getMBBEntryLayout(&Block)) << "\n";
  for (auto const &Op : StackModel.getOperations(&Block)) {
    OS << "\n";
    Stack EntryLayout = Layout.getOperationEntryLayout(&Op);
    OS << stackToString(EntryLayout) << "\n";
    OS << Op.toString() << "\n";
    assert(Op.getInput().size() <= EntryLayout.size());
    EntryLayout.resize(EntryLayout.size() - Op.getInput().size());
    EntryLayout.append(Op.getOutput());
    OS << stackToString(EntryLayout) << "\n";
  }
  OS << "\n";
  OS << stackToString(Layout.getMBBExitLayout(&Block)) << "\n";
  OS << "];\n";

  const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(&Block);
  MBBExitType ExitType = TermInfo->getExitType();
  if (ExitType == MBBExitType::UnconditionalBranch) {
    auto [BranchInstr, Target] = TermInfo->getUnconditionalBranch();
    OS << "Block" << getBlockId(Block) << "Exit [label=\"";
    OS << "Jump\"];\n";
    OS << "Block" << getBlockId(Block) << "Exit -> Block" << getBlockId(*Target)
       << ";\n";
  } else if (ExitType == MBBExitType::ConditionalBranch) {
    auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
        TermInfo->getConditionalBranch();
    OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
    OS << StackModel.getStackSlot(*Condition)->toString();
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
