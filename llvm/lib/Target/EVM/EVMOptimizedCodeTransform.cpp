//===--- EVMOptimizedCodeTransform.h - Create stackified MIR ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file transforms MIR to the 'stackified' MIR using CFG, StackLayout
// and EVMAssembly classes.
//
//===----------------------------------------------------------------------===//

#include "EVMOptimizedCodeTransform.h"
#include "EVMStackDebug.h"
#include "EVMStackShuffler.h"

using namespace llvm;

#define DEBUG_TYPE "evm-optimized-code-transform"

void EVMOptimizedCodeTransform::visitCall(const CFG::FunctionCall &Call) {
  // Validate stack.
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
  assert(CurrentStack.size() >= Call.NumArguments + (Call.CanContinue ? 1 : 0));

  // Assert that we got the correct return label on stack.
  if (Call.CanContinue) {
    [[maybe_unused]] const auto *returnLabelSlot =
        std::get_if<FunctionCallReturnLabelSlot>(
            &CurrentStack.at(CurrentStack.size() - Call.NumArguments - 1));
    assert(returnLabelSlot && returnLabelSlot->Call == Call.Call);
  }

  // Emit code.
  const MachineOperand *CalleeOp = Call.Call->explicit_uses().begin();
  assert(CalleeOp->isGlobal());
  Assembly.emitFuncCall(Call.Call, CalleeOp->getGlobal(),
                        Call.Call->getNumExplicitDefs() - Call.NumArguments -
                            (Call.CanContinue ? 1 : 0),
                        Call.CanContinue);

  // Update stack, remove arguments and return label from CurrentStack.
  for (size_t I = 0; I < Call.NumArguments + (Call.CanContinue ? 1 : 0); ++I)
    CurrentStack.pop_back();

  // Push return values to CurrentStack.
  unsigned Idx = 0;
  for (const auto &MO : Call.Call->defs()) {
    assert(MO.isReg());
    CurrentStack.emplace_back(TemporarySlot{Call.Call, MO.getReg(), Idx++});
  }
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
}

void EVMOptimizedCodeTransform::visitInst(const CFG::BuiltinCall &Call) {
  size_t NumArgs = Call.Builtin->getNumExplicitOperands() -
                   Call.Builtin->getNumExplicitDefs();
  // Validate stack.
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
  assert(CurrentStack.size() >= NumArgs);
  // TODO: assert that we got a correct stack for the call.

  // Emit code.
  Assembly.emitInst(Call.Builtin);

  // Update stack and remove arguments from CurrentStack.
  for (size_t i = 0; i < NumArgs; ++i)
    CurrentStack.pop_back();

  // Push return values to CurrentStack.
  unsigned Idx = 0;
  for (const auto &MO : Call.Builtin->defs()) {
    assert(MO.isReg());
    CurrentStack.emplace_back(TemporarySlot{Call.Builtin, MO.getReg(), Idx++});
  }
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
}

void EVMOptimizedCodeTransform::visitAssign(const CFG::Assignment &Assignment) {
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));

  // Invalidate occurrences of the assigned variables.
  for (auto &CurrentSlot : CurrentStack)
    if (const VariableSlot *VarSlot = std::get_if<VariableSlot>(&CurrentSlot))
      if (is_contained(Assignment.Variables, *VarSlot))
        CurrentSlot = JunkSlot{};

  // Assign variables to current stack top.
  assert(CurrentStack.size() >= Assignment.Variables.size());
  auto StackRange = make_range(CurrentStack.end() - Assignment.Variables.size(),
                               CurrentStack.end());
  auto RangeIt = StackRange.begin(), RangeE = StackRange.end();
  auto VarsIt = Assignment.Variables.begin(),
       VarsE = Assignment.Variables.end();
  for (; (RangeIt != RangeE) && (VarsIt != VarsE); ++RangeIt, ++VarsIt)
    *RangeIt = *VarsIt;
}

bool EVMOptimizedCodeTransform::areLayoutsCompatible(const Stack &SourceStack,
                                                     const Stack &TargetStack) {
  return SourceStack.size() == TargetStack.size() &&
         all_of(zip_equal(SourceStack, TargetStack), [](const auto &Pair) {
           const auto &[Src, Tgt] = Pair;
           return std::holds_alternative<JunkSlot>(Tgt) || (Src == Tgt);
         });
}

