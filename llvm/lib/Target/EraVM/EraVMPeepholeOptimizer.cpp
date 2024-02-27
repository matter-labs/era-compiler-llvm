//===-- EraVMPeepholeOptimizer.cpp - PeepholeOptimizer hooks --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementations for immediate and memory operand foldings. They are declared
// in TargetInstrInfo.h and used by PeepholeOptimizer.
//
//===----------------------------------------------------------------------===//
#include <cstdint>
#include <optional>
#include <utility>

#include "EraVM.h"
#include "EraVMInstrInfo.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMTargetMachine.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadicDetails.h"

using namespace llvm;
using namespace EraVM;

#define DEBUG_TYPE "peephole-opt"

STATISTIC(NumSTFolded,
          "Number of ST1/ST2 instructions transformed to ST1i/ST2i");
STATISTIC(NumArithFolded, "Number of generic arith instructions where reg "
                          "operands were substituted with imms");
STATISTIC(NumMovIRFolded, "Number of MOVir uses folded");
STATISTIC(NumMovCRFolded, "Number of MOVcr or LOADCONST uses folded");

namespace llvm {

namespace {

using Opcode = unsigned;

bool supportsIn0Imm(Opcode OpCode) { return getWithIRInAddrMode(OpCode) != -1; }

bool supportsIn0Code(Opcode OpCode) {
  return getWithCRInAddrMode(OpCode) != -1;
}


bool hasIn0Reg(Opcode OpCode) { return hasRRInAddressingMode(OpCode); }
  bool canFoldImmOrCodeToIn0(Opcode OpCode) {
  return supportsIn0Imm(OpCode) && supportsIn0Code(OpCode) && hasIn0Reg(OpCode);
}

const auto ST_OPERAND_STORED_VALUE_INDEX = 0;

enum class FoldingOptions { In0CodeOrImm, SpecialSTImm, FoldingImpossible };

struct Folding {
  enum class Type { FoldImmGeneric, FoldCode, FoldImmInST };
  Type FoldingType;
  Opcode NewOpcode;
  iterator_range<const MachineOperand *> Operands;
};

std::optional<Opcode> mapSTOpcode(Opcode OpCode) {
  switch (OpCode) {
  case EraVM::ST1:
    return EraVM::ST1i;
  case EraVM::ST2:
    return EraVM::ST2i;
  default:
    return std::nullopt;
  }
}

bool isST(Opcode OpCode) { return mapSTOpcode(OpCode).has_value(); }

FoldingOptions foldingOptionsForST(MachineInstr const &UseMI, Register reg) {
  if (!isST(UseMI.getOpcode()))
    return FoldingOptions::FoldingImpossible;
  assert(UseMI.getOperand(ST_OPERAND_STORED_VALUE_INDEX).isReg() &&
         "ST1 or ST2 instruction is malformed");

  if (UseMI.getOperand(ST_OPERAND_STORED_VALUE_INDEX).getReg() == reg) {
    return FoldingOptions::SpecialSTImm;
  }
  return FoldingOptions::FoldingImpossible;
}

FoldingOptions foldingOptionsGeneric(MachineInstr const &UseMI, Register reg) {
  if (canFoldImmOrCodeToIn0(UseMI.getOpcode()) && reg == in0Iterator(UseMI)->getReg()) {
    return FoldingOptions::In0CodeOrImm;
  }
  return FoldingOptions::FoldingImpossible;
}

FoldingOptions foldingOptions(MachineInstr &UseMI, Register reg) {
  if (isST(UseMI.getOpcode())) {
    auto result = foldingOptionsForST(UseMI, reg);
    if (result != FoldingOptions::FoldingImpossible) {
      return result;
    }
  } else {
    auto result = foldingOptionsGeneric(UseMI, reg);
    if (result != FoldingOptions::FoldingImpossible) {
      return result;
    }
  }
  return FoldingOptions::FoldingImpossible;
}

std::optional<std::pair<ArgumentType, iterator_range<const MachineOperand *>>>
tryExtract(MachineInstr const &MI) {

  auto ImmOrCodeOperandRange = [&](ArgumentType In0Type) {
    const auto *const Start = MI.operands_begin() + MI.getNumExplicitDefs();
    return make_range(Start, Start + argumentSize(In0Type));
  };

  switch (MI.getOpcode()) {

  case EraVM::MOVir_p:
    return std::make_pair(ArgumentType::Immediate,
                          ImmOrCodeOperandRange(ArgumentType::Immediate));
  case EraVM::MOVcr_p:
  case EraVM::LOADCONST:
    return std::make_pair(ArgumentType::Code,
                          ImmOrCodeOperandRange(ArgumentType::Code));
  default:
    return std::nullopt;
  }
}

bool isDefValid(Register Reg, EraVMInstrInfo const &II, MachineInstr &DefMI) {
  const bool NotPredicated =
      DefMI.isPseudo() || II.getCCCode(DefMI) == EraVMCC::COND_NONE;
  return NotPredicated && Reg.isVirtual();
}

bool isDefEraseable(Register Reg, MachineRegisterInfo const &MRI,
                    llvm::EraVMInstrInfo const &TII, MachineInstr &DefMI) {

  LLVM_DEBUG(dbgs() << "Users of reg " << Register::virtReg2Index(Reg) << "\n");
  LLVM_DEBUG(for (const auto &u
                  : MRI.use_instructions(Reg)) { dbgs() << "User: " << u; });

  if (!MRI.hasOneNonDBGUse(Reg)) {
    LLVM_DEBUG(dbgs() << "Not erasing def: has more than one use.\n");
    return false;
  }

  const bool NotPredicated =
      DefMI.isPseudo() || TII.getCCCode(DefMI) == EraVMCC::COND_NONE;
  const bool NoEffects = !EraVMInstrInfo::isFlagSettingInstruction(DefMI);

  LLVM_DEBUG(dbgs() << "Instruction is "
                    << (NotPredicated ? "not predicated" : "predicated")
                    << "; instruction has" << (NoEffects ? "no " : "")
                    << "effects. \n");
  return NotPredicated && NoEffects;
}

MachineInstrBuilder
copyWithIn0Subst(MachineInstr &MI, Opcode NewOpCode,
                 iterator_range<MachineInstr::const_mop_iterator> NewIn0OpRange,
                 TargetInstrInfo const &TII) {
  auto Builder =
      BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII.get(NewOpCode));
  Builder->setFlags(MI.getFlags());
  copyOperands(Builder, MI.operands_begin(), in0Iterator(MI));
  copyOperands(Builder, NewIn0OpRange);
  copyOperands(Builder, in1Iterator(MI), MI.operands_end());
  Builder.copyImplicitOps(MI);
  Builder.cloneMemRefs(MI);
  return Builder;
}

std::optional<Folding> getFolding(Opcode OpCode, ArgumentType ArgType,
                                  iterator_range<const MachineOperand *> Value,
                                  FoldingOptions Options) {
  if (Options == FoldingOptions::In0CodeOrImm) {
    if (ArgType == ArgumentType::Immediate) {
      const auto NewOpCode = getWithIRInAddrMode(OpCode);
      assert(NewOpCode != -1 && "Trying to fold an unsupported instruction");
      return Folding{Folding::Type::FoldImmGeneric,
                     static_cast<Opcode>(NewOpCode), Value};
    }
    if (ArgType == ArgumentType::Code) {
      const auto NewOpCode = getWithCRInAddrMode(OpCode);
      assert(NewOpCode != -1 && "Trying to fold an unsupported instruction");
      return Folding{Folding::Type::FoldCode, static_cast<Opcode>(NewOpCode),
                     Value};
    }
  } else if (Options == FoldingOptions::SpecialSTImm &&
             ArgType == ArgumentType::Immediate) {
    assert(*mapSTOpcode(OpCode) &&
           "Trying to fold in non-ST instruction as if it were one.");
    return Folding{Folding::Type::FoldImmInST, *mapSTOpcode(OpCode), Value};
  }

  return std::nullopt;
}

MachineInstr &performFolding(Folding const &Folding, MachineInstr &UseMI,
                             TargetInstrInfo const &TII) {
  switch (Folding.FoldingType) {
  case Folding::Type::FoldImmGeneric: {
    UseMI.setDesc(TII.get(Folding.NewOpcode));
    const auto &Operand = *Folding.Operands.begin();
    const auto Imm = getImmOrCImm(Operand);
    in0Iterator(UseMI)->ChangeToImmediate(Imm);
    NumMovIRFolded++;
    NumArithFolded++;
    return UseMI;
  }
  case Folding::Type::FoldCode: {
    auto Builder =
        copyWithIn0Subst(UseMI, Folding.NewOpcode, Folding.Operands, TII);
    UseMI.eraseFromParent();
    NumMovCRFolded++;
    NumArithFolded++;
    return *Builder.getInstr();
  }
  case Folding::Type::FoldImmInST: {
    UseMI.setDesc(TII.get(Folding.NewOpcode));
    const auto &Operand = *Folding.Operands.begin();
    const auto Imm = getImmOrCImm(Operand);
    UseMI.getOperand(ST_OPERAND_STORED_VALUE_INDEX).ChangeToImmediate(Imm);
    NumSTFolded++;
    NumMovIRFolded++;
    return UseMI;
  }
  }
}

enum class FoldImmediateResult { FoldedDefErased, FoldedDefNotErased, Skipped };

FoldImmediateResult foldImmediateImpl(MachineInstr &UseMI, MachineInstr &DefMI,
                                      Register Reg,
                                      MachineRegisterInfo const &MRI,
                                      EraVMInstrInfo const &TII) {

  LLVM_DEBUG(dbgs() << "\nFoldImmediate:: Considering folding def instruction: "
                    << DefMI << " into an immediate load for user instruction: "
                    << UseMI);

  if (!isDefValid(Reg, TII, DefMI)) {
    LLVM_DEBUG(dbgs() << "FoldImmediate:: Not considering def " << DefMI);
    return FoldImmediateResult::Skipped;
  }

  const auto FoldingOptions = foldingOptions(UseMI, Reg);
  if (FoldingOptions == FoldingOptions::FoldingImpossible) {
    LLVM_DEBUG(dbgs() << "FoldImmediate:: Unsupported user, can't fold.\n");
    return FoldImmediateResult::Skipped;
  }

  const bool DefEraseable = isDefEraseable(Reg, MRI, TII, DefMI);

  if (const auto OptImmediateDescr = tryExtract(DefMI)) {
    const auto &[DefType, DefRange] = *OptImmediateDescr;

    LLVM_DEBUG(dbgs() << "FoldImmediate:: Extracted def: " << *DefRange.begin()
                      << "\n");
    if (const auto OptFolding =
            getFolding(UseMI.getOpcode(), DefType, DefRange, FoldingOptions)) {
      const auto &FoldedInstr = performFolding(*OptFolding, UseMI, TII);
      if (DefEraseable) {
        LLVM_DEBUG(dbgs() << "FoldImmediate:: erasing " << DefMI);
        DefMI.eraseFromParent();
        return FoldImmediateResult::FoldedDefErased;
      }

      LLVM_DEBUG(dbgs() << "FoldImmediate:: Folded into: " << FoldedInstr);
      return FoldImmediateResult::FoldedDefNotErased;
    }
  }
  LLVM_DEBUG(dbgs() << "FoldImmediate:: Unsupported def, can't fold.\n");
  return FoldImmediateResult::Skipped;
}

} // namespace
bool EraVMInstrInfo::FoldImmediate(MachineInstr &UseMI, MachineInstr &DefMI,
                                   Register Reg,
                                   MachineRegisterInfo *MRI) const {
  return foldImmediateImpl(UseMI, DefMI, Reg, *MRI, *this) ==
         FoldImmediateResult::FoldedDefErased;
}
} // namespace llvm
