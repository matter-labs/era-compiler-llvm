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
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/MC/MCSymbol.h"

#include <variant>

namespace llvm {

class MachineFunction;
class MachineBasicBlock;

std::string getInstName(const MachineInstr *MI);

class StackSlot {
public:
  enum SlotKind {
    SK_Literal,
    SK_Variable,
    SK_Symbol,
    SK_FunctionCallReturnLabel,
    SK_FunctionReturnLabel,
    SK_Temporary,
    SK_Junk,
  };

private:
  const SlotKind KindID;

protected:
  StackSlot(SlotKind KindID) : KindID(KindID) {}

public:
  virtual ~StackSlot() = default;

  unsigned getSlotKind() const { return KindID; }

  // 'isRematerializable()' returns true, if a slot always has a known value
  // at compile time and therefore can safely be removed from the stack at any
  // time and then regenerated later.
  virtual bool isRematerializable() const = 0;
  virtual std::string toString() const = 0;
};

/// A slot containing a literal value.
class LiteralSlot final : public StackSlot {
  APInt Value;

public:
  LiteralSlot(const APInt &V) : StackSlot(SK_Literal), Value(V) {}
  const APInt &getValue() const { return Value; }

  bool isRematerializable() const override { return true; }
  std::string toString() const override {
    SmallString<64> S;
    Value.toStringSigned(S);
    return std::string(S.str());
  }
  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Literal;
  }
};

/// A slot containing the current value of a particular variable.
class VariableSlot final : public StackSlot {
  Register VirtualReg;

public:
  VariableSlot(const Register &R) : StackSlot(SK_Variable), VirtualReg(R) {}
  const Register &getReg() const { return VirtualReg; }

  bool isRematerializable() const override { return false; }
  std::string toString() const override {
    SmallString<64> S;
    raw_svector_ostream OS(S);
    OS << printReg(VirtualReg, nullptr, 0, nullptr);
    return std::string(S.str());
  }
  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Variable;
  }
};

/// A slot containing a MCSymbol.
class SymbolSlot final : public StackSlot {
  MCSymbol *Symbol;
  const MachineInstr *MI = nullptr;

public:
  SymbolSlot(MCSymbol *S, const MachineInstr *MI)
      : StackSlot(SK_Symbol), Symbol(S), MI(MI) {}
  const MachineInstr *getMachineInstr() const { return MI; }
  MCSymbol *getSymbol() const { return Symbol; }

  bool isRematerializable() const override { return true; }
  std::string toString() const override;

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Symbol;
  }
};

/// The label pushed as return label before a function call, i.e. the label the
/// call is supposed to return to.
class FunctionCallReturnLabelSlot final : public StackSlot {
  const MachineInstr *Call = nullptr;

public:
  FunctionCallReturnLabelSlot(const MachineInstr *Call)
      : StackSlot(SK_FunctionCallReturnLabel), Call(Call) {}
  const MachineInstr *getCall() const { return Call; }

  bool isRematerializable() const override { return true; }
  std::string toString() const override;

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_FunctionCallReturnLabel;
  }
};

/// The return jump target of a function while generating the code of the
/// function body. I.e. the caller of a function pushes a
/// 'FunctionCallReturnLabelSlot' (see above) before jumping to the function
/// and this very slot is viewed as 'FunctionReturnLabelSlot' inside the
/// function body and jumped to when returning from the function.
class FunctionReturnLabelSlot final : public StackSlot {
  const MachineFunction *MF = nullptr;

public:
  FunctionReturnLabelSlot(const MachineFunction *MF)
      : StackSlot(SK_FunctionReturnLabel), MF(MF) {}
  const MachineFunction *getMachineFunction() { return MF; }

  bool isRematerializable() const override { return false; }
  std::string toString() const override { return "RET"; }

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_FunctionReturnLabel;
  }
};

/// A slot containing the index-th return value of a previous call.
class TemporarySlot final : public StackSlot {
  /// The call that returned this slot.
  const MachineInstr *MI = nullptr;

  /// Specifies to which of the values returned by the call this slot refers.
  /// index == 0 refers to the slot deepest in the stack after the call.
  size_t Index = 0;

public:
  TemporarySlot(const MachineInstr *MI, size_t Idx)
      : StackSlot(SK_Temporary), MI(MI), Index(Idx) {}

  bool isRematerializable() const override { return false; }
  std::string toString() const override;

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Temporary;
  }
};

