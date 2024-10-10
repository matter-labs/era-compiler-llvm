//===----- EVMSplitCriticalEdges.cpp - Split Critical Edges ------*- C++
//-*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file performs splitting of CFG critical edges.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-split-critical-edges"

namespace {
class EVMSplitCriticalEdges final : public MachineFunctionPass {
public:
  static char ID;

  EVMSplitCriticalEdges() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM split critical edges"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  MachineFunction *MF = nullptr;

  bool splitCriticalEdges();
};
} // end anonymous namespace

char EVMSplitCriticalEdges::ID = 0;

INITIALIZE_PASS_BEGIN(EVMSplitCriticalEdges, DEBUG_TYPE, "Split critical edges",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(EVMSplitCriticalEdges, DEBUG_TYPE, "Split critical edges",
                    false, false)

FunctionPass *llvm::createEVMSplitCriticalEdges() {
  return new EVMSplitCriticalEdges();
}

bool EVMSplitCriticalEdges::splitCriticalEdges() {
  SetVector<std::pair<MachineBasicBlock *, MachineBasicBlock *>> ToSplit;
  for (MachineBasicBlock &MBB : *MF) {
    if (MBB.pred_size() > 1) {
      for (MachineBasicBlock *Pred : MBB.predecessors()) {
        if (Pred->succ_size() > 1)
          ToSplit.insert(std::make_pair(Pred, &MBB));
      }
    }
  }

  bool Changed = false;
  for (const auto &Pair : ToSplit) {
    auto NewSucc = Pair.first->SplitCriticalEdge(Pair.second, *this);
    if (NewSucc != nullptr) {
      Pair.first->updateTerminator(NewSucc);
      NewSucc->updateTerminator(Pair.second);
      LLVM_DEBUG(dbgs() << " *** Splitting critical edge: "
                        << printMBBReference(*Pair.first) << " -- "
                        << printMBBReference(*NewSucc) << " -- "
                        << printMBBReference(*Pair.second) << '\n');
      Changed = true;
    } else {
      llvm_unreachable("Cannot break critical edge");
    }
  }
  return Changed;
}

bool EVMSplitCriticalEdges::runOnMachineFunction(MachineFunction &Mf) {
  MF = &Mf;
  LLVM_DEBUG({
    dbgs() << "********** Splitting critical edges **********\n"
           << "********** Function: " << Mf.getName() << '\n';
  });

  bool Changed = splitCriticalEdges();
  return Changed;
}
