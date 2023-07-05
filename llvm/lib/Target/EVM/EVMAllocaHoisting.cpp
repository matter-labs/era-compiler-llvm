//===-- EVMAllocaHoisting.cpp - Hoist allocas to the entry ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass hoists the alloca instructions in the non-entry blocks to the
// entry blocks.
// Copied from NVPTX backend.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"

#include "llvm/CodeGen/StackProtector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {
// Hoisting the alloca instructions in the non-entry blocks to the entry
// block.
class EVMAllocaHoisting final : public FunctionPass {
public:
  static char ID; // Pass ID

  EVMAllocaHoisting() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<StackProtector>();
  }

  StringRef getPassName() const override {
    return "EVM specific alloca hoisting";
  }

  bool runOnFunction(Function &F) override;
};

} // end anonymous namespace

char EVMAllocaHoisting::ID = 0;

static bool runImpl(Function &F) {
  bool Modified = false;
  Function::iterator I = F.begin();
  Instruction *FirstTerminatorInst = (I++)->getTerminator();

  for (auto E = F.end(); I != E; ++I) {
    for (auto BI = I->begin(), BE = I->end(); BI != BE;) {
      auto *Alloca = dyn_cast<AllocaInst>(BI++);
      if (Alloca && isa<ConstantInt>(Alloca->getArraySize())) {
        Alloca->moveBefore(FirstTerminatorInst);
        Modified = true;
      }
    }
  }

  return Modified;
}

bool EVMAllocaHoisting::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  return runImpl(F);
}

INITIALIZE_PASS(
    EVMAllocaHoisting, "evm-alloca-hoisting",
    "Hoisting alloca instructions in non-entry blocks to the entry block",
    false, false)

FunctionPass *llvm::createEVMAllocaHoistingPass() {
  return new EVMAllocaHoisting;
}

PreservedAnalyses EVMAllocaHoistingPass::run(Function &F,
                                             FunctionAnalysisManager &AM) {
  return runImpl(F) ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
