#include "EVMControlFlowGraphBuilder.h"
#include "EVM.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

#include <ostream>
#include <variant>

using namespace llvm;

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

static StringRef getInstName(const MachineInstr *MI) {
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  return TII->getName(MI->getOpcode());
}

static const Function *getCalledFunction(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isGlobal())
      continue;
    const Function *Func = dyn_cast<Function>(MO.getGlobal());
    if (Func != nullptr)
      return Func;
  }
  return nullptr;
}

inline std::string stackSlotToString(const StackSlot &Slot) {
  return std::visit(
      Overload{
          [](const FunctionCallReturnLabelSlot &Ret) -> std::string {
            return "RET[" +
                   std::string(getCalledFunction(*Ret.Call)->getName()) + "]";
          },
          [](const FunctionReturnLabelSlot &) -> std::string { return "RET"; },
          [](const VariableSlot &Var) -> std::string {
            SmallString<64> S;
            llvm::raw_svector_ostream OS(S);
            OS << printReg(Var.VirtualReg, nullptr, 0, nullptr);
            return std::string(S);
            ;
          },
          [](const LiteralSlot &Lit) -> std::string {
            SmallString<64> S;
            Lit.Value.toStringSigned(S);
            return std::string(S);
          },
          [](const TemporarySlot &Tmp) -> std::string {
            SmallString<128> S;
            llvm::raw_svector_ostream OS(S);
            OS << "TMP[" << getInstName(Tmp.MI) << ", ";
            OS << std::to_string(Tmp.Index) + "]";
            return std::string(S);
          },
          [](const JunkSlot &Junk) -> std::string { return "JUNK"; }},
      Slot);
  ;
}

inline std::string stackToString(Stack const &S) {
  std::string Result("[ ");
  for (auto const &Slot : S)
    Result += stackSlotToString(Slot) + ' ';
  Result += ']';
  return Result;
}

void ControlFlowGraphPrinter::operator()(CFG &Cfg) {
  (*this)(Cfg.FuncInfo);
  for (const auto &Block : Cfg.Blocks)
    printBlock(Block);
}

void ControlFlowGraphPrinter::operator()(CFG::FunctionInfo const &Info) {
  OS << "FunctionEntry_" << Info.MF->getName() << "_" << getBlockId(*Info.Entry)
     << " [label=\"";
  OS << "function " << Info.MF->getName() << "\"];\n";
  OS << "Entry block: " << getBlockId(*Info.Entry) << ";\n";
}

std::string ControlFlowGraphPrinter::getBlockId(CFG::BasicBlock const &Block) {
  return std::to_string(Block.MBB->getNumber()) + "." +
         std::string(Block.MBB->getName());
}

void ControlFlowGraphPrinter::printBlock(CFG::BasicBlock const &Block) {
  OS << "Block" << getBlockId(Block) << " [label=\"\\\n";

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
    OS << stackToString(Op.Input) << " => " << stackToString(Op.Output)
       << "\\l\\\n";
  }
  OS << "\"];\n";

  std::visit(
      Overload{[&](const CFG::BasicBlock::Jump &Jump) {
                 OS << "Block" << getBlockId(Block) << " -> Block"
                    << getBlockId(Block) << "Exit [arrowhead=none];\n";
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"";
                 OS << "Jump\" shape=oval];\n";
                 OS << "Block" << getBlockId(Block) << "Exit -> Block"
                    << getBlockId(*Jump.Target) << ";\n";
               },
               [&](const CFG::BasicBlock::ConditionalJump &CondJump) {
                 OS << "Block" << getBlockId(Block) << " -> Block"
                    << getBlockId(Block) << "Exit;\n";
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
                 OS << stackSlotToString(CondJump.Condition);
                 OS << "| { <0> Zero | <1> NonZero }}\" shape=Mrecord];\n";
                 OS << "Block" << getBlockId(Block);
                 OS << "Exit:0 -> Block" << getBlockId(*CondJump.Zero) << ";\n";
                 OS << "Block" << getBlockId(Block);
                 OS << "Exit:1 -> Block" << getBlockId(*CondJump.NonZero)
                    << ";\n";
               },
               [&](const CFG::BasicBlock::FunctionReturn &Return) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"FunctionReturn["
                    << Return.Info->MF->getName() << "]\"];\n";
                 OS << "Block" << getBlockId(Block) << " -> Block"
                    << getBlockId(Block) << "Exit;\n";
               },
               [&](const CFG::BasicBlock::Terminated &) {
                 OS << "Block" << getBlockId(Block)
                    << "Exit [label=\"Terminated\"];\n";
                 OS << "Block" << getBlockId(Block) << " -> Block"
                    << getBlockId(Block) << "Exit;\n";
               },
               [&](const CFG::BasicBlock::InvalidExit &) {
                 assert(0 && "Invalid basic block exit");
               }},
      Block.Exit);
  OS << "\n";
}