void EVMOptimizedCodeTransform::createStackLayout(const Stack &TargetStack) {
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

  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
  // ::createStackLayout asserts that it has successfully achieved the target
  // layout.
  ::createStackLayout(
      CurrentStack, TargetStack,
      // Swap callback.
      [&](unsigned I) {
        assert(static_cast<int>(CurrentStack.size()) ==
               Assembly.getStackHeight());
        assert(I > 0 && I < CurrentStack.size());
        if (I <= 16) {
          Assembly.emitSWAP(I);
        } else {
          int Deficit = static_cast<int>(I) - 16;
          const StackSlot &DeepSlot =
              CurrentStack.at(CurrentStack.size() - I - 1);
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
        assert(static_cast<int>(CurrentStack.size()) ==
               Assembly.getStackHeight());

        // Dup the slot, if already on stack and reachable.
        auto SlotIt = llvm::find(llvm::reverse(CurrentStack), Slot);
        if (SlotIt != CurrentStack.rend()) {
          unsigned Depth = std::distance(CurrentStack.rbegin(), SlotIt);
          if (Depth < 16) {
            Assembly.emitDUP(static_cast<unsigned>(Depth + 1));
            return;
          }
          if (!canBeFreelyGenerated(Slot)) {
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
                       Assembly.emitConstant(Literal.Value);
                     },
                     [&](const SymbolSlot &Symbol) {
                       Assembly.emitSymbol(Symbol.MI, Symbol.Symbol);
                     },
                     [&](const FunctionReturnLabelSlot &) {
                       llvm_unreachable("Cannot produce function return label");
                     },
                     [&](const FunctionCallReturnLabelSlot &ReturnLabel) {
                       Assembly.emitLabelReference(ReturnLabel.Call);
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
                       Assembly.emitConstant(0);
                     }},
            Slot);
      },
      // Pop callback.
      [&]() { Assembly.emitPOP(); });

  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
}

void EVMOptimizedCodeTransform::createOperationLayout(
    const CFG::Operation &Op) {
  // Create required layout for entering the Operation.
  // Check if we can choose cheaper stack shuffling if the Operation is an
  // instruction with commutable arguments.
  bool SwapCommutable = false;
  if (const auto *Inst = std::get_if<CFG::BuiltinCall>(&Op.Operation);
      Inst && Inst->Builtin->isCommutable()) {
    // Get the stack layout before the instruction.
    const Stack &DefaultTargetStack = Layout.operationEntryLayout.at(&Op);
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
    createStackLayout(Layout.operationEntryLayout.at(&Op));
  }

  // Assert that we have the inputs of the Operation on stack top.
  assert(static_cast<int>(CurrentStack.size()) == Assembly.getStackHeight());
  assert(CurrentStack.size() >= Op.Input.size());
  Stack StackInput(CurrentStack.end() - Op.Input.size(), CurrentStack.end());
  // Adjust the StackInput if needed.
  if (SwapCommutable) {
    std::swap(StackInput[StackInput.size() - 1],
              StackInput[StackInput.size() - 2]);
  }
  assert(areLayoutsCompatible(StackInput, Op.Input));
}

