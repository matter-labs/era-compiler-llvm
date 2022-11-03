//===- AllocaHoisting.cpp - Code to perform alloca hoisting ---------------===//
//
// This file implements hoisting alloca to the entry basic block
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/AllocaHoisting.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"

bool llvm::HoistAllocaToEntry(Function &F) {
  bool Modified = false;
  if (F.empty())
    return Modified;

  auto I = F.begin();
  Instruction *FirstTerminatorInst = (I++)->getTerminator();

  for (auto E = F.end(); I != E; ++I) {
    for (auto BI = I->begin(), BE = I->end(); BI != BE;) {
      AllocaInst *Alloca = dyn_cast<AllocaInst>(BI++);
      if (Alloca && isa<ConstantInt>(Alloca->getArraySize())) {
        Alloca->moveBefore(FirstTerminatorInst);
        Modified = true;
      }
    }
  }

  return Modified;
}
