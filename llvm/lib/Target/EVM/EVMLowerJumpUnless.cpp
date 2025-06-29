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
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

#define DEBUG_TYPE "evm-lower-jump-unless"
#define EVM_LOWER_JUMP_UNLESS_NAME "EVM Lower jump_unless"

STATISTIC(NumPseudoJumpUnlessFolded, "Number of PseudoJUMP_UNLESS folded");

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

/// Fold `<PrevMI> ; PseudoJUMP_UNLESS` into `PseudoJUMPI`.
///
/// Supported `PrevMI` patterns and changes:
///   • `ISZERO_S` -> delete `ISZERO_S`
///   • `EQ_S`     -> change to `SUB_S`
///   • `SUB_S`    -> change to `EQ_S`
///
/// Returns `true` if any fold was performed.
static bool tryFoldJumpUnless(MachineInstr &MI, const EVMInstrInfo *TII) {
  auto I = MachineBasicBlock::iterator(&MI);
  auto *PrevMI = I == MI.getParent()->begin() ? nullptr : &*std::prev(I);
  bool CanFold = PrevMI && (PrevMI->getOpcode() == EVM::ISZERO_S ||
                            PrevMI->getOpcode() == EVM::EQ_S ||
                            PrevMI->getOpcode() == EVM::SUB_S);

  if (!CanFold)
    return false;

  ++NumPseudoJumpUnlessFolded;

  if (PrevMI->getOpcode() == EVM::ISZERO_S)
    PrevMI->eraseFromParent();
  else if (PrevMI->getOpcode() == EVM::EQ_S)
    PrevMI->setDesc(TII->get(EVM::SUB_S));
  else if (PrevMI->getOpcode() == EVM::SUB_S)
    PrevMI->setDesc(TII->get(EVM::EQ_S));
  return true;
}

/// Lower a `PseudoJUMP_UNLESS` to condition-setting + `PseudoJUMPI`.
///
/// If `FoldJumps` is enabled and the local pattern allows it, an
/// optimisation in `tryFoldJumpUnless` removes the explicit `ISZERO_S`.
/// Otherwise the pseudo-op expands to:
///     ISZERO_S
///     PseudoJUMPI
static void lowerPseudoJumpUnless(MachineInstr &MI, const EVMInstrInfo *TII,
                                  const bool IsStackified,
                                  const bool FoldJumps) {
  assert(IsStackified && "Found pseudo jump_unless in non-stackified function");
  assert(MI.getNumExplicitOperands() == 1 &&
         "Unexpected number of operands in pseudo jump_unless");

  if (!FoldJumps || !tryFoldJumpUnless(MI, TII))
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::ISZERO_S));
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::PseudoJUMPI))
      .add(MI.getOperand(0));
}

bool EVMLowerJumpUnless::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Lower jump_unless instructions **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  CodeGenOptLevel OptLevel = MF.getTarget().getOptLevel();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();
  const bool IsStackified =
      MF.getInfo<EVMMachineFunctionInfo>()->getIsStackified();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    auto TermIt = MBB.getFirstInstrTerminator();
    if (TermIt == MBB.end())
      continue;

    switch (TermIt->getOpcode()) {
    case EVM::PseudoJUMP_UNLESS:
      lowerPseudoJumpUnless(*TermIt, TII, IsStackified,
                            OptLevel != CodeGenOptLevel::None);
      break;
    case EVM::JUMP_UNLESS:
      lowerJumpUnless(*TermIt, TII, IsStackified, MRI);
      break;
    default:
      continue;
    }

    TermIt->eraseFromParent();
    Changed = true;
  }
  return Changed;
}
