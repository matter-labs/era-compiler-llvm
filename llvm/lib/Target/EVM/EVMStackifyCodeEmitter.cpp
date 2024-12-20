//===--- EVMStackifyCodeEmitter.h - Create stackified MIR -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file transforms MIR to the 'stackified' MIR.
//
//===----------------------------------------------------------------------===//

#include "EVMStackifyCodeEmitter.h"
#include "EVMHelperUtilities.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMStackDebug.h"
#include "EVMStackShuffler.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "evm-stackify-code-emitter"

// Return the number of input arguments of the call instruction.
static size_t getCallArgCount(const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  assert(Call->explicit_uses().begin()->isGlobal() &&
         "First operand must be a function");
  size_t NumExplicitInputs =
      Call->getNumExplicitOperands() - Call->getNumExplicitDefs();

  // The first operand is a function, so don't count it. If function
  // will return, we need to account for the return label.
  constexpr size_t NumFuncOp = 1;
  return NumExplicitInputs - NumFuncOp + EVMUtils::callWillReturn(Call);
}

size_t EVMStackifyCodeEmitter::CodeEmitter::stackHeight() const {
  return StackHeight;
}

void EVMStackifyCodeEmitter::CodeEmitter::enterMBB(MachineBasicBlock *MBB,
                                                   int Height) {
  StackHeight = Height;
  CurMBB = MBB;
  LLVM_DEBUG(dbgs() << "\n"
                    << "Set stack height: " << StackHeight << "\n");
  LLVM_DEBUG(dbgs() << "Setting current location to: " << MBB->getNumber()
                    << "." << MBB->getName() << "\n");
}

void EVMStackifyCodeEmitter::CodeEmitter::emitInst(const MachineInstr *MI) {
  unsigned Opc = MI->getOpcode();
  assert(Opc != EVM::JUMP && Opc != EVM::JUMPI && Opc != EVM::ARGUMENT &&
         Opc != EVM::RET && Opc != EVM::CONST_I256 && Opc != EVM::COPY_I256 &&
         Opc != EVM::FCALL && "Unexpected instruction");

  size_t NumInputs = MI->getNumExplicitOperands() - MI->getNumExplicitDefs();
  assert(StackHeight >= NumInputs && "Not enough operands on the stack");
  StackHeight -= NumInputs;
  StackHeight += MI->getNumExplicitDefs();

  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitSWAP(unsigned Depth) {
  unsigned Opc = EVM::getSWAPOpcode(Depth);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitDUP(unsigned Depth) {
  StackHeight += 1;
  unsigned Opc = EVM::getDUPOpcode(Depth);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitPOP() {
  assert(StackHeight > 0 && "Expected at least one operand on the stack");
  StackHeight -= 1;
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::POP_S));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitConstant(const APInt &Val) {
  StackHeight += 1;
  unsigned Opc = EVM::getPUSHOpcode(Val);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  if (Opc != EVM::PUSH0)
    NewMI.addCImm(ConstantInt::get(MF.getFunction().getContext(), Val));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitConstant(uint64_t Val) {
  emitConstant(APInt(256, Val));
}

void EVMStackifyCodeEmitter::CodeEmitter::emitSymbol(const MachineInstr *MI,
                                                     MCSymbol *Symbol) {
  unsigned Opc = MI->getOpcode();
  assert(Opc == EVM::DATASIZE ||
         Opc == EVM::DATAOFFSET && "Unexpected symbol instruction");
  StackHeight += 1;
  // This is codegen-only instruction, that will be converted into PUSH4.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)))
                   .addSym(Symbol);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitLabelReference(
    const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  StackHeight += 1;
  auto [It, Inserted] = CallReturnSyms.try_emplace(Call);
  if (Inserted)
    It->second = MF.getContext().createTempSymbol("FUNC_RET", true);
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::PUSH_LABEL))
          .addSym(It->second);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitFuncCall(const MachineInstr *MI) {
  assert(MI->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  assert(CurMBB == MI->getParent());

  size_t NumInputs = getCallArgCount(MI);
  assert(StackHeight >= NumInputs && "Not enough operands on the stack");
  StackHeight -= NumInputs;

  // PUSH_LABEL increases the stack height on 1, but we don't increase it
  // explicitly here, as the label will be consumed by the following JUMP.
  StackHeight += MI->getNumExplicitDefs();

  // Create pseudo jump to the function, that will be expanded into PUSH and
  // JUMP instructions in the AsmPrinter.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoCALL))
                   .addGlobalAddress(MI->explicit_uses().begin()->getGlobal());

  // If this function returns, add a return label so we can emit it together
  // with JUMPDEST. This is taken care in the AsmPrinter.
  if (EVMUtils::callWillReturn(MI))
    NewMI.addSym(CallReturnSyms.at(MI));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitRet(const MachineInstr *MI) {
  assert(MI->getOpcode() == EVM::RET && "Unexpected ret instruction");
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoRET));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitUncondJump(
    const MachineInstr *MI, MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMP &&
         "Unexpected unconditional jump instruction");
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoJUMP))
                   .addMBB(Target);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitCondJump(
    const MachineInstr *MI, MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMPI &&
         "Unexpected conditional jump instruction");
  assert(StackHeight > 0 && "Expected at least one operand on the stack");
  StackHeight -= 1;
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoJUMPI))
                   .addMBB(Target);
  verify(NewMI);
}

