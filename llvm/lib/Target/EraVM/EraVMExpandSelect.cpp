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
      dbgs() << "********** EraVM EXPAND PSEUDO INSTRUCTIONS **********\n"
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

      {EraVM::SELrrs, {EraVM::ADDrrs_s, EraVM::ADDrrs_s, 1, 1, 3}},
      {EraVM::SELirs, {EraVM::ADDirs_s, EraVM::ADDrrs_s, 1, 1, 3}},
      {EraVM::SELcrs, {EraVM::ADDcrs_s, EraVM::ADDrrs_s, 2, 1, 3}},
      {EraVM::SELsrs, {EraVM::ADDsrs_s, EraVM::ADDrrs_s, 3, 1, 3}},
      {EraVM::SELris, {EraVM::ADDrrs_s, EraVM::ADDirs_s, 1, 1, 3}},
      {EraVM::SELiis, {EraVM::ADDirs_s, EraVM::ADDirs_s, 1, 1, 3}},
      {EraVM::SELcis, {EraVM::ADDcrs_s, EraVM::ADDirs_s, 2, 1, 3}},
      {EraVM::SELsis, {EraVM::ADDsrs_s, EraVM::ADDirs_s, 3, 1, 3}},
      {EraVM::SELrcs, {EraVM::ADDrrs_s, EraVM::ADDcrs_s, 1, 2, 3}},
      {EraVM::SELics, {EraVM::ADDirs_s, EraVM::ADDcrs_s, 1, 2, 3}},
      {EraVM::SELccs, {EraVM::ADDcrs_s, EraVM::ADDcrs_s, 2, 2, 3}},
      {EraVM::SELscs, {EraVM::ADDsrs_s, EraVM::ADDcrs_s, 3, 2, 3}},
      {EraVM::SELrss, {EraVM::ADDrrs_s, EraVM::ADDsrs_s, 1, 3, 3}},
      {EraVM::SELiss, {EraVM::ADDirs_s, EraVM::ADDsrs_s, 1, 3, 3}},
      {EraVM::SELcss, {EraVM::ADDcrs_s, EraVM::ADDsrs_s, 2, 3, 3}},
      {EraVM::SELsss, {EraVM::ADDsrs_s, EraVM::ADDsrs_s, 3, 3, 3}},
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
        unsigned MovOpc = Pseudos[Opc][1];
        unsigned CMovOpc = Pseudos[Opc][0];
        unsigned Src0ArgPos = NumDefs;
        unsigned Src1ArgPos = NumDefs + Pseudos[Opc][2];
        unsigned CCPos = Src1ArgPos + Pseudos[Opc][3];
        unsigned DstArgPos = CCPos + 1;
        unsigned EndPos = DstArgPos + Pseudos[Opc][4];

        // Avoid mov rN, rN
        if (NumDefs != 1 || CCPos - Src1ArgPos != 1 ||
            !MI.getOperand(Src1ArgPos).isReg() ||
            MI.getOperand(0).getReg() != MI.getOperand(Src1ArgPos).getReg()) {
          // unconditional mov
          auto Mov = [&]() {
            if (NumDefs)
              return BuildMI(MBB, &MI, DL, TII->get(MovOpc),
                             MI.getOperand(0).getReg());
            return BuildMI(MBB, &MI, DL, TII->get(MovOpc));
          }();
          // SEL src1 -> MOV src0
          for (unsigned i = Src1ArgPos; i < CCPos; ++i)
            Mov.add(MI.getOperand(i));
          // r0 -> MOV src1
          Mov.addReg(EraVM::R0);
          // SEL dst -> MOV dst
          for (unsigned i = DstArgPos; i < EndPos; ++i) {
            Mov.add(MI.getOperand(i));
          }
          // COND_NONE -> MOV cc
          Mov.addImm(EraVMCC::COND_NONE);
        }

        // condtional mov
        auto CMov = [&]() {
          if (NumDefs)
            return BuildMI(MBB, &MI, DL, TII->get(CMovOpc),
                           MI.getOperand(0).getReg());
          return BuildMI(MBB, &MI, DL, TII->get(CMovOpc));
        }();
        // early clobber the definition:
        if (NumDefs) {
          CMov->getOperand(0).setIsEarlyClobber(true);
        }
        // SEL src0 -> CMOV src0
        for (unsigned i = Src0ArgPos; i < Src1ArgPos; ++i)
          CMov.add(MI.getOperand(i));
        // r0 -> CMOV src1
        CMov.addReg(EraVM::R0);
        // SEL dst -> CMOV dst
        for (unsigned i = DstArgPos; i < EndPos; ++i) {
          CMov.add(MI.getOperand(i));
        }
        // SEL cc -> CMOV cc
        CMov.add(MI.getOperand(CCPos));

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
