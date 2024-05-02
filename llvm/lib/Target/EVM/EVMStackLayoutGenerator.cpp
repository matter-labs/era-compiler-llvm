#include "EVMStackLayoutGenerator.h"
#include "EVMHelperUtilities.h"
#include "EVMRegisterInfo.h"
#include "EVMStackHelpers.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <sstream>

using namespace llvm;

#define DEBUG_TYPE "evm-stack-layout-gen"

StackLayoutPrinter::StackLayoutPrinter(raw_ostream &OS,
                                       const StackLayout &Layout)
    : OS(OS), Layout(Layout) {}

void StackLayoutPrinter::operator()(CFG::BasicBlock const &_block,
                                    bool _isMainEntry) {
  if (_isMainEntry) {
    OS << "Entry [label=\"Entry\"];\n";
    OS << "Entry -> Block" << getBlockId(_block) << ";\n";
  }
  while (!BlocksToPrint.empty()) {
    CFG::BasicBlock const *block = *BlocksToPrint.begin();
    BlocksToPrint.erase(BlocksToPrint.begin());
    printBlock(*block);
  }
}

void StackLayoutPrinter::operator()(CFG::FunctionInfo const &_info) {
  OS << "FunctionEntry_" << _info.MF->getName() << " [label=\"";
  OS << "function " << _info.MF->getName() << "(";
  for (const auto &Param : _info.Parameters)
    OS << printReg(Param.VirtualReg, nullptr, 0, nullptr);
  OS << ")";
  OS << "\\l\\\n";
  // Stack functionEntryStack = {FunctionReturnLabelSlot{_info.function}};
  // functionEntryStack += _info.parameters | ranges::views::reverse;
  //  OS << stackToString(functionEntryStack) << "\"];\n";
  OS << "FunctionEntry_" << _info.MF->getName() << " -> Block"
     << getBlockId(*_info.Entry) << ";\n";
  (*this)(*_info.Entry, false);
}

void StackLayoutPrinter::printBlock(CFG::BasicBlock const &_block) {
  OS << "Block" << getBlockId(_block) << " [label=\"\\\n";
  // Verify that the entries of this block exit into this block.
  for (auto const &entry : _block.Entries) {
    std::visit(
        Overload{[&](CFG::BasicBlock::Jump const &_jump) {
                   assert(_jump.Target == &_block);
                 },
                 [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
                   assert(_conditionalJump.Zero == &_block ||
                          _conditionalJump.NonZero == &_block);
                 },
                 [&](auto const &) {
                   llvm_unreachable("Invalid control flow graph");
                 }},
        entry->Exit);
  }

  auto const &blockInfo = Layout.blockInfos.at(&_block);
  OS << stackToString(blockInfo.entryLayout) << "\\l\\\n";
  for (auto const &operation : _block.Operations) {
    OS << "\n";
    auto entryLayout = Layout.operationEntryLayout.at(&operation);
    OS << stackToString(Layout.operationEntryLayout.at(&operation))
       << "\\l\\\n";
    std::visit(Overload{[&](CFG::FunctionCall const &_call) {
                          const MachineOperand *Callee =
                              _call.Call->explicit_uses().begin();
                          OS << Callee->getGlobal()->getName();
                        },
                        [&](CFG::BuiltinCall const &_call) {
                          OS << getInstName(_call.Builtin) << ": ";
                        },
                        [&](CFG::Assignment const &_assignment) {
                          OS << "Assignment(";
                          for (const auto &Var : _assignment.Variables)
                            OS << printReg(Var.VirtualReg, nullptr, 0, nullptr)
                               << ", ";

                          OS << ")";
                        }},
               operation.Operation);
    OS << "\\l\\\n";

    assert(operation.Input.size() <= entryLayout.size());
    for (size_t i = 0; i < operation.Input.size(); ++i)
      entryLayout.pop_back();
    entryLayout += operation.Output;
    OS << stackToString(entryLayout) << "\\l\\\n";
  }
  OS << "\n";
  OS << stackToString(blockInfo.exitLayout) << "\\l\\\n";
  OS << "\"];\n";

  std::visit(
      Overload{[&](CFG::BasicBlock::InvalidExit const &) {
                 assert(0 && "Invalid basic block exit");
               },
               [&](CFG::BasicBlock::Jump const &_jump) {
                 OS << "Block" << getBlockId(_block) << " -> Block"
                    << getBlockId(_block) << "Exit [arrowhead=none];\n";
                 OS << "Block" << getBlockId(_block) << "Exit [label=\"";
                 if (_jump.Backwards)
                   OS << "Backwards";
                 OS << "Jump\" shape=oval];\n";
                 OS << "Block" << getBlockId(_block) << "Exit -> Block"
                    << getBlockId(*_jump.Target) << ";\n";
               },
               [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
                 OS << "Block" << getBlockId(_block) << " -> Block"
                    << getBlockId(_block) << "Exit;\n";
                 OS << "Block" << getBlockId(_block) << "Exit [label=\"{ ";
                 OS << stackSlotToString(_conditionalJump.Condition);
                 OS << "| { <0> Zero | <1> NonZero }}\" shape=Mrecord];\n";
                 OS << "Block" << getBlockId(_block);
                 OS << "Exit:0 -> Block" << getBlockId(*_conditionalJump.Zero)
                    << ";\n";
                 OS << "Block" << getBlockId(_block);
                 OS << "Exit:1 -> Block"
                    << getBlockId(*_conditionalJump.NonZero) << ";\n";
               },
               [&](CFG::BasicBlock::FunctionReturn const &_return) {
                 OS << "Block" << getBlockId(_block)
                    << "Exit [label=\"FunctionReturn["
                    << _return.Info->MF->getName() << "]\"];\n";
                 OS << "Block" << getBlockId(_block) << " -> Block"
                    << getBlockId(_block) << "Exit;\n";
               },
               [&](CFG::BasicBlock::Terminated const &) {
                 OS << "Block" << getBlockId(_block)
                    << "Exit [label=\"Terminated\"];\n";
                 OS << "Block" << getBlockId(_block) << " -> Block"
                    << getBlockId(_block) << "Exit;\n";
               }},
      _block.Exit);
  OS << "\n";
}

