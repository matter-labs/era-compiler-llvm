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
  /// If MI is a pseudo select instruction (which is the most common case),
  /// then try to ask RA to coalesce an input register with the output, so that
  /// EraVMExpandSelectPass can have better results.
  /// \par Arg which argument to tie (in0 or in1).
  bool tryPlacingTie(MachineInstr &MI, EraVM::ArgumentKind Arg) const;

  /// Return true if we can place a tie for the given instruction and argument.
  bool canPlaceTie(MachineInstr &MI, EraVM::ArgumentKind Arg) const;
};

char EraVMTieSelectOperands::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMTieSelectOperands, DEBUG_TYPE,
                ERAVM_TIE_SELECT_OPERANDS_NAME, false, false)

bool EraVMTieSelectOperands::canPlaceTie(MachineInstr &MI,
                                         EraVM::ArgumentKind Arg) const {
  if (!EraVM::isSelect(MI) || MI.getOpcode() == EraVM::FATPTR_SELrrr)
    return false;

  return EraVM::argumentType(Arg, MI.getOpcode()) ==
         EraVM::ArgumentType::Register;
}

/// Try to create an implicit tie so that the register allocator can coalesce
/// both registers.
/// Return true if we managed to do so.
bool EraVMTieSelectOperands::tryPlacingTie(MachineInstr &MI,
                                           EraVM::ArgumentKind Arg) const {
  assert(Arg == EraVM::ArgumentKind::In0 || Arg == EraVM::ArgumentKind::In1);

  // Bail out if we cannot place a tie for the given instruction.
  if (!canPlaceTie(MI, Arg))
    return false;

  // Skip if the output register is already tied to an input register.
  if (MI.isRegTiedToUseOperand(0))
    return false;

  // Normally we reverse condition to optimize if we can place tie to the first
  // argument. But for overflow LT a.k.a COND_OF caused by uaddo and umulo, we
  // shouldn't do so, because GE is inappropriate for being used as reversal
  // condition code, therefore we only attempt to put tie to the 2nd argument
  // when it is a register operand. Please be noted that for normal condition
  // code and overflow LT caused by usubo, there is no such restriction.
  const auto CC = getImmOrCImm(*EraVM::ccIterator(MI));
  if (CC == EraVMCC::COND_OF && Arg == EraVM::ArgumentKind::In0)
    return false;

  MachineOperand &Opnd = (Arg == EraVM::ArgumentKind::In0)
                             ? *EraVM::in0Iterator(MI)
                             : *EraVM::in1Iterator(MI);
  if (!Opnd.getReg().isVirtual() || !Opnd.isKill())
    return false;

  // Cannot tie if output is physical register.
  if (EraVM::out0Iterator(MI)->getReg().isPhysical())
    return false;

  // Since RegisterCoalescer works well when we have both early clobber and
  // explicit tied register, but not with implicit tied register, remove the
  // early clobber flag so RegisterCoalescer can remove unneeded copies.
  // This is safe to do because the regalloc will allocate the same register
  // for the output and input register operand, so if there is a register in
  // code operand, it won't be overwritten with the output register and we can
  // do proper expansion PostRA.
  if (EraVM::out0Iterator(MI)->isEarlyClobber())
    EraVM::out0Iterator(MI)->setIsEarlyClobber(false);

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
