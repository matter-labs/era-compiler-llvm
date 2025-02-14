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
// The algorithm is broken into following components:
//   - CFG (Control Flow Graph) and CFG builder. Stackification CFG has similar
//     structure to LLVM CFG one, but employs wider notion of instruction.
//   - Stack layout generator. Contains information about the stack layout at
//     entry and exit of each CFG::BasicBlock. It also contains input/output
//     stack layout for each operation.
//   - Code transformation into stakified form. This component uses both CFG
//     and the stack layout information to get stackified LLVM MIR.
//   - Stack shuffler. Finds optimal (locally) transformation between two stack
//     layouts using three primitives: POP, PUSHn, DUPn. The stack shuffler
//     is used by the components above.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineCFGInfo.h"
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

#define DEBUG_TYPE "evm-ethereum-stackify"

namespace {
class EVMBPStackification final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  EVMBPStackification() : MachineFunctionPass(ID) {}

private:
  StringRef getPassName() const override {
    return "EVM Ethereum stackification";
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
  EVMMachineCFGInfo CFGInfo(MF);
  EVMStackModel StackModel(MF, LIS,
                           MF.getSubtarget<EVMSubtarget>().stackDepthLimit());
  EVMMIRToStack StackMaps = EVMStackSolver(MF, MLI, StackModel, CFGInfo).run();
  EVMStackifyCodeEmitter(StackMaps, StackModel, CFGInfo, MF).run();
  return true;
}