std::string StackLayoutPrinter::getBlockId(CFG::BasicBlock const &_block) {
  std::string Name = std::to_string(_block.MBB->getNumber()) + "." +
                     std::string(_block.MBB->getName());
  if (auto It = BlockIds.find(&_block); It != BlockIds.end())
    return std::to_string(It->second) + "(" + Name + ")'";
  size_t id = BlockIds[&_block] = BlockCount++;
  BlocksToPrint.emplace_back(&_block);
  return std::to_string(id) + "(" + Name + ")'";
}

StackLayout StackLayoutGenerator::run(const CFG &Cfg) {
  StackLayout stackLayout;
  StackLayoutGenerator{stackLayout, &Cfg.FuncInfo}.processEntryPoint(
      *Cfg.FuncInfo.Entry, &Cfg.FuncInfo);
  return stackLayout;
}
/*
std::vector<StackLayoutGenerator::StackTooDeep>
StackLayoutGenerator::reportStackTooDeep(CFG const &Cfg) {
  StackLayout stackLayout;
  StackLayoutGenerator generator{stackLayout, &Cfg.FuncInfo};
  CFG::BasicBlock const *entry = Cfg.FuncInfo.Entry;
  generator.processEntryPoint(*entry);
  return generator.reportStackTooDeep(*entry);
}
*/
StackLayoutGenerator::StackLayoutGenerator(
    StackLayout &_layout, CFG::FunctionInfo const *_functionInfo)
    : Layout(_layout), CurrentFunctionInfo(_functionInfo) {}

