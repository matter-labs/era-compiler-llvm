//===------------- EVMStackModel.h - Stack Model ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a representation used by the backwards propagation
// stackification algorithm. It consists of 'Operation', 'StackSlot' and 'Stack'
// entities. New stack representation is derived from Machine IR as following:
// MachineInstr   -> Operation
// MachineOperand -> StackSlot
// MI's defs/uses -> Stack (array of StackSlot)
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKMODEL_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKMODEL_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/MC/MCSymbol.h"

#include <variant>

namespace llvm {

class MachineFunction;
class MachineBasicBlock;

/// The following structs describe different kinds of stack slots.
/// Each stack slot is equality- and less-than-comparable and
/// specifies an attribute 'isRematerializable' that is true,
/// if a slot of this kind always has a known value at compile time and
/// therefore can safely be removed from the stack at any time and then
/// regenerated later.

/// The label pushed as return label before a function call, i.e. the label the
/// call is supposed to return to.
struct FunctionCallReturnLabelSlot {
  const MachineInstr *Call = nullptr;
  static constexpr bool isRematerializable = true;

  bool operator==(FunctionCallReturnLabelSlot const &Rhs) const {
    return Call == Rhs.Call;
  }

  bool operator<(FunctionCallReturnLabelSlot const &Rhs) const {
    return Call < Rhs.Call;
  }
};

/// The return jump target of a function while generating the code of the
/// function body. I.e. the caller of a function pushes a
/// 'FunctionCallReturnLabelSlot' (see above) before jumping to the function
/// and this very slot is viewed as 'FunctionReturnLabelSlot' inside the
/// function body and jumped to when returning from the function.
struct FunctionReturnLabelSlot {
  const MachineFunction *MF = nullptr;
  static constexpr bool isRematerializable = false;

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
  static constexpr bool isRematerializable = false;

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
  static constexpr bool isRematerializable = true;

  bool operator==(LiteralSlot const &Rhs) const { return Value == Rhs.Value; }

  bool operator<(LiteralSlot const &Rhs) const { return Value.ult(Rhs.Value); }
};

/// A slot containing a MCSymbol.
struct SymbolSlot {
  MCSymbol *Symbol;
  const MachineInstr *MI = nullptr;
  static constexpr bool isRematerializable = true;

  bool operator==(SymbolSlot const &Rhs) const {
    return Symbol == Rhs.Symbol && MI->getOpcode() == Rhs.MI->getOpcode();
  }

  bool operator<(SymbolSlot const &Rhs) const {
    return std::make_pair(Symbol, MI->getOpcode()) <
           std::make_pair(Rhs.Symbol, Rhs.MI->getOpcode());
  }
};

/// A slot containing the index-th return value of a previous call.
struct TemporarySlot {
  /// The call that returned this slot.
  const MachineInstr *MI = nullptr;

  Register VirtualReg;
  /// Specifies to which of the values returned by the call this slot refers.
  /// index == 0 refers to the slot deepest in the stack after the call.
  size_t Index = 0;
  static constexpr bool isRematerializable = false;

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
  static constexpr bool isRematerializable = true;

  bool operator==(JunkSlot const &) const { return true; }

  bool operator<(JunkSlot const &) const { return false; }
};

using StackSlot =
    std::variant<FunctionCallReturnLabelSlot, FunctionReturnLabelSlot,
                 VariableSlot, LiteralSlot, SymbolSlot, TemporarySlot,
                 JunkSlot>;

/// The stack top is the last element of the vector.
using Stack = std::vector<StackSlot>;

/// Returns true if Slot can be materialized on the stack at any time.
inline bool isRematerializable(const StackSlot &Slot) {
  return std::visit(
      [](auto const &TypedSlot) {
        return std::decay_t<decltype(TypedSlot)>::isRematerializable;
      },
      Slot);
}

struct BuiltinCall {
  MachineInstr *MI = nullptr;
};

struct FunctionCall {
  const MachineInstr *MI;
  size_t NumArguments = 0;
};

struct Assignment {
  /// The variables being assigned to also occur as 'Output' in the
  /// 'Operation' containing the assignment, but are also stored here for
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

class EVMStackModel {
public:
  EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS);
  Stack getFunctionParameters() const;
  StackSlot getStackSlot(const MachineOperand &MO) const;
  Stack getInstrInput(const MachineInstr &MI) const;
  Stack getInstrOutput(const MachineInstr &MI) const;
  Stack getReturnArguments(const MachineInstr &MI) const;
  const std::vector<Operation> &
  getOperations(const MachineBasicBlock *MBB) const {
    return OperationsMap.at(MBB);
  }

private:
  void createOperation(MachineInstr &MI, std::vector<Operation> &Ops) const;

  MachineFunction &MF;
  const LiveIntervals &LIS;
  std::map<const MachineBasicBlock *, std::vector<Operation>> OperationsMap;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPH_H
