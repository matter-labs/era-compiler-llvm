//===---------- EVMArgumentMove.cpp - Argument instruction moving ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file moves and orders ARGUMENT instructions after ScheduleDAG
// scheduling.
//
// Arguments are really live-in registers, however, since we use virtual
// registers and LLVM doesn't support live-in virtual registers, we're
// currently making do with ARGUMENT instructions which are placed at the top
// of the entry block. The trick is to get them to *stay* at the top of the
// entry block.
//
// The ARGUMENTS physical register keeps these instructions pinned in place
// during liveness-aware CodeGen passes, however one thing which does not
// respect this is the ScheduleDAG scheduler. This pass is therefore run
// immediately after that.
//
// This is all hopefully a temporary solution until we find a better solution
// for describing the live-in nature of arguments.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "evm-argument-move"

namespace {
class EVMArgumentMove final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  EVMArgumentMove() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM Argument Move"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addPreserved<MachineBlockFrequencyInfo>();
    AU.addPreservedID(MachineDominatorsID);
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

char EVMArgumentMove::ID = 0;
INITIALIZE_PASS(EVMArgumentMove, DEBUG_TYPE,
                "Move ARGUMENT instructions for EVM", false, false)

FunctionPass *llvm::createEVMArgumentMove() { return new EVMArgumentMove(); }

bool EVMArgumentMove::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Argument Move **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  bool Changed = false;
  MachineBasicBlock &EntryMBB = MF.front();
  SmallVector<MachineInstr *> Args;
  for (MachineInstr &MI : EntryMBB) {
    if (EVM::ARGUMENT == MI.getOpcode())
      Args.push_back(&MI);
  }

  // Sort ARGUMENT instructions in ascending order of their arguments.
  std::sort(Args.begin(), Args.end(),
            [](const MachineInstr *MI1, const MachineInstr *MI2) {
              int64_t Arg1Idx = MI1->getOperand(1).getImm();
              int64_t Arg2Idx = MI2->getOperand(1).getImm();
              return Arg1Idx < Arg2Idx;
            });

  for (MachineInstr *MI : reverse(Args)) {
    MachineInstr *Arg = MI->removeFromParent();
    EntryMBB.insert(EntryMBB.begin(), Arg);
    Changed = true;
  }
  return Changed;
}