namespace {

/// @returns all stack too deep errors that would occur when shuffling @a Source
/// to @a Target.
std::vector<StackLayoutGenerator::StackTooDeep>
findStackTooDeep(Stack const &Source, Stack const &Target) {
  Stack CurrentStack = Source;
  std::vector<StackLayoutGenerator::StackTooDeep> Errors;

  auto getVariableChoices = [](auto &&SlotRange) {
    std::vector<Register> result;
    for (auto const &slot : SlotRange)
      if (auto const *variableSlot = std::get_if<VariableSlot>(&slot))
        if (!EVMUtils::contains(result, variableSlot->VirtualReg))
          result.push_back(variableSlot->VirtualReg);
    return result;
  };

  ::createStackLayout(
      CurrentStack, Target,
      [&](unsigned I) {
        if (I > 16)
          Errors.emplace_back(StackLayoutGenerator::StackTooDeep{
              I - 16,
              getVariableChoices(EVMUtils::take_last(CurrentStack, I + 1))});
      },
      [&](StackSlot const &Slot) {
        if (canBeFreelyGenerated(Slot))
          return;

        if (auto depth =
                EVMUtils::findOffset(EVMUtils::get_reverse(CurrentStack), Slot);
            depth && *depth >= 16)
          Errors.emplace_back(StackLayoutGenerator::StackTooDeep{
              *depth - 15, getVariableChoices(
                               EVMUtils::take_last(CurrentStack, *depth + 1))});
      },
      [&]() {});
  return Errors;
}

/// @returns the ideal stack to have before executing an operation that outputs
/// @a _operationOutput, s.t. shuffling to @a _post is cheap (excluding the
/// input of the operation itself). If @a _generateSlotOnTheFly returns true for
/// a slot, this slot should not occur in the ideal stack, but rather be
/// generated on the fly during shuffling.
template <typename Callable>
Stack createIdealLayout(const Stack &_operationOutput, const Stack &_post,
                        Callable _generateSlotOnTheFly) {
  struct PreviousSlot {
    size_t slot;
  };
  using LayoutT = std::vector<std::variant<PreviousSlot, StackSlot>>;

  // Determine the number of slots that have to be on stack before executing the
  // operation (excluding the inputs of the operation itself). That is slots
  // that should not be generated on the fly and are not outputs of the
  // operation.
  size_t preOperationLayoutSize = _post.size();
  for (auto const &slot : _post)
    if (EVMUtils::contains(_operationOutput, slot) ||
        _generateSlotOnTheFly(slot))
      --preOperationLayoutSize;

  // The symbolic layout directly after the operation has the form
  // PreviousSlot{0}, ..., PreviousSlot{n}, [output<0>], ..., [output<m>]
  LayoutT layout;
  for (size_t Index = 0; Index < preOperationLayoutSize; ++Index)
    layout.emplace_back(PreviousSlot{Index});
  layout += _operationOutput;

  // Shortcut for trivial case.
  if (layout.empty())
    return Stack{};

  // Next we will shuffle the layout to the post stack using ShuffleOperations
  // that are aware of PreviousSlot's.
  struct ShuffleOperations {
    LayoutT &layout;
    const Stack &post;
    std::set<StackSlot> outputs;
    Multiplicity multiplicity;
    Callable generateSlotOnTheFly;
    ShuffleOperations(LayoutT &_layout, Stack const &_post,
                      Callable _generateSlotOnTheFly)
        : layout(_layout), post(_post),
          generateSlotOnTheFly(_generateSlotOnTheFly) {
      for (auto const &layoutSlot : layout)
        if (const StackSlot *slot = std::get_if<StackSlot>(&layoutSlot))
          outputs.insert(*slot);

      for (auto const &layoutSlot : layout)
        if (const StackSlot *slot = std::get_if<StackSlot>(&layoutSlot))
          --multiplicity[*slot];

      for (auto &&slot : post)
        if (outputs.count(slot) || generateSlotOnTheFly(slot))
          ++multiplicity[slot];
    }

    bool isCompatible(size_t _source, size_t _target) {
      return _source < layout.size() && _target < post.size() &&
             (std::holds_alternative<JunkSlot>(post.at(_target)) ||
              std::visit(Overload{[&](const PreviousSlot &) {
                                    return !outputs.count(post.at(_target)) &&
                                           !generateSlotOnTheFly(
                                               post.at(_target));
                                  },
                                  [&](const StackSlot &_s) {
                                    return _s == post.at(_target);
                                  }},
                         layout.at(_source)));
    }

    bool sourceIsSame(size_t _lhs, size_t _rhs) {
      return std::visit(
          Overload{
              [&](PreviousSlot const &, PreviousSlot const &) { return true; },
              [&](StackSlot const &_lhs, StackSlot const &_rhs) {
                return _lhs == _rhs;
              },
              [&](auto const &, auto const &) { return false; }},
          layout.at(_lhs), layout.at(_rhs));
    }

    int sourceMultiplicity(size_t _offset) {
      return std::visit(
          Overload{[&](PreviousSlot const &) { return 0; },
                   [&](StackSlot const &_s) { return multiplicity.at(_s); }},
          layout.at(_offset));
    }

    int targetMultiplicity(size_t _offset) {
      if (!outputs.count(post.at(_offset)) &&
          !generateSlotOnTheFly(post.at(_offset)))
        return 0;
      return multiplicity.at(post.at(_offset));
    }

    bool targetIsArbitrary(size_t _offset) {
      return _offset < post.size() &&
             std::holds_alternative<JunkSlot>(post.at(_offset));
    }

    void swap(size_t _i) {
      assert(!std::holds_alternative<PreviousSlot>(
                 layout.at(layout.size() - _i - 1)) ||
             !std::holds_alternative<PreviousSlot>(layout.back()));
      std::swap(layout.at(layout.size() - _i - 1), layout.back());
    }

    size_t sourceSize() { return layout.size(); }

    size_t targetSize() { return post.size(); }

    void pop() { layout.pop_back(); }

    void pushOrDupTarget(size_t _offset) { layout.push_back(post.at(_offset)); }
  };

  Shuffler<ShuffleOperations>::shuffle(layout, _post, _generateSlotOnTheFly);

  // Now we can construct the ideal layout before the operation.
  // "layout" has shuffled the PreviousSlot{x} to new places using minimal
  // operations to move the operation output in place. The resulting permutation
  // of the PreviousSlot yields the ideal positions of slots before the
  // operation, i.e. if PreviousSlot{2} is at a position at which _post contains
  // VariableSlot{"tmp"}, then we want the variable tmp in the slot at offset 2
  // in the layout before the operation.
  assert(layout.size() == _post.size());
  std::vector<std::optional<StackSlot>> idealLayout(_post.size(), std::nullopt);
  for (unsigned Idx = 0; Idx < std::min(layout.size(), _post.size()); ++Idx) {
    auto &slot = _post[Idx];
    auto &idealPosition = layout[Idx];
    if (PreviousSlot *previousSlot = std::get_if<PreviousSlot>(&idealPosition))
      idealLayout.at(previousSlot->slot) = slot;
  }

  // The tail of layout must have contained the operation outputs and will not
  // have been assigned slots in the last loop.
  while (!idealLayout.empty() && !idealLayout.back())
    idealLayout.pop_back();

  assert(idealLayout.size() == preOperationLayoutSize);

  Stack Result;
  for (const auto &Item : idealLayout) {
    assert(Item);
    Result.emplace_back(*Item);
  }

  return Result;
}

} // end anonymous namespace

Stack StackLayoutGenerator::propagateStackThroughOperation(
    Stack _exitStack, const CFG::Operation &_operation,
    bool _aggressiveStackCompression) {
  // Enable aggressive stack compression for recursive calls.
  if (auto const *functionCall =
          std::get_if<CFG::FunctionCall>(&_operation.Operation))
    // if (functionCall->recursive)
    //   _aggressiveStackCompression = true;
    _aggressiveStackCompression = false;

  // This is a huge tradeoff between code size, gas cost and stack size.
  auto generateSlotOnTheFly = [&](StackSlot const &_slot) {
    return _aggressiveStackCompression && canBeFreelyGenerated(_slot);
  };

  // Determine the ideal permutation of the slots in _exitLayout that are not
  // operation outputs (and not to be generated on the fly), s.t. shuffling the
  // `stack + _operation.output` to _exitLayout is cheap.
  Stack stack =
      createIdealLayout(_operation.Output, _exitStack, generateSlotOnTheFly);

  // Make sure the resulting previous slots do not overlap with any assignmed
  // variables.
  if (auto const *assignment =
          std::get_if<CFG::Assignment>(&_operation.Operation))
    for (auto &stackSlot : stack)
      if (auto const *varSlot = std::get_if<VariableSlot>(&stackSlot))
        assert(!EVMUtils::contains(assignment->Variables, *varSlot));

  // Since stack+_operation.output can be easily shuffled to _exitLayout, the
  // desired layout before the operation is stack+_operation.input;
  stack += _operation.Input;

  // Store the exact desired operation entry layout. The stored layout will be
  // recreated by the code transform before executing the operation. However,
  // this recreation can produce slots that can be freely generated or are
  // duplicated, i.e. we can compress the stack afterwards without causing
  // problems for code generation later.
  Layout.operationEntryLayout[&_operation] = stack;

  // Remove anything from the stack top that can be freely generated or dupped
  // from deeper on the stack.
  while (!stack.empty()) {
    if (canBeFreelyGenerated(stack.back()))
      stack.pop_back();
    else if (auto offset = EVMUtils::findOffset(
                 EVMUtils::drop_first(EVMUtils::get_reverse(stack), 1),
                 stack.back())) {
      if (*offset + 2 < 16)
        stack.pop_back();
      else
        break;
    } else
      break;
  }

  return stack;
}

