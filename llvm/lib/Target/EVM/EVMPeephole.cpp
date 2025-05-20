//===----- EVMPeephole.cpp - Peephole Optimization Pass --------*- C++ -*--===//
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

#define DEBUG_TYPE "evm-peephole"
#define EVM_PEEPHOLE "EVM Peephole"

using namespace llvm;

namespace {
class EVMPeephole final : public MachineFunctionPass {
public:
  static char ID;
  EVMPeephole() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "Remove Redundant ISZERO"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF)
      Changed |= runOnMachineBasicBlock(MBB);

    return Changed;
  }

  bool runOnMachineBasicBlock(MachineBasicBlock &MBB) {
    bool Changed = false;
    MachineBasicBlock::iterator I = MBB.begin();

    while (I != MBB.end()) {
      // Fold ISZERO ISZERO to nothing.
      if (I->getOpcode() == EVM::ISZERO_S) {
        auto NextI = std::next(I);
        if (NextI != MBB.end() && NextI->getOpcode() == EVM::ISZERO_S) {
          ++NextI;
          std::next(I)->eraseFromParent();
          I->eraseFromParent();
          I = NextI;
          Changed = true;
          continue;
        }
      }
      ++I;
    }
    return Changed;
  }
};
} // namespace

char EVMPeephole::ID = 0;

INITIALIZE_PASS(EVMPeephole, DEBUG_TYPE, EVM_PEEPHOLE, false, false)

FunctionPass *llvm::createEVMPeepholePass() { return new EVMPeephole(); }
