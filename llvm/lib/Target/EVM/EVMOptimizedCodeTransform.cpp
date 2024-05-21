#include "EVMOptimizedCodeTransform.h"
#include "EVMControlFlowGraphBuilder.h"
#include "EVMHelperUtilities.h"
#include "EVMStackHelpers.h"
#include "EVMStackLayoutGenerator.h"
#include "llvm/Support/ErrorHandling.h"

#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "evm-optimized-code-transform"

void EVMOptimizedCodeTransform::run(EVMAssembly &_assembly, MachineFunction &MF,
                                    const LiveIntervals &LIS,
                                    MachineLoopInfo *MLI) {

  SmallString<1024> StringBuf;
  llvm::raw_svector_ostream OStream(StringBuf);
  ControlFlowGraphPrinter CfgPrinter(OStream);
  // LLVM_DEBUG(dbgs() << StringBuf << '\n');
  // StackLayout stackLayout = StackLayoutGenerator::run(*Cfg);
  // std::string Out = StackLayoutGenerator::Test(&MF.front());

  OStream << "*** CFG ***\n";
  std::unique_ptr<CFG> Cfg = ControlFlowGraphBuilder::build(MF, LIS, MLI);
  CfgPrinter(*Cfg);
  OStream << "*** Stack layout ***\n";
  StackLayout stackLayout = StackLayoutGenerator::run(*Cfg);
  StackLayoutPrinter LayoutPrinter{OStream, stackLayout};
  LayoutPrinter(Cfg->FuncInfo);
  LLVM_DEBUG(dbgs() << StringBuf << "\n");
  EVMOptimizedCodeTransform optimizedCodeTransform(_assembly, *Cfg, stackLayout,
                                                   MF);
  optimizedCodeTransform();
  return;
}

void EVMOptimizedCodeTransform::operator()(CFG::FunctionCall const &_call) {
  // Validate stack.
  {
    assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));
    assert(m_stack.size() >= _call.NumArguments + (_call.CanContinue ? 1 : 0));

    /*
    // Assert that we got the correct arguments on stack for the call.
    for (auto &&[arg, slot] : ranges::zip_view(
             _call.functionCall.get().arguments | ranges::views::reverse,
             m_stack | ranges::views::take_last(
                           _call.functionCall.get().arguments.size())))
      validateSlot(slot, arg);
    */

    // Assert that we got the correct return label on stack.
    if (_call.CanContinue) {
      auto const *returnLabelSlot = std::get_if<FunctionCallReturnLabelSlot>(
          &m_stack.at(m_stack.size() - _call.NumArguments - 1));

      assert(returnLabelSlot && returnLabelSlot->Call == _call.Call);
    }
  }

  // Emit code.
  {
    /*
    m_assembly.appendJumpTo(
        getFunctionLabel(_call.function),
        static_cast<int>(_call.function.get().returns.size() -
                         _call.function.get().arguments.size()) -
            (_call.canContinue ? 1 : 0),
        AbstractAssembly::JumpType::IntoFunction);
    */
    const MachineOperand *CalleeOp = _call.Call->explicit_uses().begin();
    assert(CalleeOp->isGlobal());
    m_assembly.appendFuncCall(
        _call.Call, CalleeOp->getGlobal(),
        _call.Call->getNumExplicitDefs() - _call.NumArguments -
            (_call.CanContinue ? 1 : 0),
        _call.CanContinue ? CallToReturnMCSymbol.at(_call.Call) : nullptr);
    if (_call.CanContinue)
      m_assembly.appendLabel();
  }

  // Update stack.
  {
    // Remove arguments and return label from m_stack.
    for (size_t i = 0; i < _call.NumArguments + (_call.CanContinue ? 1 : 0);
         ++i)
      m_stack.pop_back();

    // Push return values to m_stack.
    unsigned Idx = 0;
    for (const auto &MO : _call.Call->defs()) {
      assert(MO.isReg());
      m_stack.emplace_back(TemporarySlot{_call.Call, MO.getReg(), Idx++});
    }
    /*
    for (size_t index : iota17(0u, _call.Call->getNumExplicitDefs()))
      m_stack.emplace_back(TemporarySlot{_call.Call, index});
    */
    assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));
  }
}

