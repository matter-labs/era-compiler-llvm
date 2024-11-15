//===----- EVMControlFlowGraphBuilder.CPP - CFG builder ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file builds the Control Flow Graph used for the backward propagation
// stackification algorithm.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"

#include "EVMControlFlowGraphBuilder.h"
#include "EVMHelperUtilities.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMStackDebug.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

#include <ostream>
#include <variant>

using namespace llvm;

#define DEBUG_TYPE "evm-control-flow-graph-builder"

/// Marks each block that needs to maintain a clean stack. That is each block
/// that has an outgoing path to a function return.
static void markNeedsCleanStack(CFG &Cfg) {
  for (CFG::BasicBlock *Exit : Cfg.FuncInfo.Exits)
    EVMUtils::BreadthFirstSearch<CFG::BasicBlock *>{{Exit}}.run(
        [&](CFG::BasicBlock *Block, auto AddChild) {
          Block->NeedsCleanStack = true;
          // TODO: it seems this is not needed, as the return block has
          // no childs.
          for (CFG::BasicBlock *Entry : Block->Entries)
            AddChild(Entry);
        });
}

/// Marks each cut-vertex in the CFG, i.e. each block that begins a disconnected
/// sub-graph of the CFG. Entering such a block means that control flow will
/// never return to a previously visited block.
static void markStartsOfSubGraphs(CFG &Cfg) {
  CFG::BasicBlock *Entry = Cfg.FuncInfo.Entry;
  /**
   * Detect bridges following Algorithm 1 in
   * https://arxiv.org/pdf/2108.07346.pdf and mark the bridge targets as starts
   * of sub-graphs.
   */
  std::set<CFG::BasicBlock *> Visited;
  std::map<CFG::BasicBlock *, size_t> Disc;
  std::map<CFG::BasicBlock *, size_t> Low;
  std::map<CFG::BasicBlock *, CFG::BasicBlock *> Parent;
  size_t Time = 0;
  auto Dfs = [&](CFG::BasicBlock *U, auto Recurse) -> void {
    Visited.insert(U);
    Disc[U] = Low[U] = Time;
    Time++;

    std::vector<CFG::BasicBlock *> Children = U->Entries;
    std::visit(Overload{[&](CFG::BasicBlock::Jump const &Jump) {
                          Children.emplace_back(Jump.Target);
                        },
                        [&](CFG::BasicBlock::ConditionalJump const &Jump) {
                          Children.emplace_back(Jump.Zero);
                          Children.emplace_back(Jump.NonZero);
                        },
                        [&](CFG::BasicBlock::FunctionReturn const &) {},
                        [&](CFG::BasicBlock::Terminated const &) {
                          U->IsStartOfSubGraph = true;
                        },
                        [&](CFG::BasicBlock::Unreachable const &) {
                          U->IsStartOfSubGraph = true;
                        },
                        [&](CFG::BasicBlock::InvalidExit const &) {
                          llvm_unreachable("Unexpected BB terminator");
                        }},
               U->Exit);

    for (CFG::BasicBlock *V : Children) {
      // Ignore the loop edge, as it cannot be the bridge.
      if (V == U)
        continue;

      if (!Visited.count(V)) {
        Parent[V] = U;
        Recurse(V, Recurse);
        Low[U] = std::min(Low[U], Low[V]);
        if (Low[V] > Disc[U]) {
          // U <-> V is a cut edge in the undirected graph
          bool EdgeVtoU = EVMUtils::contains(U->Entries, V);
          bool EdgeUtoV = EVMUtils::contains(V->Entries, U);
          if (EdgeVtoU && !EdgeUtoV)
            // Cut edge V -> U
            U->IsStartOfSubGraph = true;
          else if (EdgeUtoV && !EdgeVtoU)
            // Cut edge U -> v
            V->IsStartOfSubGraph = true;
        }
      } else if (V != Parent[U])
        Low[U] = std::min(Low[U], Disc[V]);
    }
  };
  Dfs(Entry, Dfs);
}

