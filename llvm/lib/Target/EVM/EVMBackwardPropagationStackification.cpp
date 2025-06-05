//===----- EVMBPStackification.cpp - BP stackification ---------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements backward propagation (BP) stackification.
// Original idea was taken from the Ethereum's compiler (solc) stackification
// algorithm.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMStackSolver.h"
#include "EVMStackifyCodeEmitter.h"
#include "EVMSubtarget.h"
#include "llvm/CodeGen/CalcSpillWeights.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveStacks.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-backward-propagation-stackification"

static cl::opt<unsigned> MaxIterations(
    "evm-stackification-max-iterations", cl::Hidden, cl::init(100),
    cl::desc("Maximum number of iterations for stackification pass"));

namespace {
class EVMBPStackification final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  EVMBPStackification() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "EVM backward propagation stackification";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addPreserved<SlotIndexesWrapperPass>();
    AU.addRequired<VirtRegMap>();
    AU.addPreserved<VirtRegMap>();
    AU.addRequired<LiveStacks>();
    AU.addPreserved<LiveStacks>();
    AU.addRequired<MachineBlockFrequencyInfoWrapperPass>();
    AU.addPreserved<MachineBlockFrequencyInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::TracksLiveness);
  }

private:
  VirtRegMap *VRM = nullptr;
  LiveStacks *LSS = nullptr;
  LiveIntervals *LIS = nullptr;
  const MachineLoopInfo *MLI = nullptr;
  bool IsSpillWeightsCalculated{};

  /// From a vector of spillable registers, find the cheapest one to spill.
  Register
  getRegToSpill(const SmallSetVector<Register, 16> &SpillableRegs) const;

  /// Calculate spill weights. This is needed to determine which register to
  /// spill when we have multiple spillable registers.
  void calculateSpillWeights(MachineFunction &MF) {
    if (IsSpillWeightsCalculated)
      return;

    VirtRegAuxInfo VRAI(
        MF, *LIS, *VRM, *MLI,
        getAnalysis<MachineBlockFrequencyInfoWrapperPass>().getMBFI());
    VRAI.calculateSpillWeightsAndHints();
    IsSpillWeightsCalculated = true;
  }
};
} // end anonymous namespace

char EVMBPStackification::ID = 0;

INITIALIZE_PASS_BEGIN(EVMBPStackification, DEBUG_TYPE,
                      "Backward propagation stackification", false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervalsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(SlotIndexesWrapperPass)
INITIALIZE_PASS_DEPENDENCY(VirtRegMap)
INITIALIZE_PASS_DEPENDENCY(LiveStacks)
INITIALIZE_PASS_DEPENDENCY(MachineBlockFrequencyInfoWrapperPass)
INITIALIZE_PASS_END(EVMBPStackification, DEBUG_TYPE,
                    "Backward propagation stackification", false, false)

FunctionPass *llvm::createEVMBPStackification() {
  return new EVMBPStackification();
}

Register EVMBPStackification::getRegToSpill(
    const SmallSetVector<Register, 16> &SpillableRegs) const {
  assert(!SpillableRegs.empty() && "SpillableRegs should not be empty");

  const auto *BestInterval = &LIS->getInterval(SpillableRegs[0]);
  for (auto Reg : drop_begin(SpillableRegs)) {
    const auto *LI = &LIS->getInterval(Reg);

    // Take this interval only if it has a non-zero weight and
    // either BestInterval has zero weight or this interval has a lower
    // weight than the current best.
    if (LI->weight() != 0.0F && (BestInterval->weight() == 0.0F ||
                                 LI->weight() < BestInterval->weight()))
      BestInterval = LI;
  }

  LLVM_DEBUG({
    for (Register Reg : SpillableRegs) {
      dbgs() << "Spill candidate: ";
      LIS->getInterval(Reg).dump();
    }
    dbgs() << "  Best spill candidate: ";
    BestInterval->dump();
    dbgs() << '\n';
  });
  return BestInterval->reg();
}

bool EVMBPStackification::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Backward propagation stackification **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  MachineRegisterInfo &MRI = MF.getRegInfo();
  LIS = &getAnalysis<LiveIntervalsWrapperPass>().getLIS();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();
  VRM = &getAnalysis<VirtRegMap>();
  LSS = &getAnalysis<LiveStacks>();
  IsSpillWeightsCalculated = false;

  // We don't preserve SSA form.
  MRI.leaveSSA();

  assert(MRI.tracksLiveness() && "Stackification expects liveness");
  EVMStackModel StackModel(MF, *LIS,
                           MF.getSubtarget<EVMSubtarget>().stackDepthLimit());
  EVMStackSolver StackSolver(MF, StackModel, MLI);
  EVMStackifyCodeEmitter CodeEmitter(StackModel, MF, *VRM, *LSS, *LIS);
  if (MaxIterations == 0) {
    StackSolver.run();
    CodeEmitter.run(false);
  } else {
    unsigned IterCount = 0;
    while (true) {
      StackSolver.run();
      auto SpillSlotCandidates = CodeEmitter.run(true);
      if (SpillSlotCandidates.empty()) {
        CodeEmitter.run(false);
        break;
      }

      if (++IterCount > MaxIterations)
        report_fatal_error("Stackification failed: too many iterations");

      LLVM_DEBUG({
        dbgs() << "Unreachable slots found: " << SpillSlotCandidates.size()
               << ", iteration: " << IterCount << '\n';
      });

      // We are about to spill registers, so we need to calculate spill
      // weights to determine which register to spill.
      calculateSpillWeights(MF);

      SmallSet<Register, 4> RegsToSpill;
      for (auto &[StackSlots, Idx] : SpillSlotCandidates) {
        LLVM_DEBUG({
          dbgs() << "Unreachable slot: " << StackSlots[Idx]->toString()
                 << " at index: " << Idx << '\n';
          dbgs() << "Stack with unreachable slot: " << StackSlots.toString()
                 << '\n';
        });

        // Collect spillable registers from the stack slots starting from
        // the given index.
        SmallSetVector<Register, 16> SpillableRegs;
        for (unsigned I = Idx, E = StackSlots.size(); I < E; ++I)
          if (const auto *RegSlot = dyn_cast<RegisterSlot>(StackSlots[I]))
            if (!RegSlot->isRematerializable())
              SpillableRegs.insert(RegSlot->getReg());

        if (!SpillableRegs.empty())
          RegsToSpill.insert(getRegToSpill(SpillableRegs));
      }
      assert(!RegsToSpill.empty() && "No register to spill");
      StackModel.addSpillRegs(RegsToSpill);
    }
  }

  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  MFI->setIsStackified();

  // In a stackified code register liveness has no meaning.
  MRI.invalidateLiveness();
  return true;
}
