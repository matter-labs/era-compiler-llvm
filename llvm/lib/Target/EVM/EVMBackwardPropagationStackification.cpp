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
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-backward-propagation-stackification"

namespace {
class EVMBPStackification final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  EVMBPStackification() : MachineFunctionPass(ID) {}

private:
  StringRef getPassName() const override {
    return "EVM backward propagation stackification";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::TracksLiveness);
  }
};
} // end anonymous namespace

char EVMBPStackification::ID = 0;

INITIALIZE_PASS_BEGIN(EVMBPStackification, DEBUG_TYPE,
                      "Backward propagation stackification", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(EVMBPStackification, DEBUG_TYPE,
                    "Backward propagation stackification", false, false)

FunctionPass *llvm::createEVMBPStackification() {
  return new EVMBPStackification();
}

bool EVMBPStackification::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Backward propagation stackification **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto &LIS = getAnalysis<LiveIntervals>();
  MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();

  // We don't preserve SSA form.
  MRI.leaveSSA();

  assert(MRI.tracksLiveness() && "Stackification expects liveness");
  EVMStackModel StackModel(MF, LIS,
                           MF.getSubtarget<EVMSubtarget>().stackDepthLimit());
  EVMStackSolver(MF, StackModel, MLI).run();
  EVMStackifyCodeEmitter(StackModel, MF).run();

  auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
  MFI->setIsStackified();

  // In a stackified code register liveness has no meaning.
  MRI.invalidateLiveness();
  return true;
}
