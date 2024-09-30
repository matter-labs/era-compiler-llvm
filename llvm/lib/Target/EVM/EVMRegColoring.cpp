//===---------- EVMRegColoring.cpp - Register coloring -----*- C++ -*------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a virtual register coloring pass.
//
// EVM doesn't have a fixed number of registers, but it is still
// desirable to minimize the total number of registers used in each function.
//
// This code is modeled after lib/CodeGen/StackSlotColoring.cpp.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "evm-reg-coloring"

namespace {
class EVMRegColoring final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  EVMRegColoring() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM Register Coloring"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<LiveIntervals>();
    AU.addRequired<MachineBlockFrequencyInfo>();
    AU.addPreserved<MachineBlockFrequencyInfo>();
    AU.addPreservedID(MachineDominatorsID);
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
};
} // end anonymous namespace

char EVMRegColoring::ID = 0;
INITIALIZE_PASS(EVMRegColoring, DEBUG_TYPE, "Minimize number of registers used",
                false, false)

FunctionPass *llvm::createEVMRegColoring() { return new EVMRegColoring(); }

// Compute the total spill weight for VReg.
static float computeWeight(const MachineRegisterInfo *MRI,
                           const MachineBlockFrequencyInfo *MBFI,
                           unsigned VReg) {
  float Weight = 0.0F;
  for (MachineOperand &MO : MRI->reg_nodbg_operands(VReg))
    Weight += LiveIntervals::getSpillWeight(MO.isDef(), MO.isUse(), MBFI,
                                            *MO.getParent());
  return Weight;
}

bool EVMRegColoring::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Register Coloring **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  if (MF.exposesReturnsTwice())
    return false;

  MachineRegisterInfo *MRI = &MF.getRegInfo();
  LiveIntervals *Liveness = &getAnalysis<LiveIntervals>();
  const MachineBlockFrequencyInfo *MBFI =
      &getAnalysis<MachineBlockFrequencyInfo>();
  EVMMachineFunctionInfo &MFI = *MF.getInfo<EVMMachineFunctionInfo>();

  // Gather all register intervals into a list and sort them.
  unsigned NumVRegs = MRI->getNumVirtRegs();
  SmallVector<LiveInterval *, 0> SortedIntervals;
  SortedIntervals.reserve(NumVRegs);

  LLVM_DEBUG(dbgs() << "Interesting register intervals:\n");
  for (unsigned I = 0; I < NumVRegs; ++I) {
    Register VReg = Register::index2VirtReg(I);
    if (MFI.isVRegStackified(VReg))
      continue;
    //  Skip unused registers, which can use $drop.
    if (MRI->use_empty(VReg))
      continue;

    LiveInterval *LI = &Liveness->getInterval(VReg);
    assert(LI->weight() == 0.0F);
    LI->setWeight(computeWeight(MRI, MBFI, VReg));
    LLVM_DEBUG(LI->dump());
    SortedIntervals.push_back(LI);
  }
  LLVM_DEBUG(dbgs() << '\n');

  // Sort them to put arguments first (since we don't want to rename live-in
  // registers), by weight next, and then by position.
  // TODO: Investigate more intelligent sorting heuristics. For starters, we
  // should try to coalesce adjacent live intervals before non-adjacent ones.
  llvm::sort(SortedIntervals, [MRI](LiveInterval *LHS, LiveInterval *RHS) {
    if (MRI->isLiveIn(LHS->reg()) != MRI->isLiveIn(RHS->reg()))
      return MRI->isLiveIn(LHS->reg());
    if (LHS->weight() != RHS->weight())
      return LHS->weight() > RHS->weight();
    if (LHS->empty() || RHS->empty())
      return !LHS->empty() && RHS->empty();
    return *LHS < *RHS;
  });

  LLVM_DEBUG(dbgs() << "Coloring register intervals:\n");
  SmallVector<unsigned, 16> SlotMapping(SortedIntervals.size(), -1U);
  SmallVector<SmallVector<LiveInterval *, 4>, 16> Assignments(
      SortedIntervals.size());
  BitVector UsedColors(SortedIntervals.size());
  bool Changed = false;
  for (size_t I = 0, E = SortedIntervals.size(); I < E; ++I) {
    LiveInterval *LI = SortedIntervals[I];
    Register Old = LI->reg();
    size_t Color = I;
    const TargetRegisterClass *RC = MRI->getRegClass(Old);

    // Check if it's possible to reuse any of the used colors.
    if (!MRI->isLiveIn(Old))
      for (unsigned C : UsedColors.set_bits()) {
        if (MRI->getRegClass(SortedIntervals[C]->reg()) != RC)
          continue;
        for (LiveInterval *OtherLI : Assignments[C])
          if ((!OtherLI->empty() && OtherLI->overlaps(*LI)) ||
              Liveness->intervalIsInOneMBB(*OtherLI) !=
                  Liveness->intervalIsInOneMBB(*LI))
            goto continue_outer;
        Color = C;
        break;
      continue_outer:;
      }

    Register New = SortedIntervals[Color]->reg();
    SlotMapping[I] = New;
    Changed |= Old != New;
    UsedColors.set(Color);
    Assignments[Color].push_back(LI);

    LLVM_DEBUG(dbgs() << "Assigning vreg" << Register::virtReg2Index(LI->reg())
                      << " to vreg" << Register::virtReg2Index(New) << "\n");
  }
  if (!Changed)
    return false;

  // Rewrite register operands.
  for (size_t I = 0, E = SortedIntervals.size(); I < E; ++I) {
    Register Old = SortedIntervals[I]->reg();
    unsigned New = SlotMapping[I];
    if (Old != New)
      MRI->replaceRegWith(Old, New);
  }
  return true;
}
