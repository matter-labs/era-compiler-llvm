//===-- EVMSHA3ConstFolding.cpp - Const fold calls to sha3 ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EVM SHA3 const folding pass.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsEVM.h"
#include "llvm/Transforms/Scalar/SHA3ConstFolding.h"

using namespace llvm;

PreservedAnalyses EVMSHA3ConstFoldingPass::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  // Don't run this pass if optimizing for size, since result of SHA3
  // calls will be replaced with a 32-byte constant, thus PUSH32 will
  // be emitted. This will increase code size.
  if (F.hasOptSize())
    return PreservedAnalyses::all();

  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  auto &AA = AM.getResult<AAManager>(F);
  const auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();
  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto IsSha3Call = [](const Instruction *I) {
    const auto *II = dyn_cast<IntrinsicInst>(I);
    return II && II->getIntrinsicID() == Intrinsic::evm_sha3;
  };

  return llvm::runSHA3ConstFolding(F, AA, AC, MSSA, DT, TLI, LI, IsSha3Call,
                                   EVMAS::AS_HEAP)
             ? PreservedAnalyses::none()
             : PreservedAnalyses::all();
}
