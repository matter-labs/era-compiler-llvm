//===---------- EVMSplitLatchBlocks.cpp - Register coloring -----*- C++
//-*------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file performs spliting of a joined Latch/Exiting basic block into
// two Latch and Exiting.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "evm-split-latch-mbb"

namespace {
class EVMSplitLatchBlocks final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  EVMSplitLatchBlocks() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM spliting latch blocks"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool splitLatch(MachineBasicBlock *Latch, MachineBasicBlock *Header);

  MachineBasicBlock *createNewBlockAfter(MachineBasicBlock &OrigMBB);

  void splitLatchBlockBeforeInstr(MachineInstr *MI,
                                  MachineBasicBlock *LoopHeader);

  void createNewLatchBlock(MachineInstr *CondBr, MachineBasicBlock *Latch,
                           MachineBasicBlock *LoopHeader);

  MachineFunction *MF = nullptr;
  const TargetInstrInfo *TII = nullptr;
};
} // end anonymous namespace

char EVMSplitLatchBlocks::ID = 0;

INITIALIZE_PASS_BEGIN(EVMSplitLatchBlocks, DEBUG_TYPE,
                      "Split Latch/exiting mbb", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(EVMSplitLatchBlocks, DEBUG_TYPE, "Split Latch/exiting mbb",
                    false, false)

FunctionPass *llvm::createEVMSplitLatchBlocks() {
  return new EVMSplitLatchBlocks();
}

/// Insert a new empty MachineBasicBlock after \p OrigMBB
MachineBasicBlock *
EVMSplitLatchBlocks::createNewBlockAfter(MachineBasicBlock &OrigMBB) {
  // Create a new MBB for the code after the OrigBB.
  MachineBasicBlock *NewBB =
      MF->CreateMachineBasicBlock(OrigMBB.getBasicBlock());
  MF->insert(++OrigMBB.getIterator(), NewBB);
  return NewBB;
}

void EVMSplitLatchBlocks::createNewLatchBlock(MachineInstr *CondBr,
                                              MachineBasicBlock *Latch,
                                              MachineBasicBlock *LoopHeader) {
  MachineBasicBlock *NewLatch = createNewBlockAfter(*Latch);
  // Update branch target of the conditional branch.
  CondBr->getOperand(0).setMBB(NewLatch);

  // Insert unconditional jump to the new block.
  TII->insertUnconditionalBranch(*NewLatch, LoopHeader, CondBr->getDebugLoc());

  // Update the succesor lists according to the transformation.
  Latch->replaceSuccessor(LoopHeader, NewLatch);
  NewLatch->addSuccessor(LoopHeader);

  // Cleanup potential unconditional branch to successor block.
  Latch->updateTerminator(NewLatch);
}

/// Split the latch basic block containing MI into two blocks, which are joined
/// by an unconditional branch.
void EVMSplitLatchBlocks::splitLatchBlockBeforeInstr(
    MachineInstr *MI, MachineBasicBlock *LoopHeader) {
  MachineBasicBlock *Latch = MI->getParent();

  // Create a new MBB after the OrigBB.
  MachineBasicBlock *NewLatch =
      MF->CreateMachineBasicBlock(Latch->getBasicBlock());
  MF->insert(++Latch->getIterator(), NewLatch);

  // Splice the instructions starting with MI over to NewBB.
  NewLatch->splice(NewLatch->end(), Latch, MI->getIterator(), Latch->end());

  // Add an unconditional branch from OrigBB to NewBB.
  TII->insertUnconditionalBranch(*Latch, NewLatch, DebugLoc());

  Latch->replaceSuccessor(LoopHeader, NewLatch);
  NewLatch->addSuccessor(LoopHeader);

  // Cleanup potential unconditional branch to successor block.
  Latch->updateTerminator(NewLatch);
}

bool EVMSplitLatchBlocks::splitLatch(MachineBasicBlock *Latch,
                                     MachineBasicBlock *Header) {
  MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
  SmallVector<MachineOperand, 1> Cond;
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  if (TII->analyzeBranch(*Latch, TBB, FBB, Cond))
    llvm_unreachable("Unexpected Latch terminator");

  // Return if this is an unconditional branch
  if (!TBB || Cond.empty())
    return false;

  MachineInstr *CondJump = Cond[0].getParent();
  if (!FBB) {
    MachineBasicBlock *FH = Latch->getFallThrough();
    assert(FH);
    // Insert an unconditional jump to the Latch, because "false" block isn't
    // fallthrough anymore.
    TII->insertUnconditionalBranch(*Latch, FH, CondJump->getDebugLoc());
    assert(TBB = Header);
    createNewLatchBlock(CondJump, Latch, Header);
  } else {
    // There are both conditional and unconditional branches at the BB end.
    // We need to figure out which branch targets the loop header.
    MachineInstr *JumpToHeader = nullptr;
    MachineBasicBlock::iterator I = Latch->getFirstTerminator(),
                                E = Latch->end();
    for (; I != E; ++I) {
      if (I->isConditionalBranch() || I->isUnconditionalBranch())
        if (I->getOperand(0).getMBB() == Header) {
          JumpToHeader = &*I;
          break;
        }
    }
    if (JumpToHeader->isUnconditionalBranch())
      splitLatchBlockBeforeInstr(JumpToHeader, Header);
    else
      createNewLatchBlock(JumpToHeader, Latch, Header);
  }

  return true;
}

bool EVMSplitLatchBlocks::runOnMachineFunction(MachineFunction &Mf) {
  MF = &Mf;
  LLVM_DEBUG({
    dbgs() << "********** Spliting Latch/exiting MBB **********\n"
           << "********** Function: " << Mf.getName() << '\n';
  });

  const TargetSubtargetInfo &ST = MF->getSubtarget();
  TII = ST.getInstrInfo();

  bool Changed = false;
  MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();
  SmallVector<MachineLoop *, 8> Worklist(MLI->begin(), MLI->end());
  while (!Worklist.empty()) {
    MachineLoop *ML = Worklist.pop_back_val();
    SmallVector<MachineBasicBlock *, 8> Latches;
    ML->getLoopLatches(Latches);
    Worklist.append(ML->begin(), ML->end());
    for (MachineBasicBlock *Latch : Latches)
      Changed |= splitLatch(Latch, ML->getHeader());
  }

  return Changed;
}
