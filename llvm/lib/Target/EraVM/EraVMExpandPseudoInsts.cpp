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
  EraVMExpandPseudo() : MachineFunctionPass(ID) {
    initializeEraVMExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_EXPAND_PSEUDO_NAME; }

private:
  const TargetInstrInfo *TII{};
  LLVMContext *Context{};
};

char EraVMExpandPseudo::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMExpandPseudo, DEBUG_TYPE, ERAVM_EXPAND_PSEUDO_NAME,
                false, false)

bool EraVMExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> PseudoInst;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == EraVM::INVOKE) {
        // convert INVOKE to an actual near_call
        Register ABIReg = MI.getOperand(0).getReg();
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(EraVM::NEAR_CALL))
            .addReg(ABIReg)
            .add(MI.getOperand(1))
            .add(MI.getOperand(2))
            .copyImplicitOps(MI);
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == EraVM::CALL) {
        // Special handling of calling to __cxa_throw.
        // If we are calling into the throw wrapper function, we jump into a
        // local frame with unwind path of `DEFAULT_UNWIND`, which will turn
        // our prepared THROW into a PANIC. This will cause values in registers
        // not propagated back to upper level, causing lost of returndata
        const auto *func_opnd = MI.getOperand(1).getGlobal();
        auto func_name = func_opnd->getName();
        if (func_name == "__cxa_throw") {
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                  TII->get(EraVM::THROW)).addReg(EraVM::R1)
            .copyImplicitOps(MI);
        } else {
          // One of the problem: the backend cannot restrict frontend to not
          // emit calls (Should we reinforce it?) so this route is needed. If a
          // call is generated, it is incomplete as it misses EH label info, pad
          // 0 instead.
          Register ABIReg = MI.getOperand(0).getReg();
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                  TII->get(EraVM::NEAR_CALL))
              .addReg(ABIReg)
              .add(MI.getOperand(1))
              .addExternalSymbol(
                  "DEFAULT_UNWIND")  // Linker inserts a basic block
                                     // which bubbles up the exception.
              .copyImplicitOps(MI);
        }

        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == EraVM::PTR_TO_INT) {
        // Eliminate PTR_TO_INT instruction.
        assert(MI.getOperand(0).getReg() == MI.getOperand(1).getReg() &&
               "Src and dst registers are not the same in PTR_TO_INT");
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == EraVM::LOADCONST) {
      // expand load const
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(EraVM::ADDcrr_s))
            .add(MI.getOperand(0))
            .addImm(0)
            .add(MI.getOperand(1))
            .addReg(EraVM::R0)
            .addImm(0)
            .getInstr();
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