Stack StackLayoutGenerator::propagateStackThroughBlock(
    Stack _exitStack, CFG::BasicBlock const &_block,
    bool _aggressiveStackCompression) {
  Stack stack = _exitStack;
  for (auto &operation : EVMUtils::get_reverse(_block.Operations)) {
    Stack newStack = propagateStackThroughOperation(
        stack, operation, _aggressiveStackCompression);
    if (!_aggressiveStackCompression &&
        !findStackTooDeep(newStack, stack).empty())
      // If we had stack errors, run again with aggressive stack compression.
      return propagateStackThroughBlock(std::move(_exitStack), _block, true);
    stack = std::move(newStack);
  }
  return stack;
}

void StackLayoutGenerator::processEntryPoint(
    CFG::BasicBlock const &_entry, CFG::FunctionInfo const *_functionInfo) {
  std::list<CFG::BasicBlock const *> toVisit{&_entry};
  std::set<CFG::BasicBlock const *> visited;

  // TODO: check whether visiting only a subset of these in the outer iteration
  // below is enough.
  std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
      backwardsJumps = collectBackwardsJumps(_entry);
  while (!toVisit.empty()) {
    // First calculate stack layouts without walking backwards jumps, i.e.
    // assuming the current preliminary entry layout of the backwards jump
    // target as the initial exit layout of the backwards-jumping block.
    while (!toVisit.empty()) {
      CFG::BasicBlock const *block = *toVisit.begin();
      toVisit.pop_front();

      LLVM_DEBUG(dbgs() << "Process: " << block->MBB->getNumber() << "."
                        << block->MBB->getName());
      LLVM_DEBUG(dbgs() << " (BBs queue size: " << toVisit.size() << ")");
      if (visited.count(block)) {
        LLVM_DEBUG(dbgs() << "\n");
        continue;
      }

      if (std::optional<Stack> exitLayout =
              getExitLayoutOrStageDependencies(*block, visited, toVisit)) {

        LLVM_DEBUG(dbgs() << ", propagate stack through");
        LLVM_DEBUG(dbgs() << ", exit layout: " << stackToString(*exitLayout));
        visited.emplace(block);
        auto &info = Layout.blockInfos[block];
        info.exitLayout = *exitLayout;
        info.entryLayout = propagateStackThroughBlock(info.exitLayout, *block);
        for (auto entry : block->Entries)
          toVisit.emplace_back(entry);
      }
      LLVM_DEBUG(dbgs() << "\n");
    }

    LLVM_DEBUG(dbgs() << "Handling backwards jumps:\n");
    // Determine which backwards jumps still require fixing and stage revisits
    // of appropriate nodes.
    for (auto [jumpingBlock, target] : backwardsJumps) {
      // This block jumps backwards, but does not provide all slots required by
      // the jump target on exit. Therefore we need to visit the subgraph
      // between ``target`` and ``jumpingBlock`` again.
      auto StartIt = std::begin(Layout.blockInfos[target].entryLayout);
      auto EndIt = std::end(Layout.blockInfos[target].entryLayout);
      if (std::any_of(StartIt, EndIt,
                      [exitLayout = Layout.blockInfos[jumpingBlock].exitLayout](
                          StackSlot const &_slot) {
                        return !EVMUtils::contains(exitLayout, _slot);
                      })) {
        // In particular we can visit backwards starting from ``jumpingBlock``
        // and mark all entries to-be-visited- again until we hit ``target``.
        toVisit.emplace_front(jumpingBlock);
        // Since we are likely to permute the entry layout of ``target``, we
        // also visit its entries again. This is not required for correctness,
        // since the set of stack slots will match, but it may move some
        // required stack shuffling from the loop condition to outside the loop.
        for (CFG::BasicBlock const *entry : target->Entries)
          visited.erase(entry);

        EVMUtils::BreadthFirstSearch<const CFG::BasicBlock *>{{jumpingBlock}}
            .run([&visited, target = target](const CFG::BasicBlock *_block,
                                             auto _addChild) {
              visited.erase(_block);
              if (_block == target)
                return;
              for (auto const *entry : _block->Entries)
                _addChild(entry);
            });
        // While the shuffled layout for ``target`` will be compatible, it can
        // be worthwhile propagating it further up once more. This would mean
        // not stopping at _block == target above, resp. even doing
        // visited.clear() here, revisiting the entire graph. This is a tradeoff
        // between the runtime of this process and the optimality of the result.
        // Also note that while visiting the entire graph again *can* be
        // helpful, it can also be detrimental.
      }
    }
  }

  stitchConditionalJumps(_entry);
  fillInJunk(_entry, _functionInfo);
}

