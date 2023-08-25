//===-- EraVMOptimizeStdLibCalls.cpp - Optimize stdlib calls ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass tries to replace an stdlib call with either a call to the more
// efficient algorithm, or just an optimized sequence of instructions.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#define DEBUG_TYPE "eravm-optimize-stdlib-calls"

using namespace llvm;

namespace {

class EraVMOptimizeStdLibCalls final : public FunctionPass {
private:
  StringRef getPassName() const override {
    return "EraVM optimize stdlib calls";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    FunctionPass::getAnalysisUsage(AU);
  }

  bool runOnFunction(Function &function) override;

public:
  static char ID; // Pass ID

  EraVMOptimizeStdLibCalls() : FunctionPass(ID) {}
};

} // end anonymous namespace

static bool hasUndefOrPoision(CallBase *Call) {
  for (const auto &Op : Call->operands()) {
    if (isa<PoisonValue>(Op) || isa<UndefValue>(Op))
      return true;
  }
  return false;
}

/// Convert __exp(base, exp) call where 'base' is a power of 2 into
/// the __exp_pow2(log2(base), exp), which performs 1 << (log2(base) * exp).
static bool tryToOptimizeExpCall(CallBase *Call, const TargetLibraryInfo &TLI) {
  if (hasUndefOrPoision(Call)) {
    Call->replaceAllUsesWith(PoisonValue::get(Call->getType()));
    LLVM_DEBUG(dbgs() << "replacing with : poison" << '\n');
    return true;
  }

  auto *CI = dyn_cast<ConstantInt>(Call->getOperand(0));
  if (!CI)
    return false;

  int LogBase = CI->getValue().exactLogBase2();
  if (LogBase <= 0)
    return false;

  StringRef ShlName = TLI.getName(LibFunc_xvm_exp_pow2);
  Function *ShlF = Call->getParent()->getModule()->getFunction(ShlName);
  assert(ShlF && "__shl must be defined in eravm-stdlib.ll");

  Call->setCalledFunction(ShlF);
  Call->setOperand(0, ConstantInt::get(Call->getType(), LogBase));
  Call->removeFnAttr(Attribute::NoInline);

  LLVM_DEBUG(dbgs() << "replacing the call with: " << *Call << '\n');
  return true;
}

static bool runEraVMOptimizeStdLibCalls(Function &F,
                                        const TargetLibraryInfo &TLI) {
  bool Changed = false;
  for (auto &I : instructions(F)) {
    CallInst *Call = dyn_cast<CallInst>(&I);
    if (!Call)
      continue;

    Function *Callee = Call->getCalledFunction();
    if (!Callee)
      continue;

    LibFunc Func = NotLibFunc;
    StringRef Name = Callee->getName();
    if (!TLI.getLibFunc(Name, Func) || !TLI.has(Func))
      continue;

    LLVM_DEBUG(dbgs() << "Trying to optimize : " << *Call << '\n');

    switch (Func) {
    case LibFunc_xvm_exp:
      Changed |= tryToOptimizeExpCall(Call, TLI);
      break;
    default:
      break;
    }
  }

  return Changed;
}

bool EraVMOptimizeStdLibCalls::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  const TargetLibraryInfo &TLI =
      getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);

  return runEraVMOptimizeStdLibCalls(F, TLI);
}

char EraVMOptimizeStdLibCalls::ID = 0;

INITIALIZE_PASS(
    EraVMOptimizeStdLibCalls, "eravm-optimize-stdlib-calls",
    "Replace stdlib call with a more optimized sequence of instructions", false,
    false)

FunctionPass *llvm::createEraVMOptimizeStdLibCallsPass() {
  return new EraVMOptimizeStdLibCalls;
}

PreservedAnalyses
EraVMOptimizeStdLibCallsPass::run(Function &F, FunctionAnalysisManager &AM) {
  const TargetLibraryInfo &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  return runEraVMOptimizeStdLibCalls(F, TLI) ? PreservedAnalyses::none()
                                             : PreservedAnalyses::all();
}
