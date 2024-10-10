//===--- EVMOptimizedCodeTransform.h - Stack layout generator ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file transforms the stack layout back into the Machine IR instructions
// in 'stackified' form using the EVMAssembly class.
//
//===----------------------------------------------------------------------===//

#include "EVMOptimizedCodeTransform.h"
#include "EVMControlFlowGraphBuilder.h"
#include "EVMHelperUtilities.h"
#include "EVMStackDebug.h"
#include "EVMStackLayoutGenerator.h"
#include "EVMStackShuffler.h"
#include "llvm/Support/ErrorHandling.h"

#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "evm-optimized-code-transform"

void EVMOptimizedCodeTransform::run(EVMAssembly &Assembly, MachineFunction &MF,
                                    const LiveIntervals &LIS,
                                    MachineLoopInfo *MLI) {
  std::unique_ptr<CFG> Cfg = ControlFlowGraphBuilder::build(MF, LIS, MLI);
  StackLayout Layout = StackLayoutGenerator::run(*Cfg);
  EVMOptimizedCodeTransform optimizedCodeTransform(Assembly, *Cfg, Layout, MF);
  optimizedCodeTransform();
}

void EVMOptimizedCodeTransform::operator()(CFG::FunctionCall const &Call) {
  // Validate stack.
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
  assert(CurrentStack.size() >= Call.NumArguments + (Call.CanContinue ? 1 : 0));

  // Assert that we got the correct return label on stack.
  if (Call.CanContinue) {
    auto const *returnLabelSlot = std::get_if<FunctionCallReturnLabelSlot>(
        &CurrentStack.at(CurrentStack.size() - Call.NumArguments - 1));
    assert(returnLabelSlot && returnLabelSlot->Call == Call.Call);
  }

  // Emit code.
  const MachineOperand *CalleeOp = Call.Call->explicit_uses().begin();
  assert(CalleeOp->isGlobal());
  Assembly.appendFuncCall(Call.Call, CalleeOp->getGlobal(),
                          Call.Call->getNumExplicitDefs() - Call.NumArguments -
                              (Call.CanContinue ? 1 : 0),
                          Call.CanContinue ? CallToReturnMCSymbol.at(Call.Call)
                                           : nullptr);
  if (Call.CanContinue)
    Assembly.appendLabel();

  // Update stack.
  // Remove arguments and return label from CurrentStack.
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

void EVMOptimizedCodeTransform::operator()(CFG::BuiltinCall const &Call) {
  size_t NumArgs = Call.Builtin->getNumExplicitOperands() -
                   Call.Builtin->getNumExplicitDefs();
  // Validate stack.
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
  assert(CurrentStack.size() >= NumArgs);
  // TODO: assert that we got a correct stack for the call.

  // Emit code.
  Assembly.appendInstruction(Call.Builtin);

  // Update stack.
  // Remove arguments from CurrentStack.
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

void EVMOptimizedCodeTransform::operator()(CFG::Assignment const &Assignment) {
  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));

  // Invalidate occurrences of the assigned variables.
  for (auto &CurrentSlot : CurrentStack)
    if (VariableSlot const *VarSlot = std::get_if<VariableSlot>(&CurrentSlot))
      if (EVMUtils::contains(Assignment.Variables, *VarSlot))
        CurrentSlot = JunkSlot{};

  // Assign variables to current stack top.
  assert(CurrentStack.size() >= Assignment.Variables.size());
  auto StackRange =
      EVMUtils::take_last(CurrentStack, Assignment.Variables.size());
  auto RangeIt = StackRange.begin(), RangeE = StackRange.end();
  auto VarsIt = Assignment.Variables.begin(),
       VarsE = Assignment.Variables.end();
  for (; (RangeIt != RangeE) && (VarsIt != VarsE); ++RangeIt, ++VarsIt)
    *RangeIt = *VarsIt;
}

EVMOptimizedCodeTransform::EVMOptimizedCodeTransform(EVMAssembly &Assembly,
                                                     CFG const &Cfg,
                                                     StackLayout const &Layout,
                                                     MachineFunction &MF)
    : Assembly(Assembly), Layout(Layout), FuncInfo(&Cfg.FuncInfo), MF(MF) {}

void EVMOptimizedCodeTransform::assertLayoutCompatibility(
    Stack const &SourceStack, Stack const &TargetStack) {
  assert(SourceStack.size() == TargetStack.size());
  for (unsigned Idx = 0; Idx < SourceStack.size(); ++Idx)
    assert(std::holds_alternative<JunkSlot>(TargetStack[Idx]) ||
           SourceStack[Idx] == TargetStack[Idx]);
}