void EVMOptimizedCodeTransform::operator()(CFG::BuiltinCall const &_call) {
  // Validate stack.
  {
    assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));
    assert(m_stack.size() >= _call.Builtin->getNumExplicitDefs());
    /*
    // Assert that we got a correct stack for the call.
    for (auto &&[arg, slot] : ranges::zip_view(
             _call.functionCall.get().arguments | ranges::views::enumerate |
                 ranges::views::filter(
                     util::mapTuple([&](size_t idx, auto &) -> bool {
                       return !_call.builtin.get().literalArgument(idx);
                     })) |
                 ranges::views::reverse | ranges::views::values,
             m_stack | ranges::views::take_last(_call.arguments)))
      validateSlot(slot, arg);
      */
  }

  // Emit code.
  /*
  {
    m_assembly.setSourceLocation(originLocationOf(_call));
    static_cast<BuiltinFunctionForEVM const &>(_call.builtin.get())
        .generateCode(_call.functionCall, m_assembly, m_builtinContext);
  }
  */
  m_assembly.appendInstruction(_call.Builtin);

  // Update stack.
  {
    // Remove arguments from m_stack.
    size_t NumArgs = _call.Builtin->getNumExplicitOperands() -
                     _call.Builtin->getNumExplicitDefs();
    for (size_t i = 0; i < NumArgs; ++i)
      m_stack.pop_back();

    // Push return values to m_stack.
    unsigned Idx = 0;
    for (const auto &MO : _call.Builtin->defs()) {
      assert(MO.isReg());
      m_stack.emplace_back(TemporarySlot{_call.Builtin, MO.getReg(), Idx++});
    }

    assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));
  }
}

void EVMOptimizedCodeTransform::operator()(CFG::Assignment const &_assignment) {
  assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));

  // Invalidate occurrences of the assigned variables.
  for (auto &currentSlot : m_stack)
    if (VariableSlot const *varSlot = std::get_if<VariableSlot>(&currentSlot))
      if (EVMUtils::contains(_assignment.Variables, *varSlot))
        currentSlot = JunkSlot{};

  // Assign variables to current stack top.
  assert(m_stack.size() >= _assignment.Variables.size());

  /*
  for (auto &&[currentSlot, varSlot] : ranges::zip_view(
           m_stack | ranges::views::take_last(_assignment.variables.size()),
           _assignment.variables))
    currentSlot = varSlot;
  */
  auto StackRange = EVMUtils::take_last(m_stack, _assignment.Variables.size());
  auto RangeIt = StackRange.begin(), RangeE = StackRange.end();
  auto VarsIt = _assignment.Variables.begin(),
       VarsE = _assignment.Variables.end();
  for (; (RangeIt != RangeE) && (VarsIt != VarsE); ++RangeIt, ++VarsIt)
    *RangeIt = *VarsIt;
}

EVMOptimizedCodeTransform::EVMOptimizedCodeTransform(
    EVMAssembly &_assembly, CFG const &_cfg, StackLayout const &_stackLayout,
    MachineFunction &MF)
    : m_assembly(_assembly), m_stackLayout(_stackLayout),
      m_funcInfo(&_cfg.FuncInfo), MF(MF) {}

void EVMOptimizedCodeTransform::assertLayoutCompatibility(
    Stack const &_currentStack, Stack const &_desiredStack) {
  assert(_currentStack.size() == _desiredStack.size());
  for (unsigned Idx = 0; Idx < _currentStack.size(); ++Idx)
    assert(std::holds_alternative<JunkSlot>(_desiredStack[Idx]) ||
           _currentStack[Idx] == _desiredStack[Idx]);
}
/*
void EVMOptimizedCodeTransform::validateSlot(StackSlot const &_slot,
                                             Expression const &_expression) {
  std::visit(
      util::GenericVisitor{
          [&](yul::Literal const &_literal) {
            auto *literalSlot = std::get_if<LiteralSlot>(&_slot);
            yulAssert(literalSlot &&
                          valueOfLiteral(_literal) == literalSlot->value,
                      "");
          },
          [&](yul::Identifier const &_identifier) {
            auto *variableSlot = std::get_if<VariableSlot>(&_slot);
            yulAsse*rt(variableSlot &&
                          variableSlot->variable.get().name == _identifier.name,
                      "");
          },
          [&](yul::FunctionCall const &_call) {
            auto *temporarySlot = std::get_if<TemporarySlot>(&_slot);
            yulAssert(temporarySlot && &temporarySlot->call.get() == &_call &&
                          temporarySlot->index == 0,
                      "");
          }},
      _expression);
}
*/