std::unique_ptr<CFG> ControlFlowGraphBuilder::build(MachineFunction &MF,
                                                    const LiveIntervals &LIS) {
  auto Result = std::make_unique<CFG>();
  ControlFlowGraphBuilder Builder(*Result, MF, LIS);

  for (MachineBasicBlock &MBB : MF)
    Result->createBlock(&MBB);

  Result->FuncInfo.MF = &MF;
  Result->FuncInfo.Entry = &Result->getBlock(&MF.front());
  for (MachineBasicBlock &MBB : MF)
    Builder.handleBasicBlock(MBB);

  for (MachineBasicBlock &MBB : MF)
    Builder.handleBasicBlockSuccessors(MBB);

  return Result;
}

void ControlFlowGraphBuilder::handleBasicBlock(const MachineBasicBlock &MBB) {
  CurrentBlock = &Cfg.getBlock(&MBB);
  for (const MachineInstr &MI : MBB)
    handleMachineInstr(MI);
}

void ControlFlowGraphBuilder::collectInOut(const MachineInstr &MI, Stack &Input,
                                           Stack &Output) {
  for (const auto &MO : reverse(MI.explicit_uses())) {
    if (!MO.isReg())
      continue;

    const Register Reg = MO.getReg();
    // SP is not used anyhow.
    if (Reg == EVM::SP)
      continue;

    StackSlot Slot = VariableSlot{Reg};
    SlotIndex Idx = LIS.getInstructionIndex(MI);
    const LiveInterval *LI = &LIS.getInterval(Reg);
    LiveQueryResult LRQ = LI->Query(Idx);
    const VNInfo *VNI = LRQ.valueIn();
    assert(VNI && "Use of non-existing value");
    if (LI->containsOneValue()) {
      assert(!VNI->isPHIDef());
      const MachineInstr *DefMI = LIS.getInstructionFromIndex(VNI->def);
      assert(DefMI && "Dead valno in interval");
      if (DefMI->getOpcode() == EVM::CONST_I256) {
        const APInt Imm = DefMI->getOperand(1).getCImm()->getValue();
        Slot = LiteralSlot{std::move(Imm)};
      }
    }
    Input.push_back(Slot);
  }

  unsigned ArgsNumber = 0;
  for (const auto &MO : MI.defs()) {
    Output.push_back(TemporarySlot{&MI, MO.getReg(), ArgsNumber++});
  }
}

