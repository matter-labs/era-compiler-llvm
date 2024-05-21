#ifndef LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H
#define LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H

#include "EVMAssembly.h"
#include "EVMControlFlowGraph.h"
#include "EVMStackLayoutGenerator.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include <optional>
#include <stack>

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMOptimizedCodeTransform {
public:
  /// Use named labels for functions 1) Yes and check that the names are unique
  /// 2) For none of the functions 3) for the first function of each name.
  // enum class UseNamedLabels { YesAndForceUnique, Never,
  // ForFirstFunctionOfEachName };
  /*
    [[nodiscard]] static std::vector<StackTooDeepError> run(
            AbstractAssembly& _assembly,
            AsmAnalysisInfo& _analysisInfo,
            Block const& _block,
            EVMDialect const& _dialect,
            BuiltinContext& _builtinContext,
            UseNamedLabels _useNamedLabelsForFunctions
    );
  */
  static void run(EVMAssembly &_assembly, MachineFunction &MF,
                  const LiveIntervals &LIS, MachineLoopInfo *MLI);

  /// Generate code for the function call @a _call. Only public for using with
  /// std::visit.
  void operator()(CFG::FunctionCall const &_call);
  /// Generate code for the builtin call @a _call. Only public for using with
  /// std::visit.
  void operator()(CFG::BuiltinCall const &_call);
  /// Generate code for the assignment @a _assignment. Only public for using
  /// with std::visit.
  void operator()(CFG::Assignment const &_assignment);

private:
  /*
  OptimizedEVMCodeTransform(
          AbstractAssembly& _assembly,
          BuiltinContext& _builtinContext,
          UseNamedLabels _useNamedLabelsForFunctions,
          CFG const& _dfg,
          StackLayout const& _stackLayout
  );
  */

  EVMOptimizedCodeTransform(EVMAssembly &_assembly, const CFG &_cfg,
                            const StackLayout &_stackLayout,
                            MachineFunction &MF);

  /// Assert that it is valid to transition from @a _currentStack to @a
  /// _desiredStack. That is @a _currentStack matches each slot in @a
  /// _desiredStack that is not a JunkSlot exactly.
  static void assertLayoutCompatibility(Stack const &_currentStack,
                                        Stack const &_desiredStack);

  /// @returns The label of the entry point of the given @a _function.
  /// Creates and stores a new label, if none exists already.
  // AbstractAssembly::LabelID getFunctionLabel(Scope::Function const&
  // _function);
  /// Assert that @a _slot contains the value of @a _expression.
  // static void validateSlot(StackSlot const& _slot, Expression const&
  // _expression);

  /// Shuffles m_stack to the desired @a _targetStack while emitting the
  /// shuffling code to m_assembly. Sets the source locations to the one in @a
  /// _debugData.
  void createStackLayout(Stack _targetStack);

  /// Generate code for the given block @a _block.
  /// Expects the current stack layout m_stack to be a stack layout that is
  /// compatible with the entry layout expected by the block. Recursively
  /// generates code for blocks that are jumped to. The last emitted assembly
  /// instruction is always an unconditional jump or terminating. Always exits
  /// with an empty stack layout.
  void operator()(CFG::BasicBlock const &_block);

  /// Generate code for the given function.
  /// Resets m_stack.
  void operator()();

  EVMAssembly &m_assembly;
  StackLayout const &m_stackLayout;
  CFG::FunctionInfo const *m_funcInfo = nullptr;
  MachineFunction &MF;

  Stack m_stack;
  // std::map<yul::FunctionCall const*, AbstractAssembly::LabelID>
  DenseMap<const MachineInstr *, MCSymbol *> CallToReturnMCSymbol;
  DenseMap<const CFG::BasicBlock *, MCSymbol *> m_blockLabels;
  // std::map<CFG::FunctionInfo const*,
  // AbstractAssembly::LabelID> const m_functionLabels;
  /// Set of blocks already generated. If any of the contained blocks is ever
  /// jumped to, m_blockLabels should contain a jump label for it.
  std::set<CFG::BasicBlock const *> m_generated;
  // std::vector<StackTooDeepError> m_stackErrors;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMOPTIMIZEDCODETRANSFORM_H