void EVMOptimizedCodeTransform::createStackLayout(Stack _targetStack) {
  auto slotVariableName = [](StackSlot const &_slot) {
    return std::visit(
        Overload{
            [&](VariableSlot const &_var) {
              SmallString<1024> StrBuf;
              llvm::raw_svector_ostream OS(StrBuf);
              OS << printReg(_var.VirtualReg, nullptr, 0, nullptr);
              return std::string(StrBuf.c_str());
            },
            [&](const FunctionCallReturnLabelSlot &) { return std::string(); },
            [&](const FunctionReturnLabelSlot &) { return std::string(); },
            [&](const LiteralSlot &) { return std::string(); },
            [&](const TemporarySlot &) { return std::string(); },
            [&](const JunkSlot &) { return std::string(); }},
        _slot);
  };

  assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));
  // ::createStackLayout asserts that it has successfully achieved the target
  // layout.
  ::createStackLayout(
      m_stack, _targetStack,
      // Swap callback.
      [&](unsigned _i) {
        assert(static_cast<int>(m_stack.size()) == m_assembly.getStackHeight());
        assert(_i > 0 && _i < m_stack.size());
        if (_i <= 16)
          m_assembly.appendSWAPInstruction(_i);
        else {
          int deficit = static_cast<int>(_i) - 16;
          StackSlot const &deepSlot = m_stack.at(m_stack.size() - _i - 1);
          std::string varNameDeep = slotVariableName(deepSlot);
          std::string varNameTop = slotVariableName(m_stack.back());
          Twine msg = Twine("Cannot swap ") +
                      (varNameDeep.empty()
                           ? ("Slot " + stackSlotToString(deepSlot))
                           : (Twine("Variable ") + varNameDeep.c_str())) +
                      " with " +
                      (varNameTop.empty()
                           ? ("Slot " + stackSlotToString(m_stack.back()))
                           : (Twine("Variable ") + varNameTop.c_str())) +
                      ": too deep in the stack by " + std::to_string(deficit) +
                      " slots in " + stackToString(m_stack);

          report_fatal_error(m_funcInfo->MF->getName() + Twine(": ") + msg);
        }
      },
      // Push or dup callback.
      [&](StackSlot const &_slot) {
        assert(static_cast<int>(m_stack.size()) == m_assembly.getStackHeight());
        // Dup the slot, if already on stack and reachable.
        if (auto depth =
                EVMUtils::findOffset(EVMUtils::get_reverse(m_stack), _slot)) {
          if (*depth < 16) {
            m_assembly.appendDUPInstruction(static_cast<unsigned>(*depth + 1));
            return;
          } else if (!canBeFreelyGenerated(_slot)) {
            std::string varName = slotVariableName(_slot);
            Twine msg =
                (varName.empty() ? "Slot " + stackSlotToString(_slot)
                                 : Twine("Variable ") + varName.c_str()) +
                " is " + std::to_string(*depth - 15) +
                " too deep in the stack " + stackToString(m_stack);

            report_fatal_error(MF.getName() + ": " + msg);
            return;
          }
          // else: the slot is too deep in stack, but can be freely generated,
          // we fall through to push it again.
        }

        // The slot can be freely generated or is an unassigned return variable.
        // Push it.
        std::visit(
            Overload{[&](LiteralSlot const &_literal) {
                       m_assembly.appendConstant(_literal.Value);
                     },
                     [&](FunctionReturnLabelSlot const &) {
                       llvm_unreachable("Cannot produce function return label");
                     },
                     [&](const FunctionCallReturnLabelSlot &ReturnLabel) {
                       /*
                         if (!m_returnLabels.count(&_returnLabel.call.get()))
                           m_returnLabels[&_returnLabel.call.get()] =
                               m_assembly.newLabelId();
                         m_assembly.appendLabelReference(
                             m_returnLabels.at(&_returnLabel.call.get()));
                       */
                       if (!CallToReturnMCSymbol.count(ReturnLabel.Call))
                         CallToReturnMCSymbol[ReturnLabel.Call] =
                             m_assembly.createFuncRetSymbol();
                       m_assembly.appendLabelReference(
                           CallToReturnMCSymbol[ReturnLabel.Call]);
                     },
                     [&](const VariableSlot &Variable) {
                       llvm_unreachable("Check! Duplication of VaribaleSlot");
                       bool IsRetVar = false;
                       for (CFG::BasicBlock *exit : m_funcInfo->Exits) {
                         const Stack &RetValues =
                             std::get<CFG::BasicBlock::FunctionReturn>(
                                 exit->Exit)
                                 .RetValues;
                         // TODO:: replace RetValus with vector<VariableSlot>
                         for (const StackSlot &Val : RetValues) {
                           if (const VariableSlot *VarSlot =
                                   std::get_if<VariableSlot>(&Val)) {
                             if (*VarSlot == std::get<VariableSlot>(_slot)) {
                               IsRetVar = true;
                               break;
                             } else {
                               llvm_unreachable(
                                   "Non VariableSlot among return values");
                             }
                           }
                         }
                         if (IsRetVar)
                           break;
                       }
                       if (IsRetVar) {
                         m_assembly.appendConstant(0);
                         return;
                       }
                       llvm_unreachable("Variable not found on stack");
                       /*
                       if (m_funcInfo &&
                           util::contains(m_funcInfo->returnVariables,
                                          _variable)) {
                         m_assembly.setSourceLocation(originLocationOf(_variable));
                         m_assembly.appendConstant(0);
                         m_assembly.setSourceLocation(sourceLocation);
                         return;
                       }
                       yulAssert(false, "Variable not found on stack.");
                       */
                     },
                     [&](const TemporarySlot &) {
                       llvm_unreachable("Function call result requested, but "
                                        "not found on stack.");
                     },
                     [&](const JunkSlot &) {
                       // Note: this will always be popped, so we can push
                       // anything.
                       m_assembly.appendConstant(0);
                       // m_assembly.appendInstruction(evmasm::Instruction::CODESIZE);
                     }},
            _slot);
      },
      // Pop callback.
      [&]() { m_assembly.appendPOPInstruction(); });

  assert(m_assembly.getStackHeight() == static_cast<int>(m_stack.size()));
}