void ControlFlowGraphBuilder::handleMachineInstr(const MachineInstr &MI) {
  // First, handle instruction itself.
  unsigned Opc = MI.getOpcode();
  switch (Opc) {
  case EVM::FCALL:
    handleFunctionCall(MI);
    break;
  case EVM::RET:
    handleReturn(MI);
    return;
  case EVM::JUMP:
    [[fallthrough]];
  case EVM::JUMPI:
    // Branch instructions are handled separetly.
    return;
  case EVM::REVERT:
    [[fallthrough]];
  case EVM::RETURN:
    [[fallthrough]];
  case EVM::STOP:
    [[fallthrough]];
  case EVM::INVALID:
    CurrentBlock->Exit = CFG::BasicBlock::Terminated{};
    break;
  case EVM::COPY_I256:
    // The copy instructions represnt just an assignment that is handled below.
    break;
  case EVM::CONST_I256: {
    const LiveInterval *LI = &LIS.getInterval(MI.getOperand(0).getReg());
    // We can ignore this instruction, as we will directly create the literal
    // slot from the immediate value;
    if (LI->containsOneValue())
      return;
  } break;
  default: {
    Stack Input, Output;
    collectInOut(MI, Input, Output);
    CurrentBlock->Operations.emplace_back(CFG::Operation{
        std::move(Input), std::move(Output), CFG::BuiltinCall{&MI}});
  } break;
  }

  // Handle assignment part of the instruction.
  Stack Input, Output;
  std::vector<VariableSlot> Variables;
  switch (MI.getOpcode()) {
  case EVM::CONST_I256: {
    if (InstrToSkip.count(&MI) > 0)
      return;
    const Register DefReg = MI.getOperand(0).getReg();
    const APInt Imm = MI.getOperand(1).getCImm()->getValue();
    Input.push_back(LiteralSlot{std::move(Imm)});
    Output.push_back(VariableSlot{DefReg});
    Variables.push_back(VariableSlot{DefReg});
  } break;
  case EVM::COPY_I256: {
    // Copy instruction corresponds to the assignment operator, so
    // we do not need to create intermadiate TmpSlots.
    const Register UseReg = MI.getOperand(1).getReg();
    const Register DefReg = MI.getOperand(0).getReg();
    Input.push_back(VariableSlot{UseReg});
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
    CurrentBlock->Operations.emplace_back(
        CFG::Operation{std::move(Input), std::move(Output),
                       CFG::Assignment{std::move(Variables)}});
}

void ControlFlowGraphBuilder::handleFunctionCall(const MachineInstr &MI) {
  Stack Input, Output;
  Input.push_back(FunctionCallReturnLabelSlot{&MI});
  collectInOut(MI, Input, Output);
  const Function *Called = getCalledFunction(MI);
  CurrentBlock->Operations.emplace_back(CFG::Operation{
      Input, Output,
      CFG::FunctionCall{&MI, !Called->hasFnAttribute(Attribute::NoReturn)}});
}

void ControlFlowGraphBuilder::handleReturn(const MachineInstr &MI) {
  Cfg.FuncInfo.Exits.emplace_back(CurrentBlock);
  CurrentBlock->Exit = CFG::BasicBlock::FunctionReturn{&Cfg.FuncInfo};
}

void ControlFlowGraphBuilder::handleBasicBlockSuccessors(
    MachineBasicBlock &MBB) {
  MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
  SmallVector<MachineOperand, 1> Cond;
  const TargetInstrInfo *TII = MBB.getParent()->getSubtarget().getInstrInfo();
  CurrentBlock = &Cfg.getBlock(&MBB);
  if (TII->analyzeBranch(MBB, TBB, FBB, Cond)) {
    if (!std::holds_alternative<CFG::BasicBlock::FunctionReturn>(
            CurrentBlock->Exit) &&
        !std::holds_alternative<CFG::BasicBlock::Terminated>(
            CurrentBlock->Exit))
      llvm_unreachable("Unexpected MBB termination");
    return;
  }

  CurrentBlock = &Cfg.getBlock(&MBB);
  if (!TBB || (TBB && Cond.empty())) {
    // Fall through, or unconditional jump.
    bool FallThrough = !TBB;
    if (!TBB) {
      assert(MBB.getSingleSuccessor());
      TBB = MBB.getFallThrough();
      assert(TBB);
    }
    CFG::BasicBlock &Target = Cfg.getBlock(TBB);
    CurrentBlock->Exit = CFG::BasicBlock::Jump{&Target, FallThrough};
    Target.Entries.insert(CurrentBlock);
  } else if (TBB && !Cond.empty()) {
    // Conditional jump + fallthrough or unconditional jump.
    bool FallThrough = !FBB;
    if (!FBB) {
      FBB = MBB.getFallThrough();
      assert(FBB);
    }
    CFG::BasicBlock &NonZeroTarget = Cfg.getBlock(TBB);
    CFG::BasicBlock &ZeroTarget = Cfg.getBlock(FBB);
    assert(Cond[0].isReg());
    auto CondSlot = VariableSlot{Cond[0].getReg()};
    CurrentBlock->Exit = CFG::BasicBlock::ConditionalJump{
        std::move(CondSlot), &NonZeroTarget, &ZeroTarget, FallThrough};
    NonZeroTarget.Entries.insert(CurrentBlock);
    ZeroTarget.Entries.insert(CurrentBlock);
  }
}
