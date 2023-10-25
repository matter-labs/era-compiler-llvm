//===-- EraVMSplitLoopExit.cpp - EraVM split loop exit --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This optimization splits loop exit if it contains constant phi from the
/// loop.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-split-loop-exit"

namespace llvm {
Pass *createEraVMSplitLoopExit();
void initializeEraVMSplitLoopExitPass(PassRegistry &);
} // namespace llvm

namespace {
struct EraVMSplitLoopExit : public FunctionPass {
public:
  static char ID;
  EraVMSplitLoopExit() : FunctionPass(ID) {
    initializeEraVMSplitLoopExitPass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "EraVM split loop exit"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
  }
};
} // namespace

char EraVMSplitLoopExit::ID = 0;

INITIALIZE_PASS(EraVMSplitLoopExit, "eravm-split-loop-exit",
                "EraVM split loop exit", false, false)

// Since our backend requires structured CFG, that means some of the MIR passes
// won't be able to split critical edge, thus to sink instructions outside of
// the loop. Because of that, here we split loop exit in case it contains
// constant phi that is comming from a loop, so we can place constants there.
bool EraVMSplitLoopExit::runOnFunction(Function &F) {
  bool Changed = false;
  const LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  SmallVector<std::pair<BasicBlock *, BasicBlock *>, 16> ToSplit;
  SmallPtrSet<BasicBlock *, 16> VisitedPred;

  for (const auto &L : LI) {
    SmallVector<BasicBlock *, 8> ExitBlocks;
    L->getUniqueExitBlocks(ExitBlocks);
    for (auto *BB : ExitBlocks) {
      if (BB->isLandingPad() || !BB->canSplitPredecessors())
        continue;

      for (const PHINode &PN : BB->phis()) {
        for (unsigned I = 0, E = PN.getNumIncomingValues(); I != E; ++I) {
          if (!isa<Constant>(PN.getIncomingValue(I)))
            continue;

          auto *Pred = PN.getIncomingBlock(I);
          if (L->contains(Pred) &&
              !isa<IndirectBrInst>(Pred->getTerminator()) &&
              VisitedPred.insert(Pred).second)
            ToSplit.push_back({BB, Pred});
        }
      }
    }
  }

  for (auto [BB, InLoopPredecessor] : ToSplit) {
    if (BasicBlock *NewBB =
            SplitBlockPredecessors(BB, InLoopPredecessor, ".loopexit")) {
      LLVM_DEBUG(dbgs() << "SUCCESSFULY SPLIT\n"
                        << "Pred: " << *InLoopPredecessor << "NewBB: " << *NewBB
                        << "BB: " << *BB);
      Changed = true;
    }
  }

  return Changed;
}

FunctionPass *llvm::createEraVMSplitLoopExitPass() {
  return new EraVMSplitLoopExit();
}
