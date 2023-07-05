//===--- EVMOptimizeLiveIntervals.cpp - LiveInterval processing -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file optimize LiveIntervals for use in a post-RA context.
//
// LiveIntervals normally runs before register allocation when the code is
// only recently lowered out of SSA form, so it's uncommon for registers to
// have multiple defs, and when they do, the defs are usually closely related.
// Later, after coalescing, tail duplication, and other optimizations, it's
// more common to see registers with multiple unrelated defs. This pass
// updates LiveIntervals to distribute the value numbers across separate
// LiveIntervals.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "evm-optimize-live-intervals"

namespace {
class EVMOptimizeLiveIntervals final : public MachineFunctionPass {
  StringRef getPassName() const override {
    return "EVM Optimize Live Intervals";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.addPreserved<MachineBlockFrequencyInfoWrapperPass>();
    AU.addPreserved<SlotIndexesWrapperPass>();
    AU.addPreserved<LiveIntervalsWrapperPass>();
    AU.addPreservedID(LiveVariablesID);
    AU.addPreservedID(MachineDominatorsID);
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::TracksLiveness);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

public:
  static char ID; // Pass identification, replacement for typeid
  EVMOptimizeLiveIntervals() : MachineFunctionPass(ID) {}
};
} // end anonymous namespace

char EVMOptimizeLiveIntervals::ID = 0;
INITIALIZE_PASS(EVMOptimizeLiveIntervals, DEBUG_TYPE,
                "Optimize LiveIntervals for EVM", false, false)

FunctionPass *llvm::createEVMOptimizeLiveIntervals() {
  return new EVMOptimizeLiveIntervals();
}

bool EVMOptimizeLiveIntervals::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** Optimize LiveIntervals **********\n"
                       "********** Function: "
                    << MF.getName() << '\n');

  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto &LIS = getAnalysis<LiveIntervalsWrapperPass>().getLIS();

  // We don't preserve SSA form.
  MRI.leaveSSA();

  assert(MRI.tracksLiveness() && "OptimizeLiveIntervals expects liveness");

  // Split multiple-VN LiveIntervals into multiple LiveIntervals.
  SmallVector<LiveInterval *, 4> SplitLIs;
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I < E; ++I) {
    Register Reg = Register::index2VirtReg(I);

    if (MRI.reg_nodbg_empty(Reg))
      continue;

    LIS.splitSeparateComponents(LIS.getInterval(Reg), SplitLIs);
    SplitLIs.clear();
  }

  // In FixIrreducibleControlFlow, we conservatively inserted IMPLICIT_DEF
  // instructions to satisfy LiveIntervals' requirement that all uses be
  // dominated by defs. Now that LiveIntervals has computed which of these
  // defs are actually needed and which are dead, remove the dead ones.
  for (MachineInstr &MI : llvm::make_early_inc_range(MF.front())) {
    if (MI.isImplicitDef() && MI.getOperand(0).isDead()) {
      LiveInterval &LI = LIS.getInterval(MI.getOperand(0).getReg());
      LIS.removeVRegDefAt(LI, LIS.getInstructionIndex(MI).getRegSlot());
      LIS.RemoveMachineInstrFromMaps(MI);
      MI.eraseFromParent();
    }
  }

  return true;
}
