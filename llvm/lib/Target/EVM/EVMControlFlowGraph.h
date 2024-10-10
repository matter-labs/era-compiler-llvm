//===----- EVMControlFlowGraph.h - CFG for stackification -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines Control Flow Graph used for the stackification algorithm.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPH_H
#define LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPH_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/MC/MCSymbol.h"

#include <cassert>
#include <list>
#include <variant>

namespace llvm {

class MachineFunction;
class MachineBasicBlock;
class MachineInstr;
/// The following structs describe different kinds of stack slots.
/// Each stack slot is equality- and less-than-comparable and
/// specifies an attribute ``canBeFreelyGenerated`` that is true,
/// if a slot of this kind always has a known value at compile time and
/// therefore can safely be removed from the stack at any time and then
/// regenerated later.

/// The label pushed as return label before a function call, i.e. the label the
/// call is supposed to return to.
struct FunctionCallReturnLabelSlot {
  const MachineInstr *Call = nullptr;
  static constexpr bool canBeFreelyGenerated = true;

  bool operator==(FunctionCallReturnLabelSlot const &Rhs) const {
    return Call == Rhs.Call;
  }

  bool operator<(FunctionCallReturnLabelSlot const &Rhs) const {
    return Call < Rhs.Call;
  }
};

/// The return jump target of a function while generating the code of the
/// function body. I.e. the caller of a function pushes a
/// ``FunctionCallReturnLabelSlot`` (see above) before jumping to the function
/// and this very slot is viewed as ``FunctionReturnLabelSlot`` inside the
/// function body and jumped to when returning from the function.
struct FunctionReturnLabelSlot {
  const MachineFunction *MF = nullptr;
  static constexpr bool canBeFreelyGenerated = false;

  bool operator==(FunctionReturnLabelSlot const &Rhs) const {
    // There can never be return label slots of different functions on stack
    // simultaneously.
    assert(MF == Rhs.MF);
    return true;
  }

  bool operator<(FunctionReturnLabelSlot const &Rhs) const {
    // There can never be return label slots of different functions on stack
    // simultaneously.
    assert(MF == Rhs.MF);
    return false;
  }
};

/// A slot containing the current value of a particular variable.
struct VariableSlot {
  Register VirtualReg;
  static constexpr bool canBeFreelyGenerated = false;

  bool operator==(VariableSlot const &Rhs) const {
    return VirtualReg == Rhs.VirtualReg;
  }

  bool operator<(VariableSlot const &Rhs) const {
    return VirtualReg < Rhs.VirtualReg;
  }
};

/// A slot containing a literal value.
struct LiteralSlot {
  APInt Value;
  static constexpr bool canBeFreelyGenerated = true;

  bool operator==(LiteralSlot const &Rhs) const { return Value == Rhs.Value; }

  bool operator<(LiteralSlot const &Rhs) const { return Value.ult(Rhs.Value); }
};

/// A slot containing a Symbol.
struct SymbolSlot {
  MCSymbol *Symbol;
  static constexpr bool canBeFreelyGenerated = true;

  bool operator==(SymbolSlot const &Rhs) const { return Symbol == Rhs.Symbol; }

  bool operator<(SymbolSlot const &Rhs) const { return Symbol < Rhs.Symbol; }
};

/// A slot containing the index-th return value of a previous call.
struct TemporarySlot {
  /// The call that returned this slot.
  const MachineInstr *MI = nullptr;

  Register VirtualReg;
  /// Specifies to which of the values returned by the call this slot refers.
  /// index == 0 refers to the slot deepest in the stack after the call.
  size_t Index = 0;
  static constexpr bool canBeFreelyGenerated = false;

  bool operator==(TemporarySlot const &Rhs) const {
    return MI == Rhs.MI && Index == Rhs.Index;
  }

  bool operator<(TemporarySlot const &Rhs) const {
    return std::make_pair(MI, Index) < std::make_pair(Rhs.MI, Rhs.Index);
  }
};

/// A slot containing an arbitrary value that is always eventually popped and
/// never used. Used to maintain stack balance on control flow joins.
struct JunkSlot {
  static constexpr bool canBeFreelyGenerated = true;

  bool operator==(JunkSlot const &) const { return true; }