/// A slot containing an arbitrary value that is always eventually popped and
/// never used. Used to maintain stack balance on control flow joins.
class JunkSlot final : public StackSlot {
public:
  JunkSlot() : StackSlot(SK_Junk) {}

  bool isRematerializable() const override { return true; }
  std::string toString() const override { return "JUNK"; }

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Junk;
  }
};

/// The stack top is the last element of the vector.
using Stack = SmallVector<StackSlot *>;

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
  SmallVector<VariableSlot *> Variables;
};

struct Operation {
  /// Stack slots this operation expects at the top of the stack and consumes.
  Stack Input;
  /// Stack slots this operation leaves on the stack as output.
  Stack Output;
  std::variant<FunctionCall, BuiltinCall, Assignment> Operation;
};

class EVMStackModel {
  MachineFunction &MF;
  const LiveIntervals &LIS;
  DenseMap<const MachineBasicBlock *, SmallVector<Operation>> OperationsMap;

  // Storage for stack slots.
  mutable DenseMap<APInt, std::unique_ptr<LiteralSlot>> LiteralStorage;
  mutable DenseMap<Register, std::unique_ptr<VariableSlot>> VariableStorage;
  mutable DenseMap<std::pair<MCSymbol *, const MachineInstr *>,
                   std::unique_ptr<SymbolSlot>>
      SymbolStorage;
  mutable DenseMap<const MachineInstr *,
                   std::unique_ptr<FunctionCallReturnLabelSlot>>
      FunctionCallReturnLabelStorage;
  mutable DenseMap<std::pair<const MachineInstr *, size_t>,
                   std::unique_ptr<TemporarySlot>>
      TemporaryStorage;

  // There should be a single FunctionReturnLabelSlot for the MF.
  mutable std::unique_ptr<FunctionReturnLabelSlot> TheFunctionReturnLabelSlot;

public:
  EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS);
  Stack getFunctionParameters() const;
  Stack getInstrInput(const MachineInstr &MI) const;
  Stack getInstrOutput(const MachineInstr &MI) const;
  Stack getReturnArguments(const MachineInstr &MI) const;
  const SmallVector<Operation> &
  getOperations(const MachineBasicBlock *MBB) const {
    return OperationsMap.at(MBB);
  }

  // Get or create a requested stack slot.
  StackSlot *getStackSlot(const MachineOperand &MO) const;
  LiteralSlot *getLiteralSlot(const APInt &V) const {
    if (LiteralStorage.count(V) == 0)
      LiteralStorage[V] = std::make_unique<LiteralSlot>(V);
    return LiteralStorage[V].get();
  }
  VariableSlot *getVariableSlot(const Register &R) const {
    if (VariableStorage.count(R) == 0)
      VariableStorage[R] = std::make_unique<VariableSlot>(R);
    return VariableStorage[R].get();
  }
  SymbolSlot *getSymbolSlot(MCSymbol *S, const MachineInstr *MI) const {
    auto Key = std::make_pair(S, MI);
    if (SymbolStorage.count(Key) == 0)
      SymbolStorage[Key] = std::make_unique<SymbolSlot>(S, MI);
    return SymbolStorage[Key].get();
  }
  FunctionCallReturnLabelSlot *
  getFunctionCallReturnLabelSlot(const MachineInstr *Call) const {
    if (FunctionCallReturnLabelStorage.count(Call) == 0)
      FunctionCallReturnLabelStorage[Call] =
          std::make_unique<FunctionCallReturnLabelSlot>(Call);
    return FunctionCallReturnLabelStorage[Call].get();
  }
  FunctionReturnLabelSlot *
  getFunctionReturnLabelSlot(const MachineFunction *MF) const {
    if (!TheFunctionReturnLabelSlot)
      TheFunctionReturnLabelSlot =
          std::make_unique<FunctionReturnLabelSlot>(MF);
    assert(MF == TheFunctionReturnLabelSlot->getMachineFunction());
    return TheFunctionReturnLabelSlot.get();
  }
  TemporarySlot *getTemporarySlot(const MachineInstr *MI, size_t Idx) const {
    auto Key = std::make_pair(MI, Idx);
    if (TemporaryStorage.count(Key) == 0)
      TemporaryStorage[Key] = std::make_unique<TemporarySlot>(MI, Idx);
    return TemporaryStorage[Key].get();
  }
  // Junk is always the same slot.
  static JunkSlot *getJunkSlot() {
    static JunkSlot TheJunkSlot;
    return &TheJunkSlot;
  }

private:
  void createOperation(MachineInstr &MI, SmallVector<Operation> &Ops) const;
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKMODEL_H
