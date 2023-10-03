//===-- EraVMTieSelectOperands.cpp - Tie operands for select ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass to tie the input register of a SELECT instruction
// to the output register if the source is kill, so that the register allocator
// can coalesce them.
// This pass itself does not yield any benefit, but in synergy with
// EraVMExpandSelectPass, it can reduce the number of expanded instructions.
// Note that this pass requires up-to-date liveness information to work well.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-tie-select-opnds"
#define ERAVM_TIE_SELECT_OPERANDS_NAME "EraVM tie select operands pass"

namespace {

class EraVMTieSelectOperands : public MachineFunctionPass {
public:
  static char ID;
  EraVMTieSelectOperands() : MachineFunctionPass(ID) {
    initializeEraVMTieSelectOperandsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return ERAVM_TIE_SELECT_OPERANDS_NAME;
  }

private:
  /// if MI is a pseudo SELrrr instruction (which is the most common case),
  /// then try to ask RA to coalesce an input register with the output, so that
  /// EraVMExpandSelectPass can have better results.
  /// \par Arg which argument to tie (in0 or in1).
  bool tryPlacingTie(MachineInstr &MI, EraVM::ArgumentKind Arg) const;
};

char EraVMTieSelectOperands::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMTieSelectOperands, DEBUG_TYPE,
                ERAVM_TIE_SELECT_OPERANDS_NAME, false, false)

/// try to emit a new select instruction which contains an implicit tie
/// so that the register allocator can coalesce both registers.
/// Return true if a new instruction was emitted.
bool EraVMTieSelectOperands::tryPlacingTie(MachineInstr &MI,
                                           EraVM::ArgumentKind Arg) const {
  assert(Arg == EraVM::ArgumentKind::In0 || Arg == EraVM::ArgumentKind::In1);
  // Given that we've already made constraints on the case of single
  // register operand SELECTs, we will now focus only on the case of two
  // register selects. Also note that majority of the opportunities come
  // from reg-reg selects.
  if (MI.getOpcode() != EraVM::SELrrr)
    return false;

  MachineOperand &In0Opnd = *EraVM::in0Iterator(MI);
  const Register In0Reg = In0Opnd.getReg();
  MachineOperand &In1Opnd = *EraVM::in1Iterator(MI);
  const Register In1Reg = In1Opnd.getReg();
  const Register Out0Reg = EraVM::out0Iterator(MI)->getReg();
  const auto CC = getImmOrCImm(*EraVM::ccIterator(MI));

  MachineOperand &Opnd = (Arg == EraVM::ArgumentKind::In0) ? In0Opnd : In1Opnd;
  if (!Opnd.getReg().isVirtual() || !Opnd.isKill())
    return false;

  // Cannot tie of both are physical registers
  if (Out0Reg.isPhysical() && Opnd.getReg().isPhysical())
    return false;

  // place an implicit tie with the killed input and the output, if
  // either one of the input is kill
  auto NewMI =
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), MI.getDesc(), Out0Reg)
          .addReg(In0Reg)
          .addReg(In1Reg)
          .addImm(CC);

  // restore kill flags
  if (Arg == EraVM::ArgumentKind::In0) {
    NewMI->getOperand(1).setIsKill();
    NewMI->getOperand(2).setIsKill(In1Opnd.isKill());
  } else {
    NewMI->getOperand(1).setIsKill(In0Opnd.isKill());
    NewMI->getOperand(2).setIsKill();
  }

  // Add an implicit tie
  Opnd.setImplicit();
  NewMI.add(Opnd);
  NewMI->tieOperands(0, NewMI->getNumOperands() - 1);

  return true;
}

bool EraVMTieSelectOperands::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM Tie Select Operands **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  std::vector<MachineInstr *> ToBeRemoved;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (tryPlacingTie(MI, EraVM::ArgumentKind::In0) || tryPlacingTie(MI, EraVM::ArgumentKind::In1))
        ToBeRemoved.push_back(&MI);

  for (MachineInstr *MI : ToBeRemoved)
    MI->eraseFromParent();

  return ToBeRemoved.size() > 0;
}

/// createEraVMTieSelectOperandsPass - returns an instance of the Tie Select
/// Operands pass
FunctionPass *llvm::createEraVMTieSelectOperandsPass() {
  return new EraVMTieSelectOperands();
}
