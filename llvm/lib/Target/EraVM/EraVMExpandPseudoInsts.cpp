//===-- EraVMExpandPseudoInsts.cpp - Expand pseudo instructions -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-pseudo"
#define ERAVM_EXPAND_PSEUDO_NAME "EraVM expand pseudo instructions"

namespace {

class EraVMExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  EraVMExpandPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_EXPAND_PSEUDO_NAME; }

private:
  void expandConst(MachineInstr &MI) const;
  void expandLoadConst(MachineInstr &MI) const;
  void expandThrow(MachineInstr &MI) const;
  const TargetInstrInfo *TII{};
  LLVMContext *Context{};
};

char EraVMExpandPseudo::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMExpandPseudo, DEBUG_TYPE, ERAVM_EXPAND_PSEUDO_NAME, false,
                false)

void EraVMExpandPseudo::expandConst(MachineInstr &MI) const {
  MachineOperand Constant = MI.getOperand(1);
  MachineOperand Reg = MI.getOperand(0);
  assert((Constant.isImm() || Constant.isCImm()) && "Unexpected operand type");
  const APInt &Val = Constant.isCImm() ? Constant.getCImm()->getValue()
                                       : APInt(256, Constant.getImm(), true);
  // big immediate or negative values are loaded from constant pool
  assert(Val.isIntN(16) && !Val.isNegative());
  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(EraVM::ADDirr_s))
      .add(Reg)
      .addReg(EraVM::R0)
      .addCImm(ConstantInt::get(*Context, Val))
      .addImm(0);
}

void EraVMExpandPseudo::expandLoadConst(MachineInstr &MI) const {
  MachineOperand ConstantPool = MI.getOperand(1);
  MachineOperand Reg = MI.getOperand(0);

  auto can_combine = [](MachineInstr &cur, MachineInstr &next) {
    auto opcode = next.getOpcode();
    switch (opcode) {
    default: {
      break;
    }
    // this handles commutative cases
    case EraVM::ADDrrr_s:
    case EraVM::ANDrrr_s:
    case EraVM::XORrrr_s:
    case EraVM::ORrrr_s: {
      auto outReg = cur.getOperand(0).getReg();
      if (next.getOperand(1).getReg() == outReg ||
          next.getOperand(2).getReg() == outReg) {
        return true;
      }
      break;
    }
    }
    return false;
  };

  auto can_non_commute_combine = [](MachineInstr &cur, MachineInstr &next) {
    auto opcode = next.getOpcode();
    switch (opcode) {
    default: {
      break;
    }
    // this handles commutative cases
    case EraVM::SUBrrr_s:
    case EraVM::SHLrrr_s:
    case EraVM::SHRrrr_s:
    case EraVM::ROLrrr_s:
    case EraVM::RORrrr_s: {
      auto outReg = cur.getOperand(0).getReg();
      if (next.getOperand(1).getReg() == outReg ||
          next.getOperand(2).getReg() == outReg) {
        return true;
      }
      break;
    }
    }
    return false;
  };

  auto get_crr_op = [](auto opcode, bool reverse = false) {
    switch (opcode) {
    default: {
      llvm_unreachable("wrong opcode");
      break;
    }
    case EraVM::ADDrrr_s: {
      return EraVM::ADDcrr_s;
    }
    case EraVM::ANDrrr_s: {
      return EraVM::ANDcrr_s;
    }
    case EraVM::XORrrr_s: {
      return EraVM::XORcrr_s;
    }
    case EraVM::ORrrr_s: {
      return EraVM::ORcrr_s;
    }
    case EraVM::SUBrrr_s: {
      return reverse ? EraVM::SUByrr_s : EraVM::SUBcrr_s;
    }
    case EraVM::SHLrrr_s: {
      return reverse ? EraVM::SHLyrr_s : EraVM::SHLcrr_s;
    }
    case EraVM::SHRrrr_s: {
      return reverse ? EraVM::SHRyrr_s : EraVM::SHRcrr_s;
    }
    case EraVM::ROLrrr_s: {
      return reverse ? EraVM::ROLyrr_s : EraVM::ROLcrr_s;
    }
    case EraVM::RORrrr_s: {
      return reverse ? EraVM::RORyrr_s : EraVM::RORrrr_s;
    }
    }
  };

  // it is possible that we can merge two instructions, as long as we do not
  // call a scheduler the materialization of a const will be followed by its
  // use.
  auto MBBI = std::next(MachineBasicBlock::iterator(MI));
  auto outReg = MI.getOperand(0).getReg();
  if (can_combine(MI, *MBBI)) {
    auto opcode = MBBI->getOpcode();
    auto other_op = get_crr_op(opcode);

    auto outReg = MI.getOperand(0).getReg();
    auto otherReg = MBBI->getOperand(1).getReg() == outReg
                        ? MBBI->getOperand(2)
                        : MBBI->getOperand(1);

    BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(other_op))
        .add(MBBI->getOperand(0))
        .addImm(0)
        .add(ConstantPool)
        .add(otherReg)
        .addImm(0);
    MBBI->eraseFromParent();
    return;
  }

  if (can_non_commute_combine(MI, *MBBI)) {
    auto opcode = MBBI->getOpcode();

    bool reverse;
    MachineOperand *otherOpnd;
    if (MBBI->getOperand(1).getReg() == outReg) {
      reverse = false;
      otherOpnd = &MBBI->getOperand(2);
    } else {
      assert(MBBI->getOperand(2).getReg() == outReg);
      reverse = true;
      otherOpnd = &MBBI->getOperand(1);
    }
    auto other_op = get_crr_op(opcode, reverse);

    BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(other_op))
        .add(MBBI->getOperand(0))
        .addImm(0)
        .add(ConstantPool)
        .add(*otherOpnd)
        .addImm(0);
    MBBI->eraseFromParent();
    return;
  }

  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(EraVM::ADDcrr_s))
      .add(Reg)
      .addImm(0)
      .add(ConstantPool)
      .addReg(EraVM::R0)
      .addImm(0)
      .getInstr();
}