std::optional<Stack> StackLayoutGenerator::getExitLayoutOrStageDependencies(
    const CFG::BasicBlock &_block,
    const std::set<CFG::BasicBlock const *> &_visited,
    std::list<CFG::BasicBlock const *> &_toVisit) const {
  return std::visit(
      Overload{
          [&](CFG::BasicBlock::Jump const &_jump) -> std::optional<Stack> {
            if (_jump.Backwards) {
              // Choose the best currently known entry layout of the jump target
              // as initial exit. Note that this may not yet be the final
              // layout.
              auto It = Layout.blockInfos.find(_jump.Target);
              if (It == Layout.blockInfos.end())
                return Stack{};

              return It->second.entryLayout;
            }
            // If the current iteration has already visited the jump target,
            // start from its entry layout.
            if (_visited.count(_jump.Target))
              return Layout.blockInfos.at(_jump.Target).entryLayout;
            // Otherwise stage the jump target for visit and defer the current
            // block.
            _toVisit.emplace_front(_jump.Target);
            return std::nullopt;
          },
          [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump)
              -> std::optional<Stack> {
            bool zeroVisited = _visited.count(_conditionalJump.Zero);
            bool nonZeroVisited = _visited.count(_conditionalJump.NonZero);

            if (zeroVisited && nonZeroVisited) {
              // If the current iteration has already visited both jump targets,
              // start from its entry layout.
              Stack stack = combineStack(
                  Layout.blockInfos.at(_conditionalJump.Zero).entryLayout,
                  Layout.blockInfos.at(_conditionalJump.NonZero).entryLayout);
              // Additionally, the jump condition has to be at the stack top at
              // exit.
              stack.emplace_back(_conditionalJump.Condition);
              return stack;
            }

            // If one of the jump targets has not been visited, stage it for
            // visit and defer the current block.
            if (!zeroVisited)
              _toVisit.emplace_front(_conditionalJump.Zero);

            if (!nonZeroVisited)
              _toVisit.emplace_front(_conditionalJump.NonZero);

            return std::nullopt;
          },
          [&](CFG::BasicBlock::FunctionReturn const &_functionReturn)
              -> std::optional<Stack> {
            // A function return needs the return variables and the function
            // return label slot on stack.
            assert(_functionReturn.Info);
            Stack stack = _functionReturn.RetValues;
            stack.emplace_back(
                FunctionReturnLabelSlot{_functionReturn.Info->MF});
            return stack;
          },
          [&](CFG::BasicBlock::Terminated const &) -> std::optional<Stack> {
            // A terminating block can have an empty stack on exit.
            return Stack{};
          },
          [](CFG::BasicBlock::InvalidExit const &) -> std::optional<Stack> {
            llvm_unreachable("Unexpected BB terminator");
          }},
      _block.Exit);
}

std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
StackLayoutGenerator::collectBackwardsJumps(
    CFG::BasicBlock const &_entry) const {
  std::list<std::pair<CFG::BasicBlock const *, CFG::BasicBlock const *>>
      backwardsJumps;
  EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *>{{&_entry}}.run(
      [&](CFG::BasicBlock const *_block, auto _addChild) {
        std::visit(
            Overload{
                [&](CFG::BasicBlock::InvalidExit const &) {
                  llvm_unreachable("Unexpected BB terminator");
                },
                [&](CFG::BasicBlock::Jump const &_jump) {
                  if (_jump.Backwards)
                    backwardsJumps.emplace_back(_block, _jump.Target);
                  _addChild(_jump.Target);
                },
                [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
                  _addChild(_conditionalJump.Zero);
                  _addChild(_conditionalJump.NonZero);
                },
                [&](CFG::BasicBlock::FunctionReturn const &) {},
                [&](CFG::BasicBlock::Terminated const &) {},
            },
            _block->Exit);
      });
  return backwardsJumps;
}

void StackLayoutGenerator::stitchConditionalJumps(
    CFG::BasicBlock const &_block) {
  EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *> breadthFirstSearch{
      {&_block}};
  breadthFirstSearch.run([&](CFG::BasicBlock const *_block, auto _addChild) {
    auto &info = Layout.blockInfos.at(_block);
    std::visit(
        Overload{
            [&](CFG::BasicBlock::InvalidExit const &) {
              llvm_unreachable("Unexpected BB terminator");
            },
            [&](CFG::BasicBlock::Jump const &_jump) {
              if (!_jump.Backwards)
                _addChild(_jump.Target);
            },
            [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
              auto &zeroTargetInfo =
                  Layout.blockInfos.at(_conditionalJump.Zero);
              auto &nonZeroTargetInfo =
                  Layout.blockInfos.at(_conditionalJump.NonZero);
              Stack exitLayout = info.exitLayout;

              // The last block must have produced the condition at the stack
              // top.
              assert(!exitLayout.empty());
              assert(exitLayout.back() == _conditionalJump.Condition);
              // The condition is consumed by the jump.
              exitLayout.pop_back();

              auto fixJumpTargetEntry =
                  [&](Stack const &_originalEntryLayout) -> Stack {
                Stack newEntryLayout = exitLayout;
                // Whatever the block being jumped to does not actually require,
                // can be marked as junk.
                for (auto &slot : newEntryLayout)
                  if (!EVMUtils::contains(_originalEntryLayout, slot))
                    slot = JunkSlot{};
                // Make sure everything the block being jumped to requires is
                // actually present or can be generated.
                for (auto const &slot : _originalEntryLayout)
                  assert(canBeFreelyGenerated(slot) ||
                         EVMUtils::contains(newEntryLayout, slot));
                return newEntryLayout;
              };

              zeroTargetInfo.entryLayout =
                  fixJumpTargetEntry(zeroTargetInfo.entryLayout);
              nonZeroTargetInfo.entryLayout =
                  fixJumpTargetEntry(nonZeroTargetInfo.entryLayout);
              _addChild(_conditionalJump.Zero);
              _addChild(_conditionalJump.NonZero);
            },
            [&](CFG::BasicBlock::FunctionReturn const &) {},
            [&](CFG::BasicBlock::Terminated const &) {},
        },
        _block->Exit);
  });
}

