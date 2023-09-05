//===-- SyncVMAliasAnalysis.h - SyncVM alias analysis ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This is the SyncVM address space based alias analysis pass.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMALIASANALYSIS_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMALIASANALYSIS_H

#include "llvm/Analysis/AliasAnalysis.h"

namespace llvm {

class DataLayout;
class MemoryLocation;

/// A simple AA result that uses address space info to answer queries.
class SyncVMAAResult : public AAResultBase<SyncVMAAResult> {
  friend AAResultBase<SyncVMAAResult>;

  const DataLayout &DL;

public:
  explicit SyncVMAAResult(const DataLayout &DL) : DL(DL) {}
  SyncVMAAResult(SyncVMAAResult &&Arg)
      : AAResultBase(std::move(Arg)), DL(Arg.DL) {}

  /// Handle invalidation events from the new pass manager.
  ///
  /// By definition, this result is stateless and so remains valid.
  bool invalidate(Function &, const PreservedAnalyses &,
                  FunctionAnalysisManager::Invalidator &) {
    return false;
  }

  AliasResult alias(const MemoryLocation &LocA, const MemoryLocation &LocB,
                    AAQueryInfo &AAQI);
};

/// Analysis pass providing a never-invalidated alias analysis result.
class SyncVMAA : public AnalysisInfoMixin<SyncVMAA> {
  friend AnalysisInfoMixin<SyncVMAA>;

  static AnalysisKey Key;

public:
  using Result = SyncVMAAResult;

  SyncVMAAResult run(Function &F, AnalysisManager<Function> &AM) {
    return SyncVMAAResult(F.getParent()->getDataLayout());
  }
};

/// Legacy wrapper pass to provide the SyncVMAAResult object.
class SyncVMAAWrapperPass : public ImmutablePass {
  std::unique_ptr<SyncVMAAResult> Result;

public:
  static char ID;

  SyncVMAAWrapperPass();

  SyncVMAAResult &getResult() { return *Result; }
  const SyncVMAAResult &getResult() const { return *Result; }

  bool doInitialization(Module &M) override {
    Result.reset(new SyncVMAAResult(M.getDataLayout()));
    return false;
  }

  bool doFinalization(Module &M) override {
    Result.reset();
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

// Wrapper around ExternalAAWrapperPass so that the default constructor gets the
// callback.
class SyncVMExternalAAWrapper : public ExternalAAWrapperPass {
public:
  static char ID;

  SyncVMExternalAAWrapper()
      : ExternalAAWrapperPass([](Pass &P, Function &, AAResults &AAR) {
          if (auto *WrapperPass =
                  P.getAnalysisIfAvailable<SyncVMAAWrapperPass>())
            AAR.addAAResult(WrapperPass->getResult());
        }) {}
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SYNCVM_SYNCVMALIASANALYSIS_H
