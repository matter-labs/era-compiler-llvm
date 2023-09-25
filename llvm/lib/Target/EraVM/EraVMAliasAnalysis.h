//===-- EraVMAliasAnalysis.h - EraVM alias analysis -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This is the EraVM address space based alias analysis pass.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMALIASANALYSIS_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMALIASANALYSIS_H

#include "llvm/Analysis/AliasAnalysis.h"

namespace llvm {

class DataLayout;
class MemoryLocation;

/// A simple AA result that uses address space info to answer queries.
class EraVMAAResult : public AAResultBase<EraVMAAResult> {
  friend AAResultBase<EraVMAAResult>;

  const DataLayout &DL;

public:
  explicit EraVMAAResult(const DataLayout &DL) : DL(DL) {}
  EraVMAAResult(EraVMAAResult &&Arg)
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
class EraVMAA : public AnalysisInfoMixin<EraVMAA> {
  friend AnalysisInfoMixin<EraVMAA>;

  static AnalysisKey Key;

public:
  using Result = EraVMAAResult;

  EraVMAAResult run(Function &F, AnalysisManager<Function> &AM) {
    return EraVMAAResult(F.getParent()->getDataLayout());
  }
};

/// Legacy wrapper pass to provide the EraVMAAResult object.
class EraVMAAWrapperPass : public ImmutablePass {
  std::unique_ptr<EraVMAAResult> Result;

public:
  static char ID;

  EraVMAAWrapperPass();

  EraVMAAResult &getResult() { return *Result; }
  const EraVMAAResult &getResult() const { return *Result; }

  bool doInitialization(Module &M) override {
    Result.reset(new EraVMAAResult(M.getDataLayout()));
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
class EraVMExternalAAWrapper : public ExternalAAWrapperPass {
public:
  static char ID;

  EraVMExternalAAWrapper()
      : ExternalAAWrapperPass([](Pass &P, Function &, AAResults &AAR) {
          if (auto *WrapperPass =
                  P.getAnalysisIfAvailable<EraVMAAWrapperPass>())
            AAR.addAAResult(WrapperPass->getResult());
        }) {}
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ERAVM_ERAVMALIASANALYSIS_H