// Verify that a stackified instruction doesn't have registers and dump it.
void EVMStackifyCodeEmitter::CodeEmitter::verify(const MachineInstr *MI) const {
  assert(EVMInstrInfo::isStack(MI) &&
         "Only stackified instructions are allowed");
  assert(all_of(MI->operands(),
                [](const MachineOperand &MO) { return !MO.isReg(); }) &&
         "Registers are not allowed in stackified instructions");

  LLVM_DEBUG(dbgs() << "Adding: " << *MI << "stack height: " << StackHeight
                    << "\n");
}
void EVMStackifyCodeEmitter::CodeEmitter::finalize() {
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : make_early_inc_range(MBB))
      // Remove all the instructions that are not stackified.
      // TODO: #749: Fix debug info for stackified instructions and don't
      // remove debug instructions.
      if (!EVMInstrInfo::isStack(&MI))
        MI.eraseFromParent();

  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  MFI->setIsStackified();

  // In a stackified code register liveness has no meaning.
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MRI.invalidateLiveness();
}

void EVMStackifyCodeEmitter::adjustStackForInst(const MachineInstr *MI,
                                                size_t NumArgs) {
  // Remove arguments from CurrentStack.
  CurrentStack.erase(CurrentStack.end() - NumArgs, CurrentStack.end());

  // Push return values to CurrentStack.
  unsigned Idx = 0;
  for (const auto &MO : MI->defs()) {
    assert(MO.isReg());
    CurrentStack.emplace_back(TemporarySlot{MI, MO.getReg(), Idx++});
  }
  assert(Emitter.stackHeight() == CurrentStack.size());
}

void EVMStackifyCodeEmitter::visitCall(const FunctionCall &Call) {
  size_t NumArgs = getCallArgCount(Call.MI);
  // Validate stack.
  assert(Emitter.stackHeight() == CurrentStack.size());
  assert(CurrentStack.size() >= NumArgs);

  // Assert that we got the correct return label on stack.
  if (EVMUtils::callWillReturn(Call.MI)) {
    [[maybe_unused]] const auto *returnLabelSlot =
        std::get_if<FunctionCallReturnLabelSlot>(
            &CurrentStack[CurrentStack.size() - NumArgs]);
    assert(returnLabelSlot && returnLabelSlot->Call == Call.MI);
  }

  // Emit call.
  Emitter.emitFuncCall(Call.MI);
  adjustStackForInst(Call.MI, NumArgs);
}

