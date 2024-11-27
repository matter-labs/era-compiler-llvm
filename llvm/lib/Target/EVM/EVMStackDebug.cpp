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
void ControlFlowGraphPrinter::operator()(const CFG &Cfg) {
  (*this)();
  for (const auto &Block : Cfg.Blocks)
    printBlock(Block);
}

void ControlFlowGraphPrinter::operator()() {
  OS << "Function: " << MF.getName() << '\n';
  OS << "Entry block: " << getBlockId(MF.front()) << ";\n";
}

std::string ControlFlowGraphPrinter::getBlockId(CFG::BasicBlock const &Block) {
  return getBlockId(*Block.MBB);
}

std::string ControlFlowGraphPrinter::getBlockId(const MachineBasicBlock &MBB) {
  return std::to_string(MBB.getNumber()) + "." + std::string(MBB.getName());
}

void ControlFlowGraphPrinter::printBlock(CFG::BasicBlock const &Block) {
  OS << "Block" << getBlockId(Block) << " [\n";

  // Verify that the entries of this block exit into this block.
  for (auto const &Entry : Block.Entries) {
    std::visit(
        Overload{
            [&](CFG::BasicBlock::Jump &Jump) {
              assert((Jump.Target == &Block) && "Invalid control flow graph");
            },
            [&](CFG::BasicBlock::ConditionalJump &CondJump) {
              assert((CondJump.Zero == &Block || CondJump.NonZero == &Block) &&
                     "Invalid control flow graph");
            },
            [&](const auto &) { assert(0 && "Invalid control flow graph."); }},
        Entry->Exit);
  }

  for (auto const &Op : Block.Operations) {
    std::visit(Overload{[&](const CFG::FunctionCall &FuncCall) {
                          const MachineOperand *Callee =
                              FuncCall.Call->explicit_uses().begin();
                          OS << Callee->getGlobal()->getName() << ": ";
                        },
                        [&](const CFG::BuiltinCall &BuiltinCall) {
                          OS << getInstName(BuiltinCall.Builtin) << ": ";
                        },
                        [&](const CFG::Assignment &Assignment) {
                          OS << "Assignment(";
                          for (const auto &Var : Assignment.Variables)
                            OS << printReg(Var.VirtualReg, nullptr, 0, nullptr)
                               << ", ";
                          OS << "): ";
                        }},
               Op.Operation);
    OS << stackToString(Op.Input) << " => " << stackToString(Op.Output) << '\n';
  }
  OS << "];\n";

  std::visit(
      Overload{[&](const CFG::BasicBlock::Jump &Jump) {
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"";
                 OS << "Jump\" FallThrough:" << Jump.FallThrough << "];\n";
                 if (Jump.Backwards)
                   OS << "Backwards";
                 OS << "Block" << getBlockId(Block) << "Exit -> Block"
                    << getBlockId(*Jump.Target) << ";\n";
               },
               [&](const CFG::BasicBlock::ConditionalJump &CondJump) {
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
                 OS << stackSlotToString(CondJump.Condition);
                 OS << "| { <0> Zero | <1> NonZero }}\" FallThrough:";
                 OS << CondJump.FallThrough << "];\n";
                 OS << "Block" << getBlockId(Block);
                 OS << "Exit:0 -> Block" << getBlockId(*CondJump.Zero) << ";\n";
                 OS << "Block" << getBlockId(Block);
                 OS << "Exit:1 -> Block" << getBlockId(*CondJump.NonZero)
                    << ";\n";
               },
               [&](const CFG::BasicBlock::FunctionReturn &Return) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"FunctionReturn[" << MF.getName()
                    << "]\"];\n";
                 OS << "Return values: " << stackToString(Return.RetValues);
               },
               [&](const CFG::BasicBlock::Terminated &) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"Terminated\"];\n";
               },
               [&](const CFG::BasicBlock::Unreachable &) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"Unreachable\"];\n";
               },
               [&](const CFG::BasicBlock::InvalidExit &) {
                 assert(0 && "Invalid basic block exit");
               }},
      Block.Exit);
  OS << "\n";
}

void StackLayoutPrinter::operator()(CFG::BasicBlock const &Block,
                                    bool IsMainEntry) {
  if (IsMainEntry) {
    OS << "Entry [label=\"Entry\"];\n";
    OS << "Entry -> Block" << getBlockId(Block) << ";\n";
  }
  while (!BlocksToPrint.empty()) {
    CFG::BasicBlock const *block = *BlocksToPrint.begin();
    BlocksToPrint.erase(BlocksToPrint.begin());
    printBlock(*block);
  }
}

