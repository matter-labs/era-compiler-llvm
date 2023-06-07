//===-- EraVMStackAddressConstantPropagation.cpp - Const prop ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that attempts to extract contant part of a stack
// address from the register, replacing (op reg) where reg = reg1 + C with
// (op reg1 + C), thus utilizing reg + imm addressing mode.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include <tuple>

#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-stack-address-constant-propagation"
#define ERAVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME "EraVM bytes to cells"

STATISTIC(NumInstructionsErased, "Number of instructions erased");

namespace {

class EraVMStackAddressConstantPropagation : public MachineFunctionPass {
public:
  static char ID;
  EraVMStackAddressConstantPropagation() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return ERAVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME;
  }

private:
  void expandConst(MachineInstr &MI) const;
  void expandLoadConst(MachineInstr &MI) const;
  void expandThrow(MachineInstr &MI) const;
  std::tuple<bool, Register, int64_t>
  tryExtractConstant(MachineInstr &MI, int Multiplier, int Dividor);
  const EraVMInstrInfo *TII{};
  MachineRegisterInfo *RegInfo{};
  LLVMContext *Context{};
};

char EraVMStackAddressConstantPropagation::ID = 0;

} // namespace

static const std::vector<std::string> BinaryIO = {"MUL", "DIV"};
static const std::vector<std::string> BinaryI = {
    "ADD", "SUB", "AND", "OR", "XOR", "SHL", "SHR", "ROL", "ROR"};

INITIALIZE_PASS(EraVMStackAddressConstantPropagation, DEBUG_TYPE,
                ERAVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME, false, false)

