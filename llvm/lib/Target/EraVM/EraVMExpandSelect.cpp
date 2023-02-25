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

class EraVMExpandSelect : public MachineFunctionPass {
public:
  static char ID;
  EraVMExpandSelect() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_EXPAND_SELECT_NAME; }

private:
  const TargetInstrInfo *TII{};
  LLVMContext *Context{};
};

char EraVMExpandSelect::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMExpandSelect, DEBUG_TYPE, ERAVM_EXPAND_SELECT_NAME, false,
                false)

bool EraVMExpandSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');
  // pseudo opcode -> mov opcode, cmov opcode, #args src0, #args src1, #args dst
  DenseMap<unsigned, std::vector<unsigned>> Pseudos{
      {EraVM::SELrrr, {EraVM::ADDrrr_s, EraVM::ADDrrr_s, 1, 1, 0}},
      {EraVM::SELirr, {EraVM::ADDirr_s, EraVM::ADDrrr_s, 1, 1, 0}},
      {EraVM::SELcrr, {EraVM::ADDcrr_s, EraVM::ADDrrr_s, 2, 1, 0}},
      {EraVM::SELsrr, {EraVM::ADDsrr_s, EraVM::ADDrrr_s, 3, 1, 0}},
      {EraVM::SELrir, {EraVM::ADDrrr_s, EraVM::ADDirr_s, 1, 1, 0}},
      {EraVM::SELiir, {EraVM::ADDirr_s, EraVM::ADDirr_s, 1, 1, 0}},
      {EraVM::SELcir, {EraVM::ADDcrr_s, EraVM::ADDirr_s, 2, 1, 0}},
      {EraVM::SELsir, {EraVM::ADDsrr_s, EraVM::ADDirr_s, 3, 1, 0}},
      {EraVM::SELrcr, {EraVM::ADDrrr_s, EraVM::ADDcrr_s, 1, 2, 0}},
      {EraVM::SELicr, {EraVM::ADDirr_s, EraVM::ADDcrr_s, 1, 2, 0}},
      {EraVM::SELccr, {EraVM::ADDcrr_s, EraVM::ADDcrr_s, 2, 2, 0}},
      {EraVM::SELscr, {EraVM::ADDsrr_s, EraVM::ADDcrr_s, 3, 2, 0}},
      {EraVM::SELrsr, {EraVM::ADDrrr_s, EraVM::ADDsrr_s, 1, 3, 0}},
      {EraVM::SELisr, {EraVM::ADDirr_s, EraVM::ADDsrr_s, 1, 3, 0}},
      {EraVM::SELcsr, {EraVM::ADDcrr_s, EraVM::ADDsrr_s, 2, 3, 0}},
      {EraVM::SELssr, {EraVM::ADDsrr_s, EraVM::ADDsrr_s, 3, 3, 0}},
  };

  DenseMap<unsigned, unsigned> Inverse{
      {EraVM::SELrrr, EraVM::SELrrr},
      {EraVM::SELrir, EraVM::SELirr},
      {EraVM::SELrcr, EraVM::SELcrr},
      {EraVM::SELrsr, EraVM::SELsrr},
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

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!MI.isPseudo())
        continue;

      if (Pseudos.count(MI.getOpcode())) {
        // Expand SELxxx pseudo into mov + cmov
        unsigned Opc = MI.getOpcode();
        DebugLoc DL = MI.getDebugLoc();

        assert(Pseudos.count(Opc) && "Unexpected instr type to insert");
        unsigned NumDefs = MI.getNumDefs();
        unsigned Src0ArgPos = NumDefs;
        unsigned Src1ArgPos = NumDefs + Pseudos[Opc][2];
        unsigned CCPos = Src1ArgPos + Pseudos[Opc][3];
        unsigned DstArgPos = CCPos + 1;
        unsigned EndPos = DstArgPos + Pseudos[Opc][4];

        bool ShouldInverse =
            Inverse.count(Opc) != 0u &&
            MI.getOperand(0).getReg() == MI.getOperand(Src0ArgPos).getReg();

        auto buildMOV = [&](unsigned OpNo, unsigned CC) {
          unsigned ArgPos = (OpNo == 0) ? Src0ArgPos : Src1ArgPos;
          unsigned NextPos = (OpNo == 0) ? Src1ArgPos : CCPos;
          bool IsReg = (NextPos - ArgPos == 1) && MI.getOperand(ArgPos).isReg();
          unsigned MovOpc = Pseudos[Opc][OpNo];
          // Avoid unconditional mov rN, rN
          if (CC == EraVMCC::COND_NONE && NumDefs && IsReg &&
              MI.getOperand(ArgPos).getReg() == MI.getOperand(0).getReg())
            return;
          auto Mov = [&]() {
            if (NumDefs)
              return BuildMI(MBB, &MI, DL, TII->get(MovOpc),
                             MI.getOperand(0).getReg());
            return BuildMI(MBB, &MI, DL, TII->get(MovOpc));
          }();
          for (unsigned i = ArgPos; i < NextPos; ++i)
            Mov.add(MI.getOperand(i));
          Mov.addReg(EraVM::R0);
          for (unsigned i = DstArgPos; i < EndPos; ++i) {
            Mov.add(MI.getOperand(i));
          }
          Mov.addImm(CC);
          return;
        };

        if (ShouldInverse) {
          buildMOV(0, EraVMCC::COND_NONE);
          buildMOV(1, InverseCond[getImmOrCImm(MI.getOperand(CCPos))]);
        } else {
          buildMOV(1, EraVMCC::COND_NONE);
          buildMOV(0, getImmOrCImm(MI.getOperand(CCPos)));
        }

        PseudoInst.push_back(&MI);
      }
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
