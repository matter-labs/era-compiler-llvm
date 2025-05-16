//===-- EraVMExpandSelect.cpp - Expand select instructions ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands SEL pseudo instructions into target
// instructions.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-expand-select"
#define ERAVM_EXPAND_SELECT_NAME "EraVM expand select pseudo instructions"

namespace {

/// Lower SEL instruction family to uncoditional + conditional move.
/// Select x, y, cc -> add x, r0 + add.cc y, r0.
class EraVMExpandSelect : public MachineFunctionPass {
public:
  static char ID;
  EraVMExpandSelect() : MachineFunctionPass(ID) {
    initializeEraVMExpandSelectPass(*PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return ERAVM_EXPAND_SELECT_NAME; }

private:
  const EraVMInstrInfo *TII{};
};

char EraVMExpandSelect::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMExpandSelect, DEBUG_TYPE, ERAVM_EXPAND_SELECT_NAME, false,
                false)

/// For given \p Select and argument \p Kind return corresponding mov opcode
/// for conditional or unconditional mov.
static unsigned movOpcode(EraVM::ArgumentKind Kind, unsigned Select) {
  auto getOpcode = [Kind, Select] {
    switch (EraVM::argumentType(Kind, Select)) {
    case EraVM::ArgumentType::Register:
      if (Select == EraVM::FATPTR_SELrrr)
        return EraVM::PTR_ADDrrr_s;
      return EraVM::ADDrrr_s;
    case EraVM::ArgumentType::Immediate:
      return EraVM::ADDirr_s;
    case EraVM::ArgumentType::Code:
      return EraVM::ADDcrr_s;
    case EraVM::ArgumentType::Stack:
      return EraVM::ADDsrr_s;
    default:
      break;
    }
    llvm_unreachable("Unexpected argument type");
  };

  unsigned Op = getOpcode();

  if (EraVM::hasSROutAddressingMode(Select))
    Op = EraVM::getWithSROutAddrMode(Op);

  return Op;
}

bool EraVMExpandSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  DenseMap<unsigned, unsigned> Inverse{
      {EraVM::SELrrr, EraVM::SELrrr},
      {EraVM::SELrir, EraVM::SELirr},
      {EraVM::SELrcr, EraVM::SELcrr},
      {EraVM::SELrsr, EraVM::SELsrr},
      {EraVM::FATPTR_SELrrr, EraVM::FATPTR_SELrrr},
  };

  DenseMap<unsigned, unsigned> InverseCond{
      {EraVMCC::COND_E, EraVMCC::COND_NE},
      {EraVMCC::COND_NE, EraVMCC::COND_E},
      {EraVMCC::COND_LT, EraVMCC::COND_GE},
      {EraVMCC::COND_LE, EraVMCC::COND_GT},
      {EraVMCC::COND_GT, EraVMCC::COND_LE},
      {EraVMCC::COND_GE, EraVMCC::COND_LT},
  };

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!EraVM::isSelect(MI))
        continue;

      unsigned Opc = MI.getOpcode();
      DebugLoc DL = MI.getDebugLoc();
      auto *In0 = EraVM::in0Iterator(MI);
      auto In0Range = EraVM::in0Range(MI);
      auto In1Range = EraVM::in1Range(MI);
      auto Out = EraVM::out0Iterator(MI);
      auto CCVal = getImmOrCImm(*EraVM::ccIterator(MI));

      // For rN = cc ? rN : y it's profitable to reverse (rN = reverse_cc ? y :
      // rN) It allows to lower select to a single instruction rN =
      // add.reverse_cc y, r0.
      bool ShouldInverse =
          Inverse.count(Opc) != 0U && Out->getReg() == In0->getReg();

      auto buildMOV = [&](EraVM::ArgumentKind OpNo, unsigned CC) {
        auto OperandRange =
            (OpNo == EraVM::ArgumentKind::In0) ? In0Range : In1Range;
        auto *OperandIt = OperandRange.begin();
        bool IsRegister =
            argumentType(OpNo, MI) == EraVM::ArgumentType::Register;
        unsigned MovOpc = movOpcode(OpNo, Opc);
        // Avoid unconditional mov rN, rN
        if (CC == EraVMCC::COND_NONE && IsRegister &&
            OperandIt->getReg() == Out->getReg() &&
            (EraVM::hasRROutAddressingMode(MI) || Opc == EraVM::FATPTR_SELrrr))
          return;

        MachineInstrBuilder Mov;
        if (EraVM::hasSROutAddressingMode(MI))
          Mov = BuildMI(MBB, &MI, DL, TII->get(MovOpc));
        else
          Mov = BuildMI(MBB, &MI, DL, TII->get(MovOpc), Out->getReg());

        EraVM::copyOperands(Mov, OperandRange);
        Mov.addReg(EraVM::R0);

        if (EraVM::hasSROutAddressingMode(MI))
          EraVM::copyOperands(Mov, EraVM::out0Range(MI));

        Mov.addImm(CC);
        if (CC != EraVMCC::COND_NONE)
          Mov.addReg(EraVM::Flags, RegState::Implicit);

        LLVM_DEBUG(dbgs() << '\t' << *Mov << '\n');
      };

      LLVM_DEBUG(dbgs() << "Replace\t" << MI << "with:\n");

      if (ShouldInverse) {
        assert(CCVal != EraVMCC::COND_OF &&
               "The overflow LT shouldn't be inversed");
        buildMOV(EraVM::ArgumentKind::In0, EraVMCC::COND_NONE);
        buildMOV(EraVM::ArgumentKind::In1, InverseCond[CCVal]);
      } else {
        buildMOV(EraVM::ArgumentKind::In1, EraVMCC::COND_NONE);
        buildMOV(EraVM::ArgumentKind::In0, CCVal);
      }

      PseudoInst.push_back(&MI);
    }

  for (auto *I : PseudoInst)
    I->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !PseudoInst.empty();
}

/// createEraVMExpandPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createEraVMExpandSelectPass() {
  return new EraVMExpandSelect();
}