Stack StackLayoutGenerator::combineStack(Stack const &_stack1,
                                         Stack const &_stack2) {
  // TODO: it would be nicer to replace this by a constructive algorithm.
  // Currently it uses a reduced version of the Heap Algorithm to partly
  // brute-force, which seems to work decently well.

  Stack commonPrefix;
  for (unsigned Idx = 0; Idx < std::min(_stack1.size(), _stack2.size());
       ++Idx) {
    const StackSlot &slot1 = _stack1[Idx];
    const StackSlot &slot2 = _stack2[Idx];
    if (!(slot1 == slot2))
      break;
    commonPrefix.emplace_back(slot1);
  }

  Stack stack1Tail, stack2Tail;
  for (auto Slot : EVMUtils::drop_first(_stack1, commonPrefix.size()))
    stack1Tail.emplace_back(Slot);

  for (auto Slot : EVMUtils::drop_first(_stack2, commonPrefix.size()))
    stack2Tail.emplace_back(Slot);

  if (stack1Tail.empty())
    return commonPrefix + compressStack(stack2Tail);

  if (stack2Tail.empty())
    return commonPrefix + compressStack(stack1Tail);

  Stack candidate;
  for (auto slot : stack1Tail)
    if (!EVMUtils::contains(candidate, slot))
      candidate.emplace_back(slot);

  for (auto slot : stack2Tail)
    if (!EVMUtils::contains(candidate, slot))
      candidate.emplace_back(slot);

  {
    auto RemIt = std::remove_if(
        candidate.begin(), candidate.end(), [](StackSlot const &slot) {
          return std::holds_alternative<LiteralSlot>(slot) ||
                 std::holds_alternative<FunctionCallReturnLabelSlot>(slot);
        });
    candidate.erase(RemIt, candidate.end());
  }

  auto evaluate = [&](Stack const &_candidate) -> size_t {
    size_t numOps = 0;
    Stack testStack = _candidate;
    auto swap = [&](unsigned _swapDepth) {
      ++numOps;
      if (_swapDepth > 16)
        numOps += 1000;
    };

    auto dupOrPush = [&](StackSlot const &_slot) {
      if (canBeFreelyGenerated(_slot))
        return;

      Stack Tmp = commonPrefix;
      Tmp += testStack;

      auto depth = EVMUtils::findOffset(EVMUtils::get_reverse(Tmp), _slot);
      if (depth && *depth >= 16)
        numOps += 1000;
    };
    createStackLayout(testStack, stack1Tail, swap, dupOrPush, [&]() {});
    testStack = _candidate;
    createStackLayout(testStack, stack2Tail, swap, dupOrPush, [&]() {});
    return numOps;
  };

  // See https://en.wikipedia.org/wiki/Heap's_algorithm
  size_t n = candidate.size();
  Stack bestCandidate = candidate;
  size_t bestCost = evaluate(candidate);
  std::vector<size_t> c(n, 0);
  size_t i = 1;
  while (i < n) {
    if (c[i] < i) {
      if (i & 1)
        std::swap(candidate.front(), candidate[i]);
      else
        std::swap(candidate[c[i]], candidate[i]);

      size_t cost = evaluate(candidate);
      if (cost < bestCost) {
        bestCost = cost;
        bestCandidate = candidate;
      }
      ++c[i];
      // Note that for a proper implementation of the Heap algorithm this would
      // need to revert back to ``i = 1.`` However, the incorrect implementation
      // produces decent result and the proper version would have n! complexity
      // and is thereby not feasible.
      ++i;
    } else {
      c[i] = 0;
      ++i;
    }
  }

  return commonPrefix + bestCandidate;
}

Stack StackLayoutGenerator::compressStack(Stack _stack) {
  std::optional<size_t> firstDupOffset;
  do {
    if (firstDupOffset) {
      std::swap(_stack.at(*firstDupOffset), _stack.back());
      _stack.pop_back();
      firstDupOffset.reset();
    }

    auto I = _stack.rbegin(), E = _stack.rend();
    for (size_t depth = 0; I < E; ++I, ++depth) {
      StackSlot &slot = *I;
      if (canBeFreelyGenerated(slot)) {
        firstDupOffset = _stack.size() - depth - 1;
        break;
      }

      if (auto dupDepth = EVMUtils::findOffset(
              EVMUtils::drop_first(EVMUtils::get_reverse(_stack), depth + 1),
              slot)) {
        if (depth + *dupDepth <= 16) {
          firstDupOffset = _stack.size() - depth - 1;
          break;
        }
      }
    }
  } while (firstDupOffset);
  return _stack;
}