void EVMStackifyCodeEmitter::visitInst(const BuiltinCall &Call) {
  size_t NumArgs =
      Call.MI->getNumExplicitOperands() - Call.MI->getNumExplicitDefs();
  // Validate stack.
  assert(Emitter.stackHeight() == CurrentStack.size());
  assert(CurrentStack.size() >= NumArgs);
  // TODO: assert that we got a correct stack for the call.

  // Emit instruction.
  Emitter.emitInst(Call.MI);
  adjustStackForInst(Call.MI, NumArgs);
}

void EVMStackifyCodeEmitter::visitAssign(const Assignment &Assignment) {
  assert(Emitter.stackHeight() == CurrentStack.size());

  // Invalidate occurrences of the assigned variables.
  for (auto &CurrentSlot : CurrentStack)
    if (const VariableSlot *VarSlot = std::get_if<VariableSlot>(&CurrentSlot))
      if (is_contained(Assignment.Variables, *VarSlot))
        CurrentSlot = JunkSlot{};

  // Assign variables to current stack top.
  assert(CurrentStack.size() >= Assignment.Variables.size());
  llvm::copy(Assignment.Variables,
             CurrentStack.end() - Assignment.Variables.size());
}

bool EVMStackifyCodeEmitter::areLayoutsCompatible(const Stack &SourceStack,
                                                  const Stack &TargetStack) {
  return SourceStack.size() == TargetStack.size() &&
         all_of(zip_equal(SourceStack, TargetStack), [](const auto &Pair) {
           const auto &[Src, Tgt] = Pair;
           return std::holds_alternative<JunkSlot>(Tgt) || (Src == Tgt);
         });
}

void EVMStackifyCodeEmitter::createStackLayout(const Stack &TargetStack) {
  auto SlotVariableName = [](const StackSlot &Slot) {
    return std::visit(
        Overload{
            [&](const VariableSlot &Var) {
              SmallString<1024> StrBuf;
              raw_svector_ostream OS(StrBuf);
              OS << printReg(Var.VirtualReg, nullptr, 0, nullptr);
              return std::string(StrBuf.c_str());
            },
            [&](const FunctionCallReturnLabelSlot &) { return std::string(); },
            [&](const FunctionReturnLabelSlot &) { return std::string(); },
            [&](const LiteralSlot &) { return std::string(); },
            [&](const SymbolSlot &) { return std::string(); },
            [&](const TemporarySlot &) { return std::string(); },
            [&](const JunkSlot &) { return std::string(); }},
        Slot);
  };

  assert(Emitter.stackHeight() == CurrentStack.size());
  // ::createStackLayout asserts that it has successfully achieved the target
  // layout.
  ::createStackLayout(
      CurrentStack, TargetStack,
      // Swap callback.
      [&](unsigned I) {
        assert(CurrentStack.size() == Emitter.stackHeight());
        assert(I > 0 && I < CurrentStack.size());
        if (I <= 16) {
          Emitter.emitSWAP(I);
        } else {
          int Deficit = static_cast<int>(I) - 16;
          const StackSlot &DeepSlot = CurrentStack[CurrentStack.size() - I - 1];
          std::string VarNameDeep = SlotVariableName(DeepSlot);
          std::string VarNameTop = SlotVariableName(CurrentStack.back());
          std::string Msg =
              (Twine("cannot swap ") +
               (VarNameDeep.empty() ? ("slot " + stackSlotToString(DeepSlot))
                                    : (Twine("variable ") + VarNameDeep)) +
               " with " +
               (VarNameTop.empty()
                    ? ("slot " + stackSlotToString(CurrentStack.back()))
                    : (Twine("variable ") + VarNameTop)) +
               ": too deep in the stack by " + std::to_string(Deficit) +
               " slots in " + stackToString(CurrentStack))
                  .str();

          report_fatal_error(MF.getName() + Twine(": ") + Msg);
        }
      },
      // Push or dup callback.
      [&](const StackSlot &Slot) {
        assert(CurrentStack.size() == Emitter.stackHeight());

        // Dup the slot, if already on stack and reachable.
        auto SlotIt = llvm::find(llvm::reverse(CurrentStack), Slot);
        if (SlotIt != CurrentStack.rend()) {
          unsigned Depth = std::distance(CurrentStack.rbegin(), SlotIt);
          if (Depth < 16) {
            Emitter.emitDUP(static_cast<unsigned>(Depth + 1));
            return;
          }
          if (!isRematerializable(Slot)) {
            std::string VarName = SlotVariableName(Slot);
            std::string Msg =
                ((VarName.empty() ? "slot " + stackSlotToString(Slot)
                                  : Twine("variable ") + VarName) +
                 " is " + std::to_string(Depth - 15) +
                 " too deep in the stack " + stackToString(CurrentStack))
                    .str();

            report_fatal_error(MF.getName() + ": " + Msg);
            return;
          }
          // else: the slot is too deep in stack, but can be freely generated,
          // we fall through to push it again.
        }

        // The slot can be freely generated or is an unassigned return variable.
        // Push it.
        std::visit(
            Overload{[&](const LiteralSlot &Literal) {
                       Emitter.emitConstant(Literal.Value);
                     },
                     [&](const SymbolSlot &Symbol) {
                       Emitter.emitSymbol(Symbol.MI, Symbol.Symbol);
                     },
                     [&](const FunctionReturnLabelSlot &) {
                       llvm_unreachable("Cannot produce function return label");
                     },
                     [&](const FunctionCallReturnLabelSlot &ReturnLabel) {
                       Emitter.emitLabelReference(ReturnLabel.Call);
                     },
                     [&](const VariableSlot &Variable) {
                       llvm_unreachable("Variable not found on stack");
                     },
                     [&](const TemporarySlot &) {
                       llvm_unreachable("Function call result requested, but "
                                        "not found on stack.");
                     },
                     [&](const JunkSlot &) {
                       // Note: this will always be popped, so we can push
                       // anything.
                       Emitter.emitConstant(0);
                     }},
            Slot);
      },
      // Pop callback.
      [&]() { Emitter.emitPOP(); });

  assert(Emitter.stackHeight() == CurrentStack.size());
}

