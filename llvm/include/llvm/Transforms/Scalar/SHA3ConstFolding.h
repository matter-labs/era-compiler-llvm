//===-- SHA3ConstFolding.h - Const fold calls to sha3 -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides the interface for the SHA3 const folding pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_SHA3CONSTFOLDING_H
#define LLVM_TRANSFORMS_SCALAR_SHA3CONSTFOLDING_H

#include "llvm/Analysis/AliasAnalysis.h"

namespace llvm {

class Function;
class AssumptionCache;
class MemorySSA;
class DominatorTree;
class TargetLibraryInfo;
class LoopInfo;
class Instruction;

bool runSHA3ConstFolding(
    Function &F, AliasAnalysis &AA, AssumptionCache &AC, MemorySSA &MSSA,
    DominatorTree &DT, const TargetLibraryInfo &TLI, const LoopInfo &LI,
    const std::function<bool(const Instruction *)> &IsSha3Call,
    unsigned HeapAS);

} // end namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_SHA3CONSTFOLDING_H
