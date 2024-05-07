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

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return ERAVM_TIE_SELECT_OPERANDS_NAME;
  }

private:
  /// If MI is a pseudo SELrrr instruction (which is the most common case),
  /// then try to ask RA to coalesce an input register with the output, so that
  /// EraVMExpandSelectPass can have better results.
  /// \par Arg which argument to tie (in0 or in1).
  bool tryPlacingTie(MachineInstr &MI, EraVM::ArgumentKind Arg) const;
};

char EraVMTieSelectOperands::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMTieSelectOperands, DEBUG_TYPE,
                ERAVM_TIE_SELECT_OPERANDS_NAME, false, false)

/// Try to create an implicit tie so that the register allocator can coalesce
/// both registers.
/// Return true if we managed to do so.
bool EraVMTieSelectOperands::tryPlacingTie(MachineInstr &MI,
                                           EraVM::ArgumentKind Arg) const {
  assert(Arg == EraVM::ArgumentKind::In0 || Arg == EraVM::ArgumentKind::In1);
  // Given that we've already made constraints on the case of single
  // register operand SELECTs, we will now focus only on the case of two
  // register selects. Also note that majority of the opportunities come
  // from reg-reg selects.
  if (MI.getOpcode() != EraVM::SELrrr)
    return false;

  // Skip if the output register is already tied to an input register.
  if (MI.isRegTiedToUseOperand(0))
    return false;

  MachineOperand &Opnd = (Arg == EraVM::ArgumentKind::In0)
                             ? *EraVM::in0Iterator(MI)
                             : *EraVM::in1Iterator(MI);
  if (!Opnd.getReg().isVirtual() || !Opnd.isKill())
    return false;

  // Cannot tie if output is physical register.
  if (EraVM::out0Iterator(MI)->getReg().isPhysical())
    return false;

  // Add an implicit tie.
  MI.addOperand(MachineOperand::CreateReg(Opnd.getReg(), /*isDef=*/false,
                                          /*isImp=*/true, /*isKill=*/true));
  MI.tieOperands(0, MI.getNumOperands() - 1);

  return true;
}

bool EraVMTieSelectOperands::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM Tie Select Operands **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (tryPlacingTie(MI, EraVM::ArgumentKind::In0) ||
          tryPlacingTie(MI, EraVM::ArgumentKind::In1))
        Changed = true;

  return Changed;
}

/// createEraVMTieSelectOperandsPass - returns an instance of the Tie Select
/// Operands pass
FunctionPass *llvm::createEraVMTieSelectOperandsPass() {
  return new EraVMTieSelectOperands();
}
