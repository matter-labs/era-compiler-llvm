
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Pre-emission peephole optimizations.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

#define DEBUG_TYPE "evm-peephole"
#define EVM_PEEPHOLE "EVM Peephole"

using namespace llvm;

namespace {
/// Perform foldings on stack-form MIR before emission.
class EVMPeephole final : public MachineFunctionPass {
public:
  static char ID;
  EVMPeephole() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return EVM_PEEPHOLE; }
  bool runOnMachineFunction(MachineFunction &MF) override;
  bool optimizeConditionaJumps(MachineBasicBlock &MBB) const;
};
} // namespace

bool EVMPeephole::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    Changed |= optimizeConditionaJumps(MBB);
  }
  return Changed;
}

static bool isNegatedAndJumpedOn(const MachineBasicBlock &MBB,
                                 MachineBasicBlock::const_iterator I) {
  if (I == MBB.end() || I->getOpcode() != EVM::ISZERO_S)
    return false;
  ++I;
  // When a conditional jump’s predicate is a (possibly nested) bitwise `or`,
  // both operands are eligible for folding. Currently we only fold the operand
  // computed last.
  // TODO: #887 Apply folding to all operands.
  while (I != MBB.end() && I->getOpcode() == EVM::OR_S)
    ++I;
  return I != MBB.end() && I->getOpcode() == EVM::PseudoJUMPI;
}

bool EVMPeephole::optimizeConditionaJumps(MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator I = MBB.begin();
  const TargetInstrInfo *TII = MBB.getParent()->getSubtarget().getInstrInfo();

  while (I != MBB.end()) {
    // Fold ISZERO ISZERO to nothing, only if it's a predicate to JUMPI.
    if (I->getOpcode() == EVM::ISZERO_S &&
        isNegatedAndJumpedOn(MBB, std::next(I))) {
      std::next(I)->eraseFromParent();
      I->eraseFromParent();
      return true;
    }

    // Fold EQ ISZERO to SUB, only if it's a predicate to JUMPI.
    if (I->getOpcode() == EVM::EQ_S &&
        isNegatedAndJumpedOn(MBB, std::next(I))) {
      I->setDesc(TII->get(EVM::SUB_S));
      std::next(I)->eraseFromParent();
      return true;
    }

    // Fold SUB ISZERO to EQ, only if it's a predicate to JUMPI.
    if (I->getOpcode() == EVM::SUB_S &&
        isNegatedAndJumpedOn(MBB, std::next(I))) {
      I->setDesc(TII->get(EVM::EQ_S));
      std::next(I)->eraseFromParent();
      return true;
    }

    ++I;
  }
  return false;
}

char EVMPeephole::ID = 0;

INITIALIZE_PASS(EVMPeephole, DEBUG_TYPE, EVM_PEEPHOLE, false, false)

FunctionPass *llvm::createEVMPeepholePass() { return new EVMPeephole(); }
