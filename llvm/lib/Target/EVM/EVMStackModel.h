//===------------- EVMStackModel.h - Stack Model ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a representation used by the backwards propagation
// stackification algorithm. It consists of 'StackSlot' and 'Stack' entities.
// New stack representation is derived from Machine IR as following:
// MachineOperand -> StackSlot
// MI's defs/uses -> Stack (array of StackSlot)
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKMODEL_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKMODEL_H

#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

namespace llvm {

class MachineFunction;
class MachineBasicBlock;
class MCSymbol;

class StackSlot {
public:
  enum SlotKind : uint8_t {
    SK_Literal,
    SK_Register,
    SK_Symbol,
    SK_CallerReturn,
    SK_CalleeReturn,
    SK_Unused,
    SK_Unknown
  };

private:
  const SlotKind KindID;

protected:
  explicit StackSlot(SlotKind KindID) : KindID(KindID) {}

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
  explicit LiteralSlot(const APInt &V) : StackSlot(SK_Literal), Value(V) {}
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
  explicit RegisterSlot(const Register &R) : StackSlot(SK_Register), Reg(R) {}
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

/// The label pushed as the return address seen from the caller.
class CallerReturnSlot final : public StackSlot {
  const MachineInstr *Call = nullptr;

public:
  explicit CallerReturnSlot(const MachineInstr *Call)
      : StackSlot(SK_CallerReturn), Call(Call) {}
  const MachineInstr *getCall() const { return Call; }

  bool isRematerializable() const override { return true; }
  std::string toString() const override;

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_CallerReturn;
  }
};

/// The label pushed as the return address seen from the callee.
class CalleeReturnSlot final : public StackSlot {
  const MachineFunction *MF = nullptr;

public:
  explicit CalleeReturnSlot(const MachineFunction *MF)
      : StackSlot(SK_CalleeReturn), MF(MF) {}
  const MachineFunction *getMachineFunction() { return MF; }

  bool isRematerializable() const override { return false; }
  std::string toString() const override { return "RET"; }

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_CalleeReturn;
  }
};

/// A slot containing an arbitrary value that is always eventually popped and
/// never used. Used to maintain stack balance on control flow joins.
class UnusedSlot final : public StackSlot {
public:
  UnusedSlot() : StackSlot(SK_Unused) {}

  bool isRematerializable() const override { return true; }
  std::string toString() const override { return "Unused"; }

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Unused;
  }
};

class UnknownSlot final : public StackSlot {
  size_t Index = 0;

public:
  explicit UnknownSlot(size_t Index) : StackSlot(SK_Unknown), Index(Index) {}

  size_t getIndex() const { return Index; }
  bool isRematerializable() const override { return true; }
  std::string toString() const override {
    return "UNKNOWN(" + std::to_string(Index) + ")";
  }

  static bool classof(const StackSlot *S) {
    return S->getSlotKind() == SK_Unknown;
  }
};

/// The stack top is the last element of the vector.
class Stack : public SmallVector<const StackSlot *> {
public:
  explicit Stack(const StackSlot **Start, const StackSlot **End)
      : SmallVector(Start, End) {}
  explicit Stack(size_t Size, const StackSlot *Value)
      : SmallVector(Size, Value) {}
  explicit Stack(SmallVector<const StackSlot *> &&Slots)
      : SmallVector(std::move(Slots)) {}
  // TODO: should it be explicit? If yes, fix all build errors.
  Stack(const SmallVector<const StackSlot *> &Slots) : SmallVector(Slots) {}
  Stack() = default;

  std::string toString() const {
    std::string Result("[ ");
    for (const auto *It : *this)
      Result += It->toString() + ' ';
    Result += ']';
    return Result;
  }
};

bool isPushOrDupLikeMI(const MachineInstr &MI);
bool isLinkerPseudoMI(const MachineInstr &MI);
bool isNoReturnCallMI(const MachineInstr &MI);

class EVMStackModel {
  MachineFunction &MF;
  const LiveIntervals &LIS;
  unsigned StackDepthLimit;

  // Storage for stack slots.
  mutable DenseMap<APInt, std::unique_ptr<LiteralSlot>> LiteralStorage;
  mutable DenseMap<Register, std::unique_ptr<RegisterSlot>> RegStorage;
  mutable DenseMap<std::pair<MCSymbol *, const MachineInstr *>,
                   std::unique_ptr<SymbolSlot>>
      SymbolStorage;
  mutable DenseMap<const MachineInstr *, std::unique_ptr<CallerReturnSlot>>
      CallerReturnStorage;