void EVMOptimizedCodeTransform::operator()(CFG::BasicBlock const &_block) {
  // Assert that this is the first visit of the block and mark as generated.
  auto It = m_generated.insert(&_block);
  assert(It.second);

  auto const &blockInfo = m_stackLayout.blockInfos.at(&_block);

  // Assert that the stack is valid for entering the block.
  assertLayoutCompatibility(m_stack, blockInfo.entryLayout);

  // Might set some slots to junk, if not required by the block.
  m_stack = blockInfo.entryLayout;
  assert(static_cast<int>(m_stack.size()) == m_assembly.getStackHeight());

  // Emit jumpdest, if required.
  if (EVMUtils::valueOrNullptr(m_blockLabels, &_block))
    m_assembly.appendLabel();

  for (auto const &operation : _block.Operations) {
    // Create required layout for entering the operation.
    createStackLayout(m_stackLayout.operationEntryLayout.at(&operation));

    // Assert that we have the inputs of the operation on stack top.
    assert(static_cast<int>(m_stack.size()) == m_assembly.getStackHeight());
    assert(m_stack.size() >= operation.Input.size());
    size_t baseHeight = m_stack.size() - operation.Input.size();
    assertLayoutCompatibility(EVMUtils::to_vector(EVMUtils::take_last(
                                  m_stack, operation.Input.size())),
                              operation.Input);

    // Perform the operation.
    std::visit(*this, operation.Operation);

    // Assert that the operation produced its proclaimed output.
    assert(static_cast<int>(m_stack.size()) == m_assembly.getStackHeight());
    assert(m_stack.size() == baseHeight + operation.Output.size());
    assert(m_stack.size() >= operation.Output.size());
    assertLayoutCompatibility(EVMUtils::to_vector(EVMUtils::take_last(
                                  m_stack, operation.Output.size())),
                              operation.Output);
  }

  // Exit the block.
  std::visit(
      Overload{
          [&](const CFG::BasicBlock::InvalidExit &) {
            llvm_unreachable("Unexpected BB terminator");
          },
          [&](const CFG::BasicBlock::Jump &_jump) {
            // Create the stack expected at the jump target.
            createStackLayout(
                m_stackLayout.blockInfos.at(_jump.Target).entryLayout);
            /*
            // If this is the only jump to the block, we do not need a label and
            // can directly continue with the target block.
            if (!m_blockLabels.count(_jump.target) &&
                _jump.target->entries.size() == 1) {
              yulAssert(!_jump.backwards, "");
              (*this)(*_jump.target);
            } else {
              // Generate a jump label for the target, if not already present.
              if (!m_blockLabels.count(_jump.target))
                m_blockLabels[_jump.target] = m_assembly.newLabelId();

              // If we already have generated the target block, jump to it,
              // otherwise generate it in place.
              if (m_generated.count(_jump.target))
                m_assembly.appendJumpTo(m_blockLabels[_jump.target]);
              else
                (*this)(*_jump.target);
            }
            */

            // If this is the only jump to the block which is a fallthrought
            // we can directly continue with the target block.
            if (_jump.Target->Entries.size() == 1 && _jump.FallThrough) {
              assert(!_jump.Backwards && !m_blockLabels.count(_jump.Target));
            } else {
              // Generate a jumpdest for the target, if not already present.
              if (!m_blockLabels.count(_jump.Target))
                m_blockLabels[_jump.Target] = _jump.Target->MBB->getSymbol();
            }
            (*this)(*_jump.Target);
          },
          [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
            // Create the shared entry layout of the jump targets, which is
            // stored as exit layout of the current block.
            createStackLayout(blockInfo.exitLayout);

            // Create labels for the targets, if not already present.
            if (!m_blockLabels.count(_conditionalJump.NonZero))
              m_blockLabels[_conditionalJump.NonZero] =
                  _conditionalJump.NonZero->MBB->getSymbol();
            if (!m_blockLabels.count(_conditionalJump.Zero))
              m_blockLabels[_conditionalJump.Zero] =
                  _conditionalJump.Zero->MBB->getSymbol();

            // Assert that we have the correct condition on stack.
            assert(!m_stack.empty());
            assert(m_stack.back() == _conditionalJump.Condition);

            // Emit the conditional jump to the non-zero label and update the
            // stored stack.
            // m_assembly.appendJumpToIf(m_blockLabels[_conditionalJump.nonZero]);
            m_stack.pop_back();

            // Assert that we have a valid stack for both jump targets.
            assertLayoutCompatibility(
                m_stack, m_stackLayout.blockInfos.at(_conditionalJump.NonZero)
                             .entryLayout);
            assertLayoutCompatibility(
                m_stack,
                m_stackLayout.blockInfos.at(_conditionalJump.Zero).entryLayout);

            {
              // Restore the stack afterwards for the non-zero case below.
              EVMUtils::ScopeGuard stackRestore(
                  [storedStack = m_stack, this]() {
                    m_stack = std::move(storedStack);
                    m_assembly.setStackHeight(static_cast<int>(m_stack.size()));
                  });

              // If we have already generated the zero case, jump to it,
              // otherwise generate it in place.
              if (!m_generated.count(_conditionalJump.Zero))
                (*this)(*_conditionalJump.Zero);
            }
            // Note that each block visit terminates control flow, so we cannot
            // fall through from the zero case.

            // Generate the non-zero block, if not done already.
            if (!m_generated.count(_conditionalJump.NonZero))
              (*this)(*_conditionalJump.NonZero);
          },
          [&](CFG::BasicBlock::FunctionReturn const &_functionReturn) {
            assert(m_funcInfo);
            // assert(m_funcInfo == _functionReturn.info);
            assert(m_funcInfo->CanContinue);

            // Construct the function return layout, which is fully determined
            // by the function signature.
            Stack exitStack = _functionReturn.RetValues;

            exitStack.emplace_back(
                FunctionReturnLabelSlot{_functionReturn.Info->MF});

            // Create the function return layout and jump.
            createStackLayout(exitStack);
            m_assembly.appendJump(0);
          },
          [&](CFG::BasicBlock::Terminated const &) {
            assert(!_block.Operations.empty());
            if (const CFG::BuiltinCall *builtinCall =
                    std::get_if<CFG::BuiltinCall>(
                        &_block.Operations.back().Operation))
              assert(builtinCall->TerminatesOrReverts);
            else if (CFG::FunctionCall const *functionCall =
                         std::get_if<CFG::FunctionCall>(
                             &_block.Operations.back().Operation))
              assert(!functionCall->CanContinue);
            else
              llvm_unreachable("Unexpected BB terminator");
          }},
      _block.Exit);

  // TODO: We could assert that the last emitted assembly item terminated or was
  // an (unconditional) jump.
  //       But currently AbstractAssembly does not allow peeking at the last
  //       emitted assembly item.
  m_stack.clear();
  m_assembly.setStackHeight(0);
}

void EVMOptimizedCodeTransform::operator()() {
  assert(m_stack.empty() && m_assembly.getStackHeight() == 0);
  m_assembly.setCurrentLocation(m_funcInfo->Entry->MBB);

  // Create function entry layout in m_stack.
  if (m_funcInfo->CanContinue)
    m_stack.emplace_back(FunctionReturnLabelSlot{m_funcInfo->MF});

  for (auto const &param : m_funcInfo->Parameters)
    m_stack.emplace_back(param);

  m_assembly.setStackHeight(static_cast<int>(m_stack.size()));

  m_assembly.appendLabel();

  // Create the entry layout of the function body block and visit.
  createStackLayout(m_stackLayout.blockInfos.at(m_funcInfo->Entry).entryLayout);

  (*this)(*m_funcInfo->Entry);

  m_assembly.removeUnusedInstrs();
}
