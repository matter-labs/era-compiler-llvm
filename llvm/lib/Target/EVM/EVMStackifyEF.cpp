//===----- EVMStackifyEF.cpp - Split Critical Edges ------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file performs spliting of critical edges.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMAssembly.h"
#include "EVMOptimizedCodeTransform.h"
#include "EVMSubtarget.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-ethereum-stackify"

namespace {
class EVMStackifyEF final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  EVMStackifyEF() : MachineFunctionPass(ID) {}

private:
  StringRef getPassName() const override { return "EVM spliting latch blocks"; }

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

char EVMStackifyEF::ID = 0;

INITIALIZE_PASS_BEGIN(EVMStackifyEF, DEBUG_TYPE, "Ethereum stackification",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(EVMStackifyEF, DEBUG_TYPE, "Ethereum stackification", false,
                    false)

FunctionPass *llvm::createEVMStackifyEF() { return new EVMStackifyEF(); }

bool EVMStackifyEF::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Ethereum stackification **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const EVMInstrInfo *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();
  LiveIntervals &LIS = getAnalysis<LiveIntervals>();
  MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();

  // We don't preserve SSA form.
  MRI.leaveSSA();

  assert(MRI.tracksLiveness() && "Stackify expects liveness");

  EVMAssembly Assembly(&MF, TII);
  EVMOptimizedCodeTransform::run(Assembly, MF, LIS, MLI);
  return true;
}
