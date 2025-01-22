//===----- EVMLowerJumpUnless.cpp - Lower jump_unless ----------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass lowers jump_unless into iszero and jumpi instructions.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "evm-lower-jump-unless"
#define EVM_LOWER_JUMP_UNLESS_NAME "EVM Lower jump_unless"

namespace {
class EVMLowerJumpUnless final : public MachineFunctionPass {
public:
  static char ID;

  EVMLowerJumpUnless() : MachineFunctionPass(ID) {
    initializeEVMLowerJumpUnlessPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return EVM_LOWER_JUMP_UNLESS_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

char EVMLowerJumpUnless::ID = 0;

INITIALIZE_PASS(EVMLowerJumpUnless, DEBUG_TYPE, EVM_LOWER_JUMP_UNLESS_NAME,
                false, false)

FunctionPass *llvm::createEVMLowerJumpUnless() {
  return new EVMLowerJumpUnless();
}

// Lower jump_unless into iszero and jumpi instructions. This instruction
// can only be present in non-stackified functions.
static void lowerJumpUnless(MachineInstr &MI, const EVMInstrInfo *TII,
                            const bool IsStackified, MachineRegisterInfo &MRI) {
  assert(!IsStackified && "Found jump_unless in stackified function");
  assert(MI.getNumExplicitOperands() == 2 &&
         "Unexpected number of operands in jump_unless");
  auto NewReg = MRI.createVirtualRegister(&EVM::GPRRegClass);
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::ISZERO), NewReg)
      .add(MI.getOperand(1));
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::JUMPI))
      .add(MI.getOperand(0))
      .addReg(NewReg);
}

// Lower pseudo jump_unless into iszero and jumpi instructions. This pseudo
// instruction can only be present in stackified functions.
static void lowerPseudoJumpUnless(MachineInstr &MI, const EVMInstrInfo *TII,
                                  const bool IsStackified) {
  assert(IsStackified && "Found pseudo jump_unless in non-stackified function");
  assert(MI.getNumExplicitOperands() == 1 &&
         "Unexpected number of operands in pseudo jump_unless");
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::ISZERO_S));
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::PseudoJUMPI))
      .add(MI.getOperand(0));
}

bool EVMLowerJumpUnless::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Lower jump_unless instructions **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();
  const bool IsStackified =
      MF.getInfo<EVMMachineFunctionInfo>()->getIsStackified();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (auto &MI : make_early_inc_range(MBB)) {
      if (MI.getOpcode() == EVM::PseudoJUMP_UNLESS)
        lowerPseudoJumpUnless(MI, TII, IsStackified);
      else if (MI.getOpcode() == EVM::JUMP_UNLESS)
        lowerJumpUnless(MI, TII, IsStackified, MRI);
      else
        continue;

      MI.eraseFromParent();
      Changed = true;
    }
  }
  return Changed;
}