void StackLayoutGenerator::fillInJunk(CFG::BasicBlock const &_block,
                                      CFG::FunctionInfo const *_functionInfo) {
  /// Recursively adds junk to the subgraph starting on @a _entry.
  /// Since it is only called on cut-vertices, the full subgraph retains proper
  /// stack balance.
  auto addJunkRecursive = [&](CFG::BasicBlock const *_entry, size_t _numJunk) {
    EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *> breadthFirstSearch{
        {_entry}};
    breadthFirstSearch.run([&](CFG::BasicBlock const *_block, auto _addChild) {
      auto &blockInfo = Layout.blockInfos.at(_block);
      blockInfo.entryLayout =
          Stack{_numJunk, JunkSlot{}} + std::move(blockInfo.entryLayout);
      for (auto const &operation : _block->Operations) {
        auto &operationEntryLayout = Layout.operationEntryLayout.at(&operation);
        operationEntryLayout =
            Stack{_numJunk, JunkSlot{}} + std::move(operationEntryLayout);
      }
      blockInfo.exitLayout =
          Stack{_numJunk, JunkSlot{}} + std::move(blockInfo.exitLayout);

      std::visit(
          Overload{
              [&](CFG::BasicBlock::InvalidExit const &) {
                llvm_unreachable("Unexpected BB terminator");
              },
              [&](CFG::BasicBlock::Jump const &_jump) {
                _addChild(_jump.Target);
              },
              [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
                _addChild(_conditionalJump.Zero);
                _addChild(_conditionalJump.NonZero);
              },
              [&](CFG::BasicBlock::FunctionReturn const &) {
                llvm_unreachable("FunctionReturn : unexpected BB terminator");
              },
              [&](CFG::BasicBlock::Terminated const &) {},
          },
          _block->Exit);
    });
  };

  /// @returns the number of operations required to transform @a _source to @a
  /// _target.
  auto evaluateTransform = [&](Stack _source, Stack const &_target) -> size_t {
    size_t opGas = 0;
    auto swap = [&](unsigned _swapDepth) {
      if (_swapDepth > 16)
        opGas += 1000;
      else
        opGas += 3; // SWAP* gas price;
    };

    auto dupOrPush = [&](StackSlot const &_slot) {
      if (canBeFreelyGenerated(_slot))
        opGas += 3;
      else {
        if (auto depth =
                EVMUtils::findOffset(EVMUtils::get_reverse(_source), _slot)) {
          if (*depth < 16)
            opGas += 3; // gas price for DUP;
          else
            opGas += 1000;
        } else {
          // This has to be a previously unassigned return variable.
          // We at least sanity-check that it is among the return variables at
          // all.
          assert(std::holds_alternative<VariableSlot>(_slot));
          // FIXME:
          // assert(EVMUtils::contains(m_currentFunctionInfo->returnVariables,
          // std::get<VariableSlot>(_slot))); Strictly speaking the cost of the
          // PUSH0 depends on the targeted EVM version, but the difference will
          // not matter here.
          opGas += 2;
        }
      }
    };

    auto pop = [&]() { opGas += 2; };

    createStackLayout(_source, _target, swap, dupOrPush, pop);
    return opGas;
  };

  /// @returns the number of junk slots to be prepended to @a _targetLayout for
  /// an optimal transition from
  /// @a _entryLayout to @a _targetLayout.
  auto getBestNumJunk = [&](Stack const &_entryLayout,
                            Stack const &_targetLayout) -> size_t {
    size_t bestCost = evaluateTransform(_entryLayout, _targetLayout);
    size_t bestNumJunk = 0;
    size_t maxJunk = _entryLayout.size();
    for (size_t numJunk = 1; numJunk <= maxJunk; ++numJunk) {
      size_t cost = evaluateTransform(_entryLayout, Stack{numJunk, JunkSlot{}} +
                                                        _targetLayout);
      if (cost < bestCost) {
        bestCost = cost;
        bestNumJunk = numJunk;
      }
    }
    return bestNumJunk;
  };

  if (_functionInfo && !_functionInfo->CanContinue && _block.AllowsJunk()) {
    Stack Params;
    for (const auto &Param : _functionInfo->Parameters)
      Params.emplace_back(Param);
    std::reverse(Params.begin(), Params.end());
    size_t bestNumJunk =
        getBestNumJunk(Params, Layout.blockInfos.at(&_block).entryLayout);
    if (bestNumJunk > 0)
            addJunkRecursive(&_block, bestNumJunk);
  }

  /// Traverses the CFG and at each block that allows junk, i.e. that is a
  /// cut-vertex that never leads to a function return, checks if adding junk
  /// reduces the shuffling cost upon entering and if so recursively adds junk
  /// to the spanned subgraph.
  EVMUtils::BreadthFirstSearch<CFG::BasicBlock const *>{{&_block}}.run(
      [&](CFG::BasicBlock const *_block, auto _addChild) {
        if (_block->AllowsJunk()) {
          auto &blockInfo = Layout.blockInfos.at(_block);
          Stack entryLayout = blockInfo.entryLayout;
          Stack const &nextLayout =
              _block->Operations.empty()
                  ? blockInfo.exitLayout
                  : Layout.operationEntryLayout.at(&_block->Operations.front());
          if (entryLayout != nextLayout) {
            size_t bestNumJunk = getBestNumJunk(entryLayout, nextLayout);
            if (bestNumJunk > 0) {
              addJunkRecursive(_block, bestNumJunk);
              blockInfo.entryLayout = entryLayout;
            }
          }
        }

        std::visit(
            Overload{
                [&](CFG::BasicBlock::InvalidExit const &) {
                  llvm_unreachable("Invalid BB terminator");
                },
                [&](CFG::BasicBlock::Jump const &_jump) {
                  _addChild(_jump.Target);
                },
                [&](CFG::BasicBlock::ConditionalJump const &_conditionalJump) {
                  _addChild(_conditionalJump.Zero);
                  _addChild(_conditionalJump.NonZero);
                },
                [&](CFG::BasicBlock::FunctionReturn const &) {},
                [&](CFG::BasicBlock::Terminated const &) {},
            },
            _block->Exit);
      });
}

