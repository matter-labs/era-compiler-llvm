//===-- VMAliasAnalysis.h - VM alias analysis -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the address space based alias analysis pass, that is shared between
// EraVM and EVM backends.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_VMALIASANALYSIS_H
#define LLVM_ANALYSIS_VMALIASANALYSIS_H

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Support/ModRef.h"

namespace llvm {

class DataLayout;
class MemoryLocation;

/// A simple AA result that uses address space info to answer queries.
class VMAAResult : public AAResultBase {
  const DataLayout &DL;
  SmallDenseSet<unsigned> StorageAS;
  SmallDenseSet<unsigned> HeapAS;
  unsigned MaxAS;

public:
  explicit VMAAResult(const DataLayout &DL,
                      const SmallDenseSet<unsigned> &StorageAS,
                      const SmallDenseSet<unsigned> &HeapAS, unsigned MaxAS)
      : DL(DL), StorageAS(StorageAS), HeapAS(HeapAS), MaxAS(MaxAS) {}

  /// Handle invalidation events from the new pass manager.
  ///
  /// By definition, this result is stateless and so remains valid.
  bool invalidate(Function &, const PreservedAnalyses &,
                  FunctionAnalysisManager::Invalidator &) {
    return false;
  }

  AliasResult alias(const MemoryLocation &LocA, const MemoryLocation &LocB,
                    AAQueryInfo &AAQI, const Instruction *I);

  ModRefInfo getModRefInfo(const CallBase *Call, const MemoryLocation &Loc,
                           AAQueryInfo &AAQI);
  ModRefInfo getModRefInfo(const CallBase *Call1, const CallBase *Call2,
                           AAQueryInfo &AAQI);
};
} // end namespace llvm

#endif // LLVM_ANALYSIS_VMALIASANALYSIS_H