std::tuple<bool, Register, int64_t>
EraVMStackAddressConstantPropagation::tryExtractConstant(MachineInstr &MI,
                                                         int Multiplier,
                                                         int Divisor) {
  if (!TII->genericInstructionFor(MI) || !TII->genericInstructionFor(MI) ||
      !TII->isSilent(MI) || MI.mayStore() || MI.mayLoad())
    return {};

  if (TII->isDiv(MI) && !EraVM::hasIRInAddressingMode(MI))
    return {};

  // If the second out of MUL or DIV is used, don't extract a constant from it.
  if (MI.getNumExplicitDefs() == 2) {
    Register Def2 = MI.getOperand(1).getReg();
    if (Def2 != EraVM::R0 && !RegInfo->use_empty(MI.getOperand(1).getReg()))
      return {};
  }

  // If the result of the operation is used more than once, don't extract a
  // constant from it.
  if (!RegInfo->hasOneNonDBGUse(MI.getOperand(0).getReg()))
    return {};

  if (TII->isDiv(MI)) {
    if (Divisor != 1)
      return {};
    if (getImmOrCImm(MI.getOperand(2)) != 32)
      return {};
    Register In2 = MI.getOperand(3).getReg();
    MachineInstr *DefMI = RegInfo->getVRegDef(In2);
    auto extracted = tryExtractConstant(*DefMI, 1, 32);
    if (!std::get<0>(extracted))
      return {};
    Register NewVR = RegInfo->createVirtualRegister(&EraVM::GR256RegClass);
    MachineInstr *NewMI = BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                                  TII->get(EraVM::DIVxrrr_s))
                              .addDef(NewVR)
                              .addDef(EraVM::R0)
                              .addImm(32)
                              .addReg(std::get<1>(extracted))
                              .addImm(EraVMCC::COND_NONE)
                              .getInstr();
    LLVM_DEBUG(dbgs() << "Replace " << MI << "\n  with " << NewMI);
    ++NumInstructionsErased;
    MI.eraseFromParent();
    return {true, NewVR, std::get<2>(extracted)};
  }

  auto getNewReg = [](std::tuple<bool, Register, int64_t> Result,
                      Register Old) {
    if (std::get<0>(Result))
      return std::get<1>(Result);
    return Old;
  };

  if (TII->isAdd(MI)) {
    if (EraVM::hasRRInAddressingMode(MI)) {
      Register LHSReg = MI.getOperand(1).getReg();
      Register RHSReg = MI.getOperand(2).getReg();
      MachineInstr &LHS = *RegInfo->getVRegDef(LHSReg);
      MachineInstr &RHS = *RegInfo->getVRegDef(RHSReg);
      auto LHSRes = tryExtractConstant(LHS, Multiplier, Divisor);
      auto RHSRes = tryExtractConstant(RHS, Multiplier, Divisor);
      if (!std::get<0>(LHSRes) && !std::get<0>(RHSRes))
        return {};
      Register NewVR = RegInfo->createVirtualRegister(&EraVM::GR256RegClass);
      MachineInstr *NewMI = BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                                    TII->get(EraVM::ADDrrr_s))
                                .addDef(NewVR)
                                .addReg(getNewReg(LHSRes, LHSReg))
                                .addReg(getNewReg(RHSRes, RHSReg))
                                .addImm(EraVMCC::COND_NONE)
                                .getInstr();
      LLVM_DEBUG(dbgs() << "Replace " << MI << "\n  with " << NewMI);
      ++NumInstructionsErased;
      MI.eraseFromParent();
      return {true, NewVR, std::get<2>(LHSRes) + std::get<2>(RHSRes)};
    }
    assert(EraVM::hasIRInAddressingMode(MI));
    Register RHSReg = MI.getOperand(2).getReg();
    unsigned Val = getImmOrCImm(MI.getOperand(1));
    LLVM_DEBUG(dbgs() << "Erase " << MI);
    ++NumInstructionsErased;
    MI.eraseFromParent();
    return {true, RHSReg, Multiplier * Val / Divisor};
  }
  if (TII->isSub(MI)) {
    if (EraVM::hasRRInAddressingMode(MI)) {
      Register LHSReg = MI.getOperand(1).getReg();
      Register RHSReg = MI.getOperand(2).getReg();
      MachineInstr &LHS = *RegInfo->getVRegDef(LHSReg);
      MachineInstr &RHS = *RegInfo->getVRegDef(RHSReg);
      auto LHSRes = tryExtractConstant(LHS, Multiplier, Divisor);
      auto RHSRes = tryExtractConstant(RHS, -Multiplier, Divisor);
      if (!std::get<0>(LHSRes) && !std::get<0>(RHSRes))
        return {};
      Register NewVR = RegInfo->createVirtualRegister(&EraVM::GR256RegClass);
      MachineInstr *NewMI = BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                                    TII->get(EraVM::SUBrrr_s))
                                .addDef(NewVR)
                                .addReg(getNewReg(LHSRes, LHSReg))
                                .addReg(getNewReg(RHSRes, RHSReg))
                                .addImm(EraVMCC::COND_NONE)
                                .getInstr();
      LLVM_DEBUG(dbgs() << "Replace " << MI << "\n  with " << NewMI);
      ++NumInstructionsErased;
      MI.eraseFromParent();
      return {true, NewVR, std::get<2>(LHSRes) + std::get<2>(RHSRes)};
    }
    assert(EraVM::hasIRInAddressingMode(MI));
    Register RHSReg = MI.getOperand(2).getReg();
    unsigned Val = getImmOrCImm(MI.getOperand(1));
    LLVM_DEBUG(dbgs() << "Erase " << MI);
    ++NumInstructionsErased;
    MI.eraseFromParent();
    return {true, RHSReg, -Multiplier * Val / Divisor};
  }

  // RI mul
  // TODO: CPR-919 Make operand access more robust and readable.
  unsigned Val = getImmOrCImm(MI.getOperand(2));
  Register RHSReg = MI.getOperand(3).getReg();
  MachineInstr &RHS = *RegInfo->getVRegDef(RHSReg);
  return tryExtractConstant(RHS, Multiplier * Val, Divisor);
}

bool EraVMStackAddressConstantPropagation::runOnMachineFunction(
    MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');
  RegInfo = &MF.getRegInfo();
  assert(RegInfo->isSSA() && "The pass is supposed to be run on SSA form MIR");

  bool Changed = false;
  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      if (EraVM::hasSRInAddressingMode(*II)) {
        unsigned RegOpndNo = II->getNumExplicitDefs() + 1;
        if (!II->getOperand(RegOpndNo).isReg())
          continue;
        Register Base = II->getOperand(RegOpndNo).getReg();
        if (!RegInfo->hasOneNonDBGUse(Base))
          continue;
        MachineInstr *DefMI = RegInfo->getVRegDef(Base);
        std::tuple<bool, Register, int64_t> extractionResult =
            tryExtractConstant(*DefMI, 1, 1);
        if (std::get<0>(extractionResult)) {
          int64_t C = getImmOrCImm(II->getOperand(RegOpndNo + 1));
          C += std::get<2>(extractionResult);
          LLVM_DEBUG(dbgs() << "Replace " << *II);
          II->getOperand(RegOpndNo).ChangeToRegister(
              std::get<1>(extractionResult), 0);
          II->getOperand(RegOpndNo + 1).ChangeToImmediate(C, 0);
          LLVM_DEBUG(dbgs() << "  with " << *II);
          Changed = true;
        }
      }
    }
  }
  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

/// createEraVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createEraVMStackAddressConstantPropagationPass() {
  return new EraVMStackAddressConstantPropagation();
}