void EVMStackifyCodeEmitter::createOperationLayout(const Operation &Op) {
  // Create required layout for entering the Operation.
  // Check if we can choose cheaper stack shuffling if the Operation is an
  // instruction with commutable arguments.
  bool SwapCommutable = false;
  if (const auto *Inst = std::get_if<BuiltinCall>(&Op.Operation);
      Inst && Inst->MI->isCommutable()) {
    // Get the stack layout before the instruction.
    const Stack &DefaultTargetStack = Layout.getOperationEntryLayout(&Op);
    size_t DefaultCost =
        EvaluateStackTransform(CurrentStack, DefaultTargetStack);

    // Commutable operands always take top two stack slots.
    const unsigned OpIdx1 = 0, OpIdx2 = 1;
    assert(DefaultTargetStack.size() > 1);

    // Swap the commutable stack items and measure the stack shuffling cost
    // again.
    Stack CommutedTargetStack = DefaultTargetStack;
    std::swap(CommutedTargetStack[CommutedTargetStack.size() - OpIdx1 - 1],
              CommutedTargetStack[CommutedTargetStack.size() - OpIdx2 - 1]);
    size_t CommutedCost =
        EvaluateStackTransform(CurrentStack, CommutedTargetStack);
    // Choose the cheapest transformation.
    SwapCommutable = CommutedCost < DefaultCost;
    createStackLayout(SwapCommutable ? CommutedTargetStack
                                     : DefaultTargetStack);
  } else {
    createStackLayout(Layout.getOperationEntryLayout(&Op));
  }

  // Assert that we have the inputs of the Operation on stack top.
  assert(CurrentStack.size() == Emitter.stackHeight());
  assert(CurrentStack.size() >= Op.Input.size());
  Stack StackInput(CurrentStack.end() - Op.Input.size(), CurrentStack.end());
  // Adjust the StackInput if needed.
  if (SwapCommutable) {
    std::swap(StackInput[StackInput.size() - 1],
              StackInput[StackInput.size() - 2]);
  }
  assert(areLayoutsCompatible(StackInput, Op.Input));
}