void StackLayoutPrinter::operator()(CFG::BasicBlock const &EntryBB,
                                    const std::vector<StackSlot> &Parameters) {
  OS << "Function: " << MF.getName() << "(";
  for (const StackSlot &ParamSlot : Parameters) {
    if (const auto *Slot = std::get_if<VariableSlot>(&ParamSlot))
      OS << printReg(Slot->VirtualReg, nullptr, 0, nullptr) << ' ';
    else if (std::holds_alternative<JunkSlot>(ParamSlot))
      OS << "[unused param] ";
    else
      llvm_unreachable("Unexpected stack slot");
  }
  OS << ");\n";
  OS << "FunctionEntry "
     << " -> Block" << getBlockId(EntryBB) << ";\n";
  (*this)(EntryBB, false);
}

void StackLayoutPrinter::printBlock(CFG::BasicBlock const &Block) {
  OS << "Block" << getBlockId(Block) << " [\n";
  // Verify that the entries of this block exit into this block.
  for (auto const &entry : Block.Entries) {
    std::visit(
        Overload{[&](CFG::BasicBlock::Jump const &Jump) {
                   assert(Jump.Target == &Block);
                 },
                 [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump) {
                   assert(ConditionalJump.Zero == &Block ||
                          ConditionalJump.NonZero == &Block);
                 },
                 [&](auto const &) {
                   llvm_unreachable("Invalid control flow graph");
                 }},
        entry->Exit);
  }

  auto const &BlockInfo = Layout.blockInfos.at(&Block);
  OS << stackToString(BlockInfo.entryLayout) << "\n";
  for (auto const &Operation : Block.Operations) {
    OS << "\n";
    auto EntryLayout = Layout.operationEntryLayout.at(&Operation);
    OS << stackToString(EntryLayout) << "\n";
    std::visit(Overload{[&](CFG::FunctionCall const &Call) {
                          const MachineOperand *Callee =
                              Call.Call->explicit_uses().begin();
                          OS << Callee->getGlobal()->getName();
                        },
                        [&](CFG::BuiltinCall const &Call) {
                          OS << getInstName(Call.Builtin);
                        },
                        [&](CFG::Assignment const &Assignment) {
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
  OS << stackToString(BlockInfo.exitLayout) << "\n";
  OS << "];\n";

  std::visit(
      Overload{[&](CFG::BasicBlock::InvalidExit const &) {
                 assert(0 && "Invalid basic block exit");
               },
               [&](CFG::BasicBlock::Jump const &Jump) {
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"";
                 if (Jump.Backwards)
                   OS << "Backwards";
                 OS << "Jump\"];\n";
                 OS << "Block" << getBlockId(Block) << "Exit -> Block"
                    << getBlockId(*Jump.Target) << ";\n";
               },
               [&](CFG::BasicBlock::ConditionalJump const &ConditionalJump) {
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
                 OS << stackSlotToString(ConditionalJump.Condition);
                 OS << "| { <0> Zero | <1> NonZero }}\"];\n";
                 OS << "Block" << getBlockId(Block);
                 OS << "Exit:0 -> Block" << getBlockId(*ConditionalJump.Zero)
                    << ";\n";
                 OS << "Block" << getBlockId(Block);
                 OS << "Exit:1 -> Block" << getBlockId(*ConditionalJump.NonZero)
                    << ";\n";
               },
               [&](CFG::BasicBlock::FunctionReturn const &Return) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"FunctionReturn[" << MF.getName()
                    << "]\"];\n";
               },
               [&](CFG::BasicBlock::Terminated const &) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"Terminated\"];\n";
               },
               [&](CFG::BasicBlock::Unreachable const &) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"Unreachable\"];\n";
               }},
      Block.Exit);
  OS << "\n";
}

std::string StackLayoutPrinter::getBlockId(CFG::BasicBlock const &Block) {
  std::string Name = std::to_string(Block.MBB->getNumber()) + "." +
                     std::string(Block.MBB->getName());
  if (auto It = BlockIds.find(&Block); It != BlockIds.end())
    return std::to_string(It->second) + "(" + Name + ")";

  size_t Id = BlockIds[&Block] = BlockCount++;
  BlocksToPrint.emplace_back(&Block);
  return std::to_string(Id) + "(" + Name + ")";
}
#endif // NDEBUG
