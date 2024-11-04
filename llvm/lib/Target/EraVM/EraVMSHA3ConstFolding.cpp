//===-- EraVMSHA3ConstFolding.cpp - Const fold calls to __sha3 --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EraVM SHA3 const folding pass.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Scalar/SHA3ConstFolding.h"

using namespace llvm;

PreservedAnalyses EraVMSHA3ConstFoldingPass::run(Function &F,
                                                 FunctionAnalysisManager &AM) {
  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  auto &AA = AM.getResult<AAManager>(F);
  const auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();
  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto IsSha3Call = [&TLI](const Instruction *I) {
    const auto *Call = dyn_cast<CallInst>(I);
    if (!Call)
      return false;

    Function *Callee = Call->getCalledFunction();
    if (!Callee)
      return false;

    LibFunc Func = NotLibFunc;
    const StringRef Name = Callee->getName();
    return TLI.getLibFunc(Name, Func) && TLI.has(Func) &&
           Func == LibFunc_xvm_sha3;
  };

  return llvm::runSHA3ConstFolding(F, AA, AC, MSSA, DT, TLI, LI, IsSha3Call,
                                   EraVMAS::AS_HEAP)
             ? PreservedAnalyses::none()
             : PreservedAnalyses::all();
}
