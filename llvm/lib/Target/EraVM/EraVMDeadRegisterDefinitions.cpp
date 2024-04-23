//===-- EraVMDeadRegisterDefinitions.cpp - Replace dead defs ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass rewrites def regs to R0 for instrs whose return values are unused.
// In high register pressure cases, this can help to reduce spills that were
// previously needed to free up register so it can be used as a dead result of
// instruction whose output is not used. Also, in some cases regalloc can assign
// registers in such way that lowering of select instruction can be done in one
// instruction while inverting condition:
//   Before:
//      add stack-[19], r0, r3
//      sub.s! @CPI3_11[0], r3, r2
//      add @CPI3_11[0], r0, r2
//      add.lt r3, r0, r2
//   After:
//      add stack-[19], r0, r2
//      sub.s! @CPI3_11[0], r2, r0
//      add.ge @CPI3_11[0], r0, r2
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMInstrInfo.h"
#include "llvm/ADT/Statistic.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-dead-defs"
#define ERAVM_DEAD_REG_DEF_NAME "EraVM dead register definitions"

STATISTIC(NumDeadDefsReplaced, "Number of dead definitions replaced");

namespace {

class EraVMDeadRegisterDefinitions : public MachineFunctionPass {
public:
  static char ID;
  EraVMDeadRegisterDefinitions() : MachineFunctionPass(ID) {
    initializeEraVMDeadRegisterDefinitionsPass(
        *PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_DEAD_REG_DEF_NAME; }
};

char EraVMDeadRegisterDefinitions::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMDeadRegisterDefinitions, DEBUG_TYPE,
                ERAVM_DEAD_REG_DEF_NAME, false, false)

bool EraVMDeadRegisterDefinitions::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM DEAD REGISTER DEFINITIONS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  MachineRegisterInfo *RegInfo = &MF.getRegInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // We only handle EraVM specific instructions.
      if (MI.getOpcode() <= TargetOpcode::GENERIC_OP_END)
        continue;

      for (unsigned I = 0, E = MI.getNumExplicitDefs(); I != E; ++I) {
        MachineOperand &MO = MI.getOperand(I);
        if (!MO.isReg() || !MO.isDef() || MO.isEarlyClobber())
          continue;

        // Skip tied operands.
        if (MI.isRegTiedToUseOperand(I))
          continue;

        // Just check for virtual registers that are dead.
        if (!MO.getReg().isVirtual() ||
            (!MO.isDead() && !RegInfo->use_nodbg_empty(MO.getReg())))
          continue;

        LLVM_DEBUG(dbgs() << "== Dead def operand in:"; MI.dump());
        MO.setReg(EraVM::R0);
        MO.setIsDead();
        LLVM_DEBUG(dbgs() << "     Replacing with R0:"; MI.dump());
        ++NumDeadDefsReplaced;
        Changed = true;
      }
    }
  }
  return Changed;
}

/// createEraVMDeadRegisterDefinitionsOperandsPass - returns an instance of the
/// dead register definitions pass.
FunctionPass *llvm::createEraVMDeadRegisterDefinitionsPass() {
  return new EraVMDeadRegisterDefinitions();
}