std::unique_ptr<CFG> ControlFlowGraphBuilder::build(MachineFunction &MF,
                                                    const LiveIntervals &LIS,
                                                    MachineLoopInfo *MLI) {
  auto Result = std::make_unique<CFG>();
  ControlFlowGraphBuilder Builder(*Result, LIS, MLI);

  for (MachineBasicBlock &MBB : MF)
    Result->createBlock(&MBB);

  Result->FuncInfo.MF = &MF;
  Result->FuncInfo.Entry = &Result->getBlock(&MF.front());
  const Function &F = MF.getFunction();
  if (F.hasFnAttribute(Attribute::NoReturn))
    Result->FuncInfo.CanContinue = false;

  // Handle function parameters
  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  Result->FuncInfo.Parameters =
      std::vector<StackSlot>(MFI->getNumParams(), JunkSlot{});
  for (const MachineInstr &MI : MF.front()) {
    if (MI.getOpcode() == EVM::ARGUMENT) {
      int64_t ArgIdx = MI.getOperand(1).getImm();
      Result->FuncInfo.Parameters[ArgIdx] =
          VariableSlot{MI.getOperand(0).getReg()};
    }
  }

  for (MachineBasicBlock &MBB : MF)
    Builder.handleBasicBlock(MBB);

  for (MachineBasicBlock &MBB : MF)
    Builder.handleBasicBlockSuccessors(MBB);

  markStartsOfSubGraphs(*Result);
  markNeedsCleanStack(*Result);

  LLVM_DEBUG({
    dbgs() << "************* CFG *************\n";
    ControlFlowGraphPrinter P(dbgs());
    P(*Result);
  });

  return Result;
}

void ControlFlowGraphBuilder::handleBasicBlock(MachineBasicBlock &MBB) {
  CurrentBlock = &Cfg.getBlock(&MBB);
  for (MachineInstr &MI : MBB)
    handleMachineInstr(MI);
}