void EVMStackifyCodeEmitter::run() {
  assert(CurrentStack.empty() && Emitter.stackHeight() == 0);

  SmallPtrSet<MachineBasicBlock *, 32> Visited;
  SmallVector<MachineBasicBlock *, 32> WorkList{&MF.front()};
  while (!WorkList.empty()) {
    auto *Block = WorkList.pop_back_val();
    if (!Visited.insert(Block).second)
      continue;

    // Might set some slots to junk, if not required by the block.
    CurrentStack = Layout.getMBBEntryLayout(Block);
    Emitter.enterMBB(Block, CurrentStack.size());

    for (const auto &Operation : StackModel.getOperations(Block)) {
      createOperationLayout(Operation);

      [[maybe_unused]] size_t BaseHeight =
          CurrentStack.size() - Operation.Input.size();

      // Perform the Operation.
      std::visit(Overload{[this](const FunctionCall &Call) { visitCall(Call); },
                          [this](const BuiltinCall &Call) { visitInst(Call); },
                          [this](const Assignment &Assignment) {
                            visitAssign(Assignment);
                          }},
                 Operation.Operation);

      // Assert that the Operation produced its proclaimed output.
      assert(CurrentStack.size() == Emitter.stackHeight());
      assert(CurrentStack.size() == BaseHeight + Operation.Output.size());
      assert(CurrentStack.size() >= Operation.Output.size());
      assert(areLayoutsCompatible(
          Stack(CurrentStack.end() - Operation.Output.size(),
                CurrentStack.end()),
          Operation.Output));
    }

    // Exit the block.
    const EVMMBBTerminatorsInfo *TermInfo = CFGInfo.getTerminatorsInfo(Block);
    MBBExitType ExitType = TermInfo->getExitType();
    if (ExitType == MBBExitType::UnconditionalBranch) {
      auto [UncondBr, Target, _] = TermInfo->getUnconditionalBranch();
      // Create the stack expected at the jump target.
      createStackLayout(Layout.getMBBEntryLayout(Target));

      // Assert that we have a valid stack for the target.
      assert(
          areLayoutsCompatible(CurrentStack, Layout.getMBBEntryLayout(Target)));

      if (UncondBr)
        Emitter.emitUncondJump(UncondBr, Target);
      WorkList.emplace_back(Target);
    } else if (ExitType == MBBExitType::ConditionalBranch) {
      auto [CondBr, UncondBr, TrueBB, FalseBB, Condition] =
          TermInfo->getConditionalBranch();
      // Create the shared entry layout of the jump targets, which is
      // stored as exit layout of the current block.
      createStackLayout(Layout.getMBBExitLayout(Block));

      // Assert that we have the correct condition on stack.
      assert(!CurrentStack.empty());
      assert(CurrentStack.back() == StackModel.getStackSlot(*Condition));

      // Emit the conditional jump to the non-zero label and update the
      // stored stack.
      assert(CondBr);
      Emitter.emitCondJump(CondBr, TrueBB);
      CurrentStack.pop_back();

      // Assert that we have a valid stack for both jump targets.
      assert(
          areLayoutsCompatible(CurrentStack, Layout.getMBBEntryLayout(TrueBB)));
      assert(areLayoutsCompatible(CurrentStack,
                                  Layout.getMBBEntryLayout(FalseBB)));

      // Generate unconditional jump if needed.
      if (UncondBr)
        Emitter.emitUncondJump(UncondBr, FalseBB);
      WorkList.emplace_back(TrueBB);
      WorkList.emplace_back(FalseBB);
    } else if (ExitType == MBBExitType::FunctionReturn) {
      assert(!MF.getFunction().hasFnAttribute(Attribute::NoReturn));
      // Create the function return layout and jump.
      const MachineInstr *MI = TermInfo->getFunctionReturn();
      assert(StackModel.getReturnArguments(*MI) ==
             Layout.getMBBExitLayout(Block));
      createStackLayout(Layout.getMBBExitLayout(Block));
      Emitter.emitRet(MI);
    }
  }
  Emitter.finalize();
}
