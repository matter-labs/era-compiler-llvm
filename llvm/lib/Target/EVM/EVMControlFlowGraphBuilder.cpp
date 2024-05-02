#include "EVM.h"

#include "EVMControlFlowGraphBuilder.h"
#include "EVMHelperUtilities.h"
#include "EVMStackHelpers.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

#include <ostream>
#include <variant>

using namespace llvm;

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
                 OS << "Jump\" FallThrough:" << Jump.FallThrough;
                 OS << " shape=oval];\n";
                 if (Jump.Backwards)
                   OS << "Backwards";
                 OS << "Block" << getBlockId(Block) << "Exit -> Block"
                    << getBlockId(*Jump.Target) << ";\n";
               },
               [&](const CFG::BasicBlock::ConditionalJump &CondJump) {
                 OS << "Block" << getBlockId(Block) << " -> Block"
                    << getBlockId(Block) << "Exit;\n";
                 OS << "Block" << getBlockId(Block) << "Exit [label=\"{ ";
                 OS << stackSlotToString(CondJump.Condition);
                 OS << "| { <0> Zero | <1> NonZero }}\" FallThrough:";
                 OS << CondJump.FallThrough << " shape=Mrecord];\n";
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
                 OS << "Return values: " << stackToString(Return.RetValues);
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

/// Marks each block that needs to maintain a clean stack. That is each block
/// that has an outgoing path to a function return.
static void markNeedsCleanStack(CFG &_cfg) {
  for (CFG::BasicBlock *exit : _cfg.FuncInfo.Exits)
    EVMUtils::BreadthFirstSearch<CFG::BasicBlock *>{{exit}}.run(
        [&](CFG::BasicBlock *_block, auto _addChild) {
          _block->NeedsCleanStack = true;
          for (CFG::BasicBlock *entry : _block->Entries)
            _addChild(entry);
        });
}

/// Marks each cut-vertex in the CFG, i.e. each block that begins a disconnected
/// sub-graph of the CFG. Entering such a block means that control flow will
/// never return to a previously visited block.
static void markStartsOfSubGraphs(CFG &_cfg) {
  CFG::BasicBlock *entry = _cfg.FuncInfo.Entry;
  /**
   * Detect bridges following Algorithm 1 in
   * https://arxiv.org/pdf/2108.07346.pdf and mark the bridge targets as starts
   * of sub-graphs.
   */
  std::set<CFG::BasicBlock *> visited;
  std::map<CFG::BasicBlock *, size_t> disc;
  std::map<CFG::BasicBlock *, size_t> low;
  std::map<CFG::BasicBlock *, CFG::BasicBlock *> parent;
  size_t time = 0;
  auto dfs = [&](CFG::BasicBlock *_u, auto _recurse) -> void {
    visited.insert(_u);
    disc[_u] = low[_u] = time;
    time++;

    std::vector<CFG::BasicBlock *> children = _u->Entries;
    std::visit(Overload{[&](CFG::BasicBlock::Jump const &_jump) {
                          children.emplace_back(_jump.Target);
                        },
                        [&](CFG::BasicBlock::ConditionalJump const &_jump) {
                          children.emplace_back(_jump.Zero);
                          children.emplace_back(_jump.NonZero);
                        },
                        [&](CFG::BasicBlock::FunctionReturn const &) {},
                        [&](CFG::BasicBlock::Terminated const &) {
                          _u->IsStartOfSubGraph = true;
                        },
                        [&](CFG::BasicBlock::InvalidExit const &) {
                          llvm_unreachable("Unexpected BB terminator");
                        }},
               _u->Exit);
    assert(!EVMUtils::contains(children, _u));

    for (CFG::BasicBlock *v : children) {
      if (!visited.count(v)) {
        parent[v] = _u;
        _recurse(v, _recurse);
        low[_u] = std::min(low[_u], low[v]);
        if (low[v] > disc[_u]) {
          // _u <-> v is a cut edge in the undirected graph
          bool edgeVtoU = EVMUtils::contains(_u->Entries, v);
          bool edgeUtoV = EVMUtils::contains(v->Entries, _u);
          if (edgeVtoU && !edgeUtoV)
            // Cut edge v -> _u
            _u->IsStartOfSubGraph = true;
          else if (edgeUtoV && !edgeVtoU)
            // Cut edge _u -> v
            v->IsStartOfSubGraph = true;
        }
      } else if (v != parent[_u])
        low[_u] = std::min(low[_u], disc[v]);
    }
  };
  dfs(entry, dfs);
}

std::unique_ptr<CFG> ControlFlowGraphBuilder::build(MachineFunction &MF,
                                                    const LiveIntervals &LIS,
                                                    MachineLoopInfo *MLI) {
  auto Result = std::make_unique<CFG>();
  ControlFlowGraphBuilder Builder(*Result, MF, LIS, MLI);

  for (MachineBasicBlock &MBB : MF)
    Result->createBlock(&MBB);

  Result->FuncInfo.MF = &MF;
  Result->FuncInfo.Entry = &Result->getBlock(&MF.front());
  for (MachineBasicBlock &MBB : MF)
    Builder.handleBasicBlock(MBB);

  for (MachineBasicBlock &MBB : MF)
    Builder.handleBasicBlockSuccessors(MBB);

  markStartsOfSubGraphs(*Result);
  markNeedsCleanStack(*Result);

  return Result;
}

void ControlFlowGraphBuilder::handleBasicBlock(const MachineBasicBlock &MBB) {
  CurrentBlock = &Cfg.getBlock(&MBB);
  for (const MachineInstr &MI : MBB)
    handleMachineInstr(MI);
}

void ControlFlowGraphBuilder::collectInOut(const MachineInstr &MI, Stack &Input,
                                           Stack &Output) {
  for (const auto &MO : MI.explicit_uses()) {
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
  case EVM::ARGUMENT:
    Cfg.FuncInfo.Parameters.emplace_back(
        VariableSlot{MI.getOperand(0).getReg()});
    break;
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
  Stack Input, Output;
  collectInOut(MI, Input, Output);
  CurrentBlock->Exit =
      CFG::BasicBlock::FunctionReturn{std::move(Input), &Cfg.FuncInfo};
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
  bool IsLatch = false;
  MachineLoop *ML = MLI->getLoopFor(&MBB);
  if (ML) {
    SmallVector<MachineBasicBlock *, 8> Latches;
    ML->getLoopLatches(Latches);
    IsLatch = std::any_of(
        Latches.begin(), Latches.end(),
        [&MBB](const MachineBasicBlock *Latch) { return &MBB == Latch; });
  }

  if (!TBB || (TBB && Cond.empty())) {
    // Fall through, or unconditional jump.
    bool FallThrough = !TBB;
    if (!TBB) {
      assert(MBB.getSingleSuccessor());
      TBB = MBB.getFallThrough();
      assert(TBB);
    }
    CFG::BasicBlock &Target = Cfg.getBlock(TBB);
    if (IsLatch)
      assert(ML->getHeader() == &MBB && !FallThrough);
    CurrentBlock->Exit = CFG::BasicBlock::Jump{&Target, FallThrough, IsLatch};
    EVMUtils::push_if_noexist(Target.Entries, CurrentBlock);
  } else if (TBB && !Cond.empty()) {
    assert(!IsLatch);
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

    EVMUtils::push_if_noexist(NonZeroTarget.Entries, CurrentBlock);
    EVMUtils::push_if_noexist(ZeroTarget.Entries, CurrentBlock);
  }
}