bool EraVMExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
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

      if (MI.getOpcode() == EraVM::INVOKE) {
        // convert INVOKE to an actual call
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(EraVM::NEAR_CALL))
            .addReg(EraVM::R0)
            .add(MI.getOperand(0))
            .add(MI.getOperand(1));
        PseudoInst.push_back(&MI);
        continue;
      }

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
          for (unsigned i = DstArgPos; i < EndPos; ++i)
            Mov.add(MI.getOperand(i));
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
        // SEL src0 -> CMOV src0
        for (unsigned i = Src0ArgPos; i < Src1ArgPos; ++i)
          CMov.add(MI.getOperand(i));
        // r0 -> CMOV src1
        CMov.addReg(EraVM::R0);
        // SEL dst -> CMOV dst
        for (unsigned i = DstArgPos; i < EndPos; ++i)
          CMov.add(MI.getOperand(i));
        // SEL cc -> CMOV cc
        CMov.add(MI.getOperand(CCPos));

        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == EraVM::CONST) {
        expandConst(MI);
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == EraVM::LOADCONST) {
        expandLoadConst(MI);
        PseudoInst.push_back(&MI);
      }
    }

  // Handle calls
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == EraVM::INVOKE) {
        // convert INVOKE to an actual near_call
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(EraVM::NEAR_CALL))
            .addReg(EraVM::R0)
            .add(MI.getOperand(0))
            .add(MI.getOperand(1));
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == EraVM::CALL) {
        // Special handling of calling to __cxa_throw.
        // If we are calling into the throw wrapper function, we jump into a
        // local frame with unwind path of `DEFAULT_UNWIND`, which will turn
        // our prepared THROW into a PANIC. This will cause values in registers
        // not propagated back to upper level, causing lost of returndata
        auto *func_opnd = MI.getOperand(0).getGlobal();
        auto func_name = func_opnd->getName();
        if (func_name == "__cxa_throw") {
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                  TII->get(EraVM::THROW));
        } else {
          // One of the problem: the backend cannot restrict frontend to not
          // emit calls (Should we reinforce it?) so this route is needed. If a
          // call is generated, it is incomplete as it misses EH label info, pad
          // 0 instead.
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                  TII->get(EraVM::NEAR_CALL))
              .addReg(EraVM::R0)
              .add(MI.getOperand(0))
              .addExternalSymbol(
                  "DEFAULT_UNWIND"); // Linker inserts a basic block
                                     // which bubbles up the exception.
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
FunctionPass *llvm::createEraVMExpandPseudoPass() {
  return new EraVMExpandPseudo();
}