  // There should be a single CalleeReturnSlot for the MF.
  mutable std::unique_ptr<CalleeReturnSlot> TheCalleeReturnSlot;

  using MBBStackMap = DenseMap<const MachineBasicBlock *, Stack>;
  using InstStackMap = DenseMap<const MachineInstr *, Stack>;

  // Map MBB to its entry and exit stacks.
  MBBStackMap MBBEntryStackMap;
  // Note: For branches ending with a conditional jump, the exit stack
  // retains the jump condition slot, even though the jump consumes it.
  MBBStackMap MBBExitStackMap;

  // Map an MI to its entry stack.
  InstStackMap InstEntryStackMap;

  // Map an MI to its input slots.
  DenseMap<const MachineInstr *, Stack> MIInputMap;

  // Mutable getters for EVMStackSolver to manage the maps.
  MBBStackMap &getMBBEntryMap() { return MBBEntryStackMap; }
  MBBStackMap &getMBBExitMap() { return MBBExitStackMap; }
  InstStackMap &getInstEntryMap() { return InstEntryStackMap; }
  friend class EVMStackSolver;

public:
  EVMStackModel(MachineFunction &MF, const LiveIntervals &LIS,
                unsigned StackDepthLimit);
  Stack getFunctionParameters() const;
  Stack getReturnArguments(const MachineInstr &MI) const;

  const Stack &getMIInput(const MachineInstr &MI) const {
    return MIInputMap.at(&MI);
  }
  Stack getSlotsForInstructionDefs(const MachineInstr *MI) const {
    Stack Defs;
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
  CallerReturnSlot *getCallerReturnSlot(const MachineInstr *Call) const {
    if (CallerReturnStorage.count(Call) == 0)
      CallerReturnStorage[Call] = std::make_unique<CallerReturnSlot>(Call);
    return CallerReturnStorage[Call].get();
  }
  CalleeReturnSlot *getCalleeReturnSlot(const MachineFunction *MF) const {
    if (!TheCalleeReturnSlot)
      TheCalleeReturnSlot = std::make_unique<CalleeReturnSlot>(MF);
    assert(MF == TheCalleeReturnSlot->getMachineFunction());
    return TheCalleeReturnSlot.get();
  }
  // Unused is always the same slot.
  static UnusedSlot *getUnusedSlot() {
    static UnusedSlot TheUnusedSlot;
    return &TheUnusedSlot;
  }

  const Stack &getMBBEntryStack(const MachineBasicBlock *MBB) const {
    return MBBEntryStackMap.at(MBB);
  }
  const Stack &getMBBExitStack(const MachineBasicBlock *MBB) const {
    return MBBExitStackMap.at(MBB);
  }
  const Stack &getInstEntryStack(const MachineInstr *MI) const {
    return InstEntryStackMap.at(MI);
  }

  unsigned stackDepthLimit() const { return StackDepthLimit; }

private:
  Stack getSlotsForInstructionUses(const MachineInstr &MI) const;
  void processMI(const MachineInstr &MI);

public:
  bool skipMI(const MachineInstr &MI) const {
    auto Opc = MI.getOpcode();
    // If the virtual register has the only definition, ignore this instruction,
    // as we create literal slots from the immediate value at the register uses.
    if (Opc == EVM::CONST_I256 &&
        LIS.getInterval(MI.getOperand(0).getReg()).containsOneValue())
      return true;
    return Opc == EVM::ARGUMENT || Opc == EVM::RET || Opc == EVM::JUMP ||
           Opc == EVM::JUMPI || Opc == EVM::PUSHDEPLOYADDRESS ||
           Opc == EVM::JUMP_UNLESS;
  }
  auto instructionsToProcess(const MachineBasicBlock *MBB) const {
    return make_filter_range(
        MBB->instrs(), [this](const MachineInstr &MI) { return !skipMI(MI); });
  }
  auto reverseInstructionsToProcess(const MachineBasicBlock *MBB) const {
    return make_filter_range(
        make_range(MBB->rbegin(), MBB->rend()),
        [this](const MachineInstr &MI) { return !skipMI(MI); });
  }
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKMODEL_H