std::string StackLayoutGenerator::Test(MachineBasicBlock *MBB) {
  Stack SourceStack;
  Stack TargetStack;

  // Create a new MBB for the code after the OrigBB.
  MachineFunction &MF = *MBB->getParent();
  MachineBasicBlock *NewBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
  MF.insert(++MBB->getIterator(), NewBB);

  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  const MCInstrDesc &MCID = TII->get(EVM::SELFBALANCE);
  MachineRegisterInfo &MRI = MF.getRegInfo();

  auto CreateInstr = [&]() {
    Register Reg = MRI.createVirtualRegister(&EVM::GPRRegClass);
    MachineInstr *MI = BuildMI(NewBB, DebugLoc(), MCID, Reg);
    return std::pair(MI, Reg);
  };
  SmallVector<std::pair<MachineInstr *, Register>> Instrs;
  for (unsigned I = 0; I < 17; ++I)
    Instrs.emplace_back(CreateInstr());

  // [ v0 v1 v2 v3 v4 v5 v6 v7 v9 v10 v11 v12 v13 v14 v15 v16 RET RET v5 ]
  SourceStack.emplace_back(VariableSlot{Instrs[0].second});
  SourceStack.emplace_back(VariableSlot{Instrs[1].second});
  SourceStack.emplace_back(VariableSlot{Instrs[2].second});
  SourceStack.emplace_back(VariableSlot{Instrs[3].second});
  SourceStack.emplace_back(VariableSlot{Instrs[4].second});
  SourceStack.emplace_back(VariableSlot{Instrs[5].second});
  SourceStack.emplace_back(VariableSlot{Instrs[6].second});
  SourceStack.emplace_back(VariableSlot{Instrs[7].second});
  SourceStack.emplace_back(VariableSlot{Instrs[9].second});
  SourceStack.emplace_back(VariableSlot{Instrs[10].second});
  SourceStack.emplace_back(VariableSlot{Instrs[11].second});
  SourceStack.emplace_back(VariableSlot{Instrs[12].second});
  SourceStack.emplace_back(VariableSlot{Instrs[13].second});
  SourceStack.emplace_back(VariableSlot{Instrs[14].second});
  SourceStack.emplace_back(VariableSlot{Instrs[15].second});
  SourceStack.emplace_back(VariableSlot{Instrs[16].second});
  SourceStack.emplace_back(FunctionReturnLabelSlot{MBB->getParent()});
  SourceStack.emplace_back(FunctionReturnLabelSlot{MBB->getParent()});
  SourceStack.emplace_back(VariableSlot{Instrs[5].second});

  // [ v1 v0 v2 v3 v4 v5 v6 v7 v9 v10 v11 v12 v13 v14 v15 v16 RET JUNK JUNK ]
  TargetStack.emplace_back(VariableSlot{Instrs[1].second});
  TargetStack.emplace_back(VariableSlot{Instrs[0].second});
  TargetStack.emplace_back(VariableSlot{Instrs[2].second});
  TargetStack.emplace_back(VariableSlot{Instrs[3].second});
  TargetStack.emplace_back(VariableSlot{Instrs[4].second});
  TargetStack.emplace_back(VariableSlot{Instrs[5].second});
  TargetStack.emplace_back(VariableSlot{Instrs[6].second});
  TargetStack.emplace_back(VariableSlot{Instrs[7].second});
  TargetStack.emplace_back(VariableSlot{Instrs[9].second});
  TargetStack.emplace_back(VariableSlot{Instrs[10].second});
  TargetStack.emplace_back(VariableSlot{Instrs[11].second});
  TargetStack.emplace_back(VariableSlot{Instrs[12].second});
  TargetStack.emplace_back(VariableSlot{Instrs[13].second});
  TargetStack.emplace_back(VariableSlot{Instrs[14].second});
  TargetStack.emplace_back(VariableSlot{Instrs[15].second});
  TargetStack.emplace_back(VariableSlot{Instrs[16].second});
  TargetStack.emplace_back(FunctionReturnLabelSlot{MBB->getParent()});
  TargetStack.emplace_back(JunkSlot{});
  TargetStack.emplace_back(JunkSlot{});

  std::ostringstream output;
  createStackLayout(
      SourceStack, TargetStack,
      [&](unsigned _swapDepth) { // swap
        output << stackToString(SourceStack) << std::endl;
        output << "SWAP" << _swapDepth << std::endl;
      },
      [&](StackSlot const &_slot) { // dupOrPush
        output << stackToString(SourceStack) << std::endl;
        if (canBeFreelyGenerated(_slot))
          output << "PUSH " << stackSlotToString(_slot) << std::endl;
        else {
          Stack TmpStack = SourceStack;
          std::reverse(TmpStack.begin(), TmpStack.end());
          auto it = std::find(TmpStack.begin(), TmpStack.end(), _slot);
          if (it == TmpStack.end())
            llvm_unreachable("Invalid DUP operation.");

          auto depth = std::distance(TmpStack.begin(), it);
          output << "DUP" << depth + 1 << std::endl;
        }
      },
      [&]() { // pop
        output << stackToString(SourceStack) << std::endl;
        output << "POP" << std::endl;
      });

  output << stackToString(SourceStack) << std::endl;
  return output.str();
}
