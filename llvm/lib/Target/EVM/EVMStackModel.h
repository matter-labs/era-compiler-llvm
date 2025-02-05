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

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

namespace llvm {

class MachineFunction;
class MachineBasicBlock;
class MCSymbol;

class StackSlot {
public:
  enum SlotKind {
    SK_Literal,
    SK_Register,
    SK_Symbol,
    SK_FunctionCallReturnLabel,
    SK_FunctionReturnLabel,
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

/// A slot containing a register def.
class RegisterSlot final : public StackSlot {
  Register Reg;

public:
  RegisterSlot(const Register &R) : StackSlot(SK_Register), Reg(R) {}
  const Register &getReg() const { return Reg; }

  bool isRematerializable() const override { return false; }
  std::string toString() const override {
    SmallString<64> S;
    raw_svector_ostream OS(S);
    OS << printReg(Reg, nullptr, 0, nullptr);
    return std::string(S.str());
  }
  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Register;
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

std::string stackToString(const Stack &S);

class Operation {
public:
  enum OpType { BuiltinCall, FunctionCall, Assignment };

private:
  OpType Type;
  // Stack slots this operation expects at the top of the stack and consumes.
  Stack Input;
  // The emulated machine instruction.
  MachineInstr *MI = nullptr;

public:
  Operation(OpType Type, Stack Input, MachineInstr *MI)
      : Type(Type), Input(std::move(Input)), MI(MI) {}

  const Stack &getInput() const { return Input; }
  MachineInstr *getMachineInstr() const { return MI; }

  bool isBuiltinCall() const { return Type == BuiltinCall; }
  bool isFunctionCall() const { return Type == FunctionCall; }
  bool isAssignment() const { return Type == Assignment; }

  std::string toString() const;
};

class EVMStackModel {
  MachineFunction &MF;
  const LiveIntervals &LIS;
  DenseMap<const MachineBasicBlock *, SmallVector<Operation>> OperationsMap;

  // Storage for stack slots.
  mutable DenseMap<APInt, std::unique_ptr<LiteralSlot>> LiteralStorage;
  mutable DenseMap<Register, std::unique_ptr<RegisterSlot>> RegStorage;
  mutable DenseMap<std::pair<MCSymbol *, const MachineInstr *>,
                   std::unique_ptr<SymbolSlot>>
      SymbolStorage;
  mutable DenseMap<const MachineInstr *,
                   std::unique_ptr<FunctionCallReturnLabelSlot>>
      FunctionCallReturnLabelStorage;

  // There should be a single FunctionReturnLabelSlot for the MF.
  mutable std::unique_ptr<FunctionReturnLabelSlot> TheFunctionReturnLabelSlot;

public:
  EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS);
  Stack getFunctionParameters() const;
  Stack getInstrInput(const MachineInstr &MI) const;
  Stack getReturnArguments(const MachineInstr &MI) const;
  const SmallVector<Operation> &
  getOperations(const MachineBasicBlock *MBB) const {
    return OperationsMap.at(MBB);
  }
  SmallVector<StackSlot *>
  getSlotsForInstructionDefs(const MachineInstr *MI) const {
    SmallVector<StackSlot *> Defs;
    for (const auto &MO : MI->defs())
      Defs.push_back(getRegisterSlot(MO.getReg()));
    return Defs;
  }

  // Get or create a requested stack slot.
  StackSlot *getStackSlot(const MachineOperand &MO) const;
  LiteralSlot *getLiteralSlot(const APInt &V) const {
    if (LiteralStorage.count(V) == 0)
      LiteralStorage[V] = std::make_unique<LiteralSlot>(V);
    return LiteralStorage[V].get();
  }
  RegisterSlot *getRegisterSlot(const Register &R) const {
    if (RegStorage.count(R) == 0)
      RegStorage[R] = std::make_unique<RegisterSlot>(R);
    return RegStorage[R].get();
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