void EVMOptimizedCodeTransform::createStackLayout(Stack TargetStack) {
  auto SlotVariableName = [](StackSlot const &Slot) {
    return std::visit(
        Overload{
            [&](VariableSlot const &Var) {
              SmallString<1024> StrBuf;
              llvm::raw_svector_ostream OS(StrBuf);
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
          Assembly.appendSWAPInstruction(I);
        } else {
          int Deficit = static_cast<int>(I) - 16;
          StackSlot const &DeepSlot =
              CurrentStack.at(CurrentStack.size() - I - 1);
          std::string VarNameDeep = SlotVariableName(DeepSlot);
          std::string VarNameTop = SlotVariableName(CurrentStack.back());
          std::string Msg =
              (Twine("Cannot swap ") +
               (VarNameDeep.empty() ? ("Slot " + stackSlotToString(DeepSlot))
                                    : (Twine("Variable ") + VarNameDeep)) +
               " with " +
               (VarNameTop.empty()
                    ? ("Slot " + stackSlotToString(CurrentStack.back()))
                    : (Twine("Variable ") + VarNameTop)) +
               ": too deep in the stack by " + std::to_string(Deficit) +
               " slots in " + stackToString(CurrentStack))
                  .str();

          report_fatal_error(FuncInfo->MF->getName() + Twine(": ") + Msg);
        }
      },
      // Push or dup callback.
      [&](StackSlot const &Slot) {
        assert(static_cast<int>(CurrentStack.size()) ==
               Assembly.getStackHeight());
        // Dup the slot, if already on stack and reachable.
        if (auto Depth = EVMUtils::findOffset(
                EVMUtils::get_reverse(CurrentStack), Slot)) {
          if (*Depth < 16) {
            Assembly.appendDUPInstruction(static_cast<unsigned>(*Depth + 1));
            return;
          } else if (!canBeFreelyGenerated(Slot)) {
            std::string VarName = SlotVariableName(Slot);
            Twine Msg =
                ((VarName.empty() ? "Slot " + stackSlotToString(Slot)
                                  : Twine("Variable ") + VarName) +
                 " is " + std::to_string(*Depth - 15) +
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
            Overload{[&](LiteralSlot const &Literal) {
                       Assembly.appendConstant(Literal.Value);
                     },
                     [&](SymbolSlot const &Symbol) {
                       Assembly.appendSymbol(Symbol.Symbol);
                     },
                     [&](FunctionReturnLabelSlot const &) {
                       llvm_unreachable("Cannot produce function return label");
                     },
                     [&](const FunctionCallReturnLabelSlot &ReturnLabel) {
                       if (!CallToReturnMCSymbol.count(ReturnLabel.Call))
                         CallToReturnMCSymbol[ReturnLabel.Call] =
                             Assembly.createFuncRetSymbol();

                       Assembly.appendLabelReference(
                           CallToReturnMCSymbol[ReturnLabel.Call]);
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
                       Assembly.appendConstant(0);
                     }},
            Slot);
      },
      // Pop callback.
      [&]() { Assembly.appendPOPInstruction(); });

  assert(Assembly.getStackHeight() == static_cast<int>(CurrentStack.size()));
}

void EVMOptimizedCodeTransform::operator()(CFG::BasicBlock const &Block) {
  // Current location for the entry BB was set up in operator()().
  if (&Block != FuncInfo->Entry)
    Assembly.setCurrentLocation(Block.MBB);

  // Assert that this is the first visit of the block and mark as generated.
  auto It = GeneratedBlocks.insert(&Block);
  assert(It.second);

  auto const &BlockInfo = Layout.blockInfos.at(&Block);

  // Assert that the stack is valid for entering the block.
  assertLayoutCompatibility(CurrentStack, BlockInfo.entryLayout);

  // Might set some slots to junk, if not required by the block.
  CurrentStack = BlockInfo.entryLayout;
  assert(static_cast<int>(CurrentStack.size()) == Assembly.getStackHeight());

  // Emit jumpdest, if required.
  if (EVMUtils::valueOrNullptr(BlockLabels, &Block))
    Assembly.appendLabel();

  for (auto const &Operation : Block.Operations) {
    // Create required layout for entering the Operation.
    createStackLayout(Layout.operationEntryLayout.at(&Operation));

    // Assert that we have the inputs of the Operation on stack top.
    assert(static_cast<int>(CurrentStack.size()) == Assembly.getStackHeight());
    assert(CurrentStack.size() >= Operation.Input.size());
    size_t BaseHeight = CurrentStack.size() - Operation.Input.size();
    assertLayoutCompatibility(EVMUtils::to_vector(EVMUtils::take_last(
                                  CurrentStack, Operation.Input.size())),
                              Operation.Input);

    // Perform the Operation.
    std::visit(*this, Operation.Operation);

    // Assert that the Operation produced its proclaimed output.
    assert(static_cast<int>(CurrentStack.size()) == Assembly.getStackHeight());
    assert(CurrentStack.size() == BaseHeight + Operation.Output.size());
    assert(CurrentStack.size() >= Operation.Output.size());
    assertLayoutCompatibility(EVMUtils::to_vector(EVMUtils::take_last(
                                  CurrentStack, Operation.Output.size())),
                              Operation.Output);
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

            // If this is the only jump to the block which is a fallthrought
            // we can directly continue with the target block.
            if (Jump.Target->Entries.size() == 1 && Jump.FallThrough)
              assert(!Jump.Backwards && !BlockLabels.count(Jump.Target));

            if (!BlockLabels.count(Jump.Target))
              BlockLabels[Jump.Target] = Jump.Target->MBB->getSymbol();

            if (Jump.UncondJump)
              Assembly.appendUncondJump(Jump.UncondJump, Jump.Target->MBB);

            if (!GeneratedBlocks.count(Jump.Target))
              (*this)(*Jump.Target);
          },
          [&](CFG::BasicBlock::ConditionalJump const &CondJump) {
            // Create the shared entry layout of the jump targets, which is
            // stored as exit layout of the current block.
            createStackLayout(BlockInfo.exitLayout);

            // Create labels for the targets, if not already present.
            if (!BlockLabels.count(CondJump.NonZero))
              BlockLabels[CondJump.NonZero] =
                  CondJump.NonZero->MBB->getSymbol();

            if (!BlockLabels.count(CondJump.Zero))
              BlockLabels[CondJump.Zero] = CondJump.Zero->MBB->getSymbol();

            // Assert that we have the correct condition on stack.
            assert(!CurrentStack.empty());
            assert(CurrentStack.back() == CondJump.Condition);

            // Emit the conditional jump to the non-zero label and update the
            // stored stack.
            assert(CondJump.CondJump);
            Assembly.appendCondJump(CondJump.CondJump, CondJump.NonZero->MBB);
            CurrentStack.pop_back();

            // Assert that we have a valid stack for both jump targets.
            assertLayoutCompatibility(
                CurrentStack,
                Layout.blockInfos.at(CondJump.NonZero).entryLayout);
            assertLayoutCompatibility(
                CurrentStack, Layout.blockInfos.at(CondJump.Zero).entryLayout);

            {
              // Restore the stack afterwards for the non-zero case below.
              EVMUtils::ScopeGuard stackRestore([storedStack = CurrentStack,
                                                 this]() {
                CurrentStack = std::move(storedStack);
                Assembly.setStackHeight(static_cast<int>(CurrentStack.size()));
              });

              // If we have already generated the zero case, jump to it,
              // otherwise generate it in place.
              if (CondJump.UncondJump)
                Assembly.appendUncondJump(CondJump.UncondJump,
                                          CondJump.Zero->MBB);

              if (!GeneratedBlocks.count(CondJump.Zero))
                (*this)(*CondJump.Zero);
            }
            // Note that each block visit terminates control flow, so we cannot
            // fall through from the zero case.

            // Generate the non-zero block, if not done already.
            if (!GeneratedBlocks.count(CondJump.NonZero))
              (*this)(*CondJump.NonZero);
          },
          [&](CFG::BasicBlock::FunctionReturn const &FuncReturn) {
            assert(FuncInfo->CanContinue);

            // Construct the function return layout, which is fully determined
            // by the function signature.
            Stack ExitStack = FuncReturn.RetValues;

            ExitStack.emplace_back(
                FunctionReturnLabelSlot{FuncReturn.Info->MF});

            // Create the function return layout and jump.
            createStackLayout(ExitStack);
            Assembly.appendJump(0);
          },
          [&](CFG::BasicBlock::Terminated const &) {
            assert(!Block.Operations.empty());
            if (const CFG::BuiltinCall *BuiltinCall =
                    std::get_if<CFG::BuiltinCall>(
                        &Block.Operations.back().Operation))
              assert(BuiltinCall->TerminatesOrReverts);
            else if (CFG::FunctionCall const *FunctionCall =
                         std::get_if<CFG::FunctionCall>(
                             &Block.Operations.back().Operation))
              assert(!FunctionCall->CanContinue);
            else
              llvm_unreachable("Unexpected BB terminator");
          }},
      Block.Exit);

  // TODO: We could assert that the last emitted assembly item terminated or was
  //       an (unconditional) jump.
  CurrentStack.clear();
  Assembly.setStackHeight(0);
}

void EVMOptimizedCodeTransform::operator()() {
  assert(CurrentStack.empty() && Assembly.getStackHeight() == 0);
  Assembly.setCurrentLocation(FuncInfo->Entry->MBB);

  assert(!BlockLabels.count(FuncInfo->Entry));

  // Create function entry layout in CurrentStack.
  if (FuncInfo->CanContinue)
    CurrentStack.emplace_back(FunctionReturnLabelSlot{FuncInfo->MF});

  for (auto const &Param : reverse(FuncInfo->Parameters))
    CurrentStack.emplace_back(Param);

  Assembly.setStackHeight(static_cast<int>(CurrentStack.size()));
  Assembly.appendLabel();

  // Create the entry layout of the function body block and visit.
  createStackLayout(Layout.blockInfos.at(FuncInfo->Entry).entryLayout);

  (*this)(*FuncInfo->Entry);

  Assembly.finalize();
}