StackSlot ControlFlowGraphBuilder::getDefiningSlot(const MachineInstr &MI,
                                                   Register Reg) const {
  StackSlot Slot = VariableSlot{Reg};
  SlotIndex Idx = LIS.getInstructionIndex(MI);
  const LiveInterval *LI = &LIS.getInterval(Reg);
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

void ControlFlowGraphBuilder::collectInstrOperands(const MachineInstr &MI,
                                                   Stack *Input,
                                                   Stack *Output) const {
  if (Input) {
    for (const auto &MO : reverse(MI.explicit_uses())) {
      // All the non-register operands are handled in instruction specific
      // handlers.
      if (!MO.isReg())
        continue;

      const Register Reg = MO.getReg();
      // SP is not used anyhow.
      if (Reg == EVM::SP)
        continue;

      Input->push_back(getDefiningSlot(MI, Reg));
    }
  }

  if (Output) {
    unsigned ArgsNumber = 0;
    for (const auto &MO : MI.defs())
      Output->push_back(TemporarySlot{&MI, MO.getReg(), ArgsNumber++});
  }
}

void ControlFlowGraphBuilder::handleMachineInstr(MachineInstr &MI) {
  bool TerminatesOrReverts = false;
  unsigned Opc = MI.getOpcode();
  switch (Opc) {
  case EVM::STACK_LOAD:
    [[fallthrough]];
  case EVM::STACK_STORE:
    llvm_unreachable("Unexpected stack memory instruction");
    return;
  case EVM::ARGUMENT:
    // Is handled above.
    return;
  case EVM::FCALL:
    handleFunctionCall(MI);
    break;
  case EVM::RET:
    handleReturn(MI);
    return;
  case EVM::JUMP:
    [[fallthrough]];
  case EVM::JUMPI:
    // Branch instructions are handled separately.
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
  case EVM::REVERT:
    [[fallthrough]];
  case EVM::RETURN:
    [[fallthrough]];
  case EVM::STOP:
    [[fallthrough]];
  case EVM::INVALID:
    CurrentBlock->Exit = CFG::BasicBlock::Terminated{};
    TerminatesOrReverts = true;
    [[fallthrough]];
  default: {
    Stack Input, Output;
    collectInstrOperands(MI, &Input, &Output);
    CurrentBlock->Operations.emplace_back(CFG::Operation{
        std::move(Input), std::move(Output),
        CFG::BuiltinCall{&MI, MI.isCommutable(), TerminatesOrReverts}});
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
    Stack In;
    collectInstrOperands(MI, &In, nullptr);
    Input = In;
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
    CurrentBlock->Operations.emplace_back(
        CFG::Operation{std::move(Input), std::move(Output),
                       CFG::Assignment{std::move(Variables)}});
}

void ControlFlowGraphBuilder::handleFunctionCall(const MachineInstr &MI) {
  Stack Input, Output;
  const Function *Called = getCalledFunction(MI);
  bool IsNoReturn = Called->hasFnAttribute(Attribute::NoReturn);
  if (IsNoReturn)
    CurrentBlock->Exit = CFG::BasicBlock::Terminated{};
  else
    Input.push_back(FunctionCallReturnLabelSlot{&MI});
  collectInstrOperands(MI, &Input, &Output);
  CurrentBlock->Operations.emplace_back(
      CFG::Operation{Input, Output,
                     CFG::FunctionCall{&MI, !IsNoReturn,
                                       Input.size() - (IsNoReturn ? 0 : 1)}});
}

void ControlFlowGraphBuilder::handleReturn(const MachineInstr &MI) {
  Cfg.FuncInfo.Exits.emplace_back(CurrentBlock);
  Stack Input;
  collectInstrOperands(MI, &Input, nullptr);
  // We need to reverse input operands to restore original ordering,
  // as it is in the instruction.
  // Calling convention: return values are passed in stack such that the
  // last one specified in the RET instruction is passed on the stack TOP.
  std::reverse(Input.begin(), Input.end());
  CurrentBlock->Exit =
      CFG::BasicBlock::FunctionReturn{std::move(Input), &Cfg.FuncInfo};
}

static std::pair<MachineInstr *, MachineInstr *>
getMBBJumps(MachineBasicBlock &MBB) {
  MachineInstr *CondJump = nullptr;
  MachineInstr *UncondJump = nullptr;
  MachineBasicBlock::reverse_iterator I = MBB.rbegin(), E = MBB.rend();
  while (I != E) {
    if (I->isUnconditionalBranch())
      UncondJump = &*I;
    else if (I->isConditionalBranch())
      CondJump = &*I;
    ++I;
  }
  return std::make_pair(CondJump, UncondJump);
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

#ifndef NDEBUG
  // This corresponds to a noreturn functions at the end of the MBB.
  if (std::holds_alternative<CFG::BasicBlock::Terminated>(CurrentBlock->Exit)) {
    CFG::FunctionCall *Call = std::get_if<CFG::FunctionCall>(
        &CurrentBlock->Operations.back().Operation);
    assert(Call && !Call->CanContinue);
    return;
  }
#endif // NDEBUG

  // This corresponds to 'unreachable' at the BB end.
  if (!TBB && !FBB && MBB.succ_empty()) {
    CurrentBlock->Exit = CFG::BasicBlock::Unreachable{};
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

  std::pair<MachineInstr *, MachineInstr *> MBBJumps = getMBBJumps(MBB);
  if (!TBB || (TBB && Cond.empty())) {
    // Fall through, or unconditional jump.
    bool FallThrough = !TBB;
    if (!TBB) {
      assert(MBB.getSingleSuccessor());
      TBB = MBB.getFallThrough();
      assert(TBB);
    }
    CFG::BasicBlock &Target = Cfg.getBlock(TBB);
    assert(!MBBJumps.first);
    assert((FallThrough && !MBBJumps.second) || !FallThrough);
    CurrentBlock->Exit =
        CFG::BasicBlock::Jump{&Target, FallThrough, IsLatch, MBBJumps.second};
    EVMUtils::emplace_back_unique(Target.Entries, CurrentBlock);
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
    auto CondSlot = getDefiningSlot(*Cond[0].getParent(), Cond[0].getReg());
    CurrentBlock->Exit = CFG::BasicBlock::ConditionalJump{
        std::move(CondSlot), &NonZeroTarget, &ZeroTarget,
        FallThrough,         MBBJumps.first, MBBJumps.second};

    EVMUtils::emplace_back_unique(NonZeroTarget.Entries, CurrentBlock);
    EVMUtils::emplace_back_unique(ZeroTarget.Entries, CurrentBlock);
  }
}
