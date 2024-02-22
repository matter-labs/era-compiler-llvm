//===-- EraVMAddConditions.cpp - EraVM Add Conditions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass changes _p instruction variants to _s.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"
#include <algorithm>

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-addcc"
#define ERAVM_ADD_CONDITIONALS_NAME "EraVM add conditionals"

namespace {

class EraVMAddConditions : public MachineFunctionPass {
public:
  static char ID;
  EraVMAddConditions() : MachineFunctionPass(ID) {
    initializeEraVMAddConditionsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_ADD_CONDITIONALS_NAME; }

private:
  const TargetInstrInfo *TII{};
};

char EraVMAddConditions::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMAddConditions, DEBUG_TYPE, ERAVM_ADD_CONDITIONALS_NAME,
                false, false)

namespace {

// Maps opcode of MOV pseudo instruction to the opcodes of ADD pseudo instruction
// It is used to implement MOV -> ADD mapping
// For example, "MOVir_p src, dst" -> "ADDirr_p src, R0, dst"
std::optional<unsigned> MovPseudoMapping(unsigned OpCode) {

  // Generates a case clause mapping a MOV pseudo instruction variant to an ADD
  // non-pseudo instruction.
  // Example: MOV_TO_ADD(i,r) generates "case MOVir_p: return ADDirr_s;"
#define MOV_TO_ADD(t_in, t_out)                                                \
  case EraVM::MOV##t_in##t_out##_p:                                            \
    return EraVM::ADD##t_in##r##t_out##_p;

  switch (OpCode) {
    MOV_TO_ADD(i, r)
    MOV_TO_ADD(i, s)
    MOV_TO_ADD(c, r)
    MOV_TO_ADD(s, s)
    MOV_TO_ADD(s, r)
    MOV_TO_ADD(r, s)
    MOV_TO_ADD(r, r)
  default:
    return std::nullopt;
  }
#undef MOV_TO_ADD
} // MovPseudoMapping

// Maps opcode of PTR_MOV pseudo instruction to the opcodes of PTR_ADD pseudo instruction
// It is used to implement PTR_MOV -> PTR_ADD mapping
// For example, "PTR_MOVsr_p src, dst" -> "PTR_ADDsrr_p src, R0, dst"
std::optional<unsigned> MovPtrPseudoMapping(unsigned OpCode) {

  // Generates a case clause mapping a PTR_MOV pseudo instruction variant to an PTR_ADD
  // pseudo instruction.
  // Example: case EraVM::PTR_MOVsr_p: return EraVM::PTR_ADDsrr_s;
#define PTR_MOV_TO_PTR_ADD(t_in, t_out)                                        \
  case EraVM::PTR_MOV##t_in##t_out##_p:                                        \
    return EraVM::PTR_ADD##t_in##r##t_out##_p;
  switch (OpCode) {
    PTR_MOV_TO_PTR_ADD(s, s)
    PTR_MOV_TO_PTR_ADD(s, r)
    PTR_MOV_TO_PTR_ADD(r, s)
    PTR_MOV_TO_PTR_ADD(r, r)
  default:
    return std::nullopt;
  }
}
MachineInstrBuilder InjectR0AsSrc1(TargetInstrInfo const &TII,
                                   const MCInstrDesc &NewDescr,
                                   MachineInstr &MI) {
  auto builder = BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), NewDescr);
  builder->setFlags(MI.getFlags());

  if (EraVM::hasRROutAddressingMode(NewDescr.getOpcode())) {
    builder.add(MI.getOperand(0));
    LLVM_DEBUG(dbgs() << "Intermediate (out op0): " << *builder.getInstr());
  }
  EraVM::copyOperands(builder, EraVM::in0Range(MI));
  LLVM_DEBUG(dbgs() << "Intermediate (in0): " << *builder.getInstr());
  builder.addReg(EraVM::R0);

  EraVM::copyOperands(builder, EraVM::in0Range(MI).end(),
                      MI.operands_end()
                      );
  // LLVM_DEBUG(dbgs() << "Intermediate (r0): " << *builder.getInstr());
  // if (EraVM::hasSROutAddressingMode(NewDescr.getOpcode())) {
  //   EraVM::copyOperands(builder, EraVM::out0Range(MI));
  // }
  // LLVM_DEBUG(dbgs() << "Intermediate (out0): " << *builder.getInstr());
  builder.addImm(0);
  builder.copyImplicitOps(MI);
  builder.cloneMemRefs(MI);

  return builder;
}

std::optional<unsigned> TryLowerMovOpcode(unsigned Opcode) {
  std::optional<unsigned> MappedToPseudoOpcode = MovPseudoMapping(Opcode);
  if (!MappedToPseudoOpcode) MappedToPseudoOpcode = MovPtrPseudoMapping(Opcode);
  if (!MappedToPseudoOpcode) return std::nullopt;

  const unsigned MappedOpcode =
      llvm::EraVM::getPseudoMapOpcode(*MappedToPseudoOpcode);
  assert(MappedOpcode != UINT_MAX &&
         "both ADDxxx_p and PTR_ADDxxx_p are lowered to their _s versions, but "
         "PseudoMapOpcode does not map them to anything");
  return MappedOpcode;
 }

 bool TryLowerMov(TargetInstrInfo const &TII, MachineInstr &MI) {
  if (const auto MappedOpcodeOpt = TryLowerMovOpcode(MI.getOpcode())) {
    const auto MappedOpcode = *MappedOpcodeOpt;
    LLVM_DEBUG(dbgs() << "Lowering MOV instruction variant " << MI);
    auto builder = InjectR0AsSrc1(TII, TII.get(MappedOpcode), MI);
    LLVM_DEBUG(dbgs() << "Result: " << *builder.getInstr());
    return true;
  }
  return false;
 }

bool LowerOther(TargetInstrInfo const &TII, MachineInstr &MI) {
  auto MappedOpcode = llvm::EraVM::getPseudoMapOpcode(MI.getOpcode());
  if (MappedOpcode == -1) {
    return false;
  }
  MI.setDesc(TII.get(MappedOpcode));
  MI.addOperand(MachineOperand::CreateImm(EraVMCC::COND_NONE));
  return true;
}
} // namespace

bool EraVMAddConditions::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  std::vector<MachineInstr *> PseudoInstToErase;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (TryLowerMov(*TII, MI)) {
        PseudoInstToErase.push_back(&MI);
        Changed = true;
      } else if (LowerOther(*TII, MI)) {
        Changed = true;
      }
    }
  std::for_each(PseudoInstToErase.cbegin(), PseudoInstToErase.cend(),
                [](auto *ins) { ins->eraseFromParent(); });

  return Changed;
}

/// createEraVMAddConditionsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createEraVMAddConditionsPass() {
  return new EraVMAddConditions();
}