  bool operator<(JunkSlot const &) const { return false; }
};

using StackSlot =
    std::variant<FunctionCallReturnLabelSlot, FunctionReturnLabelSlot,
                 VariableSlot, LiteralSlot, SymbolSlot, TemporarySlot,
                 JunkSlot>;

/// The stack top is usually the last element of the vector.
using Stack = std::vector<StackSlot>;

/// Returns true if Slot can be generated on the stack at any time.
inline bool canBeFreelyGenerated(StackSlot const &Slot) {
  return std::visit(
      [](auto const &TypedSlot) {
        return std::decay_t<decltype(TypedSlot)>::canBeFreelyGenerated;
      },
      Slot);
}

/// Control flow graph consisting of ``CFG::BasicBlock``s connected by control
/// flow.
struct CFG {
  explicit CFG() {}
  CFG(CFG const &) = delete;
  CFG(CFG &&) = delete;
  CFG &operator=(CFG const &) = delete;
  CFG &operator=(CFG &&) = delete;
  ~CFG() = default;

  struct BuiltinCall {
    MachineInstr *Builtin = nullptr;
    bool TerminatesOrReverts = false;
  };

  struct FunctionCall {
    const MachineInstr *Call;
    /// True, if the call can return.
    bool CanContinue = true;
    size_t NumArguments = 0;
  };

  struct Assignment {
    /// The variables being assigned to also occur as ``output`` in the
    /// ``Operation`` containing the assignment, but are also stored here for
    /// convenience.
    std::vector<VariableSlot> Variables;
  };

  struct Operation {
    /// Stack slots this operation expects at the top of the stack and consumes.
    Stack Input;
    /// Stack slots this operation leaves on the stack as output.
    Stack Output;
    std::variant<FunctionCall, BuiltinCall, Assignment> Operation;
  };

  struct FunctionInfo;
  /// A basic control flow block containing ``Operation``s acting on the stack.
  /// Maintains a list of entry blocks and a typed exit.
  struct BasicBlock {
    struct InvalidExit {};

    struct ConditionalJump {
      StackSlot Condition;
      BasicBlock *NonZero = nullptr;
      BasicBlock *Zero = nullptr;
      bool FallThrough = false;
      MachineInstr *CondJump = nullptr;
      MachineInstr *UncondJump = nullptr;
    };

    struct Jump {
      BasicBlock *Target = nullptr;
      bool FallThrough = false;
      bool Backwards = false;
      MachineInstr *UncondJump = nullptr;
    };

    struct FunctionReturn {
      Stack RetValues;
      CFG::FunctionInfo *Info = nullptr;
    };

    struct Terminated {};

    MachineBasicBlock *MBB;
    std::vector<BasicBlock *> Entries;
    std::vector<Operation> Operations;
    /// True, if the block is the beginning of a disconnected subgraph. That is,
    /// if no block that is reachable from this block is an ancestor of this
    /// block. In other words, this is true, if this block is the target of a
    /// cut-edge/bridge in the CFG or if the block itself terminates.
    bool IsStartOfSubGraph = false;
    /// True, if there is a path from this block to a function return.
    bool NeedsCleanStack = false;
    /// If the block starts a sub-graph and does not lead to a function return,
    /// we are free to add junk to it.
    bool AllowsJunk() const { return IsStartOfSubGraph && !NeedsCleanStack; }

    std::variant<InvalidExit, Jump, ConditionalJump, FunctionReturn, Terminated>
        Exit = InvalidExit{};
  };

  struct FunctionInfo {
    MachineFunction *MF = nullptr;
    BasicBlock *Entry = nullptr;
    std::vector<VariableSlot> Parameters;
    std::vector<BasicBlock *> Exits;
    bool CanContinue = true;
  };

  FunctionInfo FuncInfo;

  /// Container for blocks for explicit ownership.
  std::list<BasicBlock> Blocks;
  DenseMap<const MachineBasicBlock *, BasicBlock *> MachineBBToBB;

  BasicBlock &getBlock(const MachineBasicBlock *MBB) {
    auto It = MachineBBToBB.find(MBB);
    assert(It != MachineBBToBB.end());
    return *It->second;
  }

  void createBlock(MachineBasicBlock *MBB) {
    auto It = MachineBBToBB.find(MBB);
    if (It == MachineBBToBB.end()) {
      BasicBlock &Block = Blocks.emplace_back(BasicBlock{MBB, {}, {}});
      MachineBBToBB[MBB] = &Block;
    }
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPH_H