void EVMOptimizedCodeTransform::run(CFG::BasicBlock &EntryBB) {
  assert(CurrentStack.empty() && Assembly.getStackHeight() == 0);

  SmallPtrSet<CFG::BasicBlock *, 32> Visited;
  SmallVector<CFG::BasicBlock *, 32> WorkList{&EntryBB};
  while (!WorkList.empty()) {
    auto *Block = WorkList.pop_back_val();
    if (!Visited.insert(Block).second)
      continue;

    const auto &BlockInfo = Layout.blockInfos.at(Block);

    // Might set some slots to junk, if not required by the block.
    CurrentStack = BlockInfo.entryLayout;
    Assembly.init(Block->MBB, static_cast<int>(CurrentStack.size()));

    for (const auto &Operation : Block->Operations) {
      createOperationLayout(Operation);

      [[maybe_unused]] size_t BaseHeight =
          CurrentStack.size() - Operation.Input.size();

      // Perform the Operation.
      std::visit(
          Overload{[this](const CFG::FunctionCall &Call) { visitCall(Call); },
                   [this](const CFG::BuiltinCall &Call) { visitInst(Call); },
                   [this](const CFG::Assignment &Assignment) {
                     visitAssign(Assignment);
                   }},
          Operation.Operation);

      // Assert that the Operation produced its proclaimed output.
      assert(static_cast<int>(CurrentStack.size()) ==
             Assembly.getStackHeight());
      assert(CurrentStack.size() == BaseHeight + Operation.Output.size());
      assert(CurrentStack.size() >= Operation.Output.size());
      assert(areLayoutsCompatible(
          Stack(CurrentStack.end() - Operation.Output.size(),
                CurrentStack.end()),
          Operation.Output));
    }

    // Exit the block.
    std::visit(
        Overload{
            [&](const CFG::BasicBlock::InvalidExit &) {
              llvm_unreachable("Unexpected BB terminator");
            },
            [&](const CFG::BasicBlock::Jump &Jump) {
              // Create the stack expected at the jump target.
              createStackLayout(Layout.blockInfos.at(Jump.Target).entryLayout);

              // Assert that we have a valid stack for the target.
              assert(areLayoutsCompatible(
                  CurrentStack, Layout.blockInfos.at(Jump.Target).entryLayout));

              if (Jump.UncondJump)
                Assembly.emitUncondJump(Jump.UncondJump, Jump.Target->MBB);
              WorkList.emplace_back(Jump.Target);
            },
            [&](CFG::BasicBlock::ConditionalJump const &CondJump) {
              // Create the shared entry layout of the jump targets, which is
              // stored as exit layout of the current block.
              createStackLayout(BlockInfo.exitLayout);

              // Assert that we have the correct condition on stack.
              assert(!CurrentStack.empty());
              assert(CurrentStack.back() == CondJump.Condition);

              // Emit the conditional jump to the non-zero label and update the
              // stored stack.
              assert(CondJump.CondJump);
              Assembly.emitCondJump(CondJump.CondJump, CondJump.NonZero->MBB);
              CurrentStack.pop_back();

              // Assert that we have a valid stack for both jump targets.
              assert(areLayoutsCompatible(
                  CurrentStack,
                  Layout.blockInfos.at(CondJump.NonZero).entryLayout));
              assert(areLayoutsCompatible(
                  CurrentStack,
                  Layout.blockInfos.at(CondJump.Zero).entryLayout));

              // Generate unconditional jump if needed.
              if (CondJump.UncondJump)
                Assembly.emitUncondJump(CondJump.UncondJump,
                                        CondJump.Zero->MBB);
              WorkList.emplace_back(CondJump.NonZero);
              WorkList.emplace_back(CondJump.Zero);
            },
            [&](CFG::BasicBlock::FunctionReturn const &FuncReturn) {
              assert(!MF.getFunction().hasFnAttribute(Attribute::NoReturn));

              // Construct the function return layout, which is fully determined
              // by the function signature.
              Stack ExitStack = FuncReturn.RetValues;

              ExitStack.emplace_back(FunctionReturnLabelSlot{&MF});

              // Create the function return layout and jump.
              createStackLayout(ExitStack);
              Assembly.emitRet(FuncReturn.Ret);
            },
            [&](CFG::BasicBlock::Unreachable const &) {
              assert(Block->Operations.empty());
            },
            [&](CFG::BasicBlock::Terminated const &) {
              assert(!Block->Operations.empty());
              if (const CFG::BuiltinCall *BuiltinCall =
                      std::get_if<CFG::BuiltinCall>(
                          &Block->Operations.back().Operation))
                assert(BuiltinCall->TerminatesOrReverts);
              else if (CFG::FunctionCall const *FunctionCall =
                           std::get_if<CFG::FunctionCall>(
                               &Block->Operations.back().Operation))
                assert(!FunctionCall->CanContinue);
              else
                llvm_unreachable("Unexpected BB terminator");
            }},
        Block->Exit);
  }

  Assembly.finalize();
}
