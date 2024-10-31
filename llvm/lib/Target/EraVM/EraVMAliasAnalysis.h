//===-- EraVMAliasAnalysis.h - EraVM alias analysis -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EraVM address space based alias analysis pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMALIASANALYSIS_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMALIASANALYSIS_H

#include "llvm/Analysis/VMAliasAnalysis.h"

namespace llvm {
/// Analysis pass providing a never-invalidated alias analysis result.
class EraVMAA : public AnalysisInfoMixin<EraVMAA> {
  friend AnalysisInfoMixin<EraVMAA>;

  static AnalysisKey Key;

public:
  using Result = VMAAResult;
  VMAAResult run(Function &F, AnalysisManager<Function> &AM);
};

/// Legacy wrapper pass to provide the VMAAResult object.
class EraVMAAWrapperPass : public ImmutablePass {
  std::unique_ptr<VMAAResult> Result;

public:
  static char ID;

  EraVMAAWrapperPass();

  VMAAResult &getResult() { return *Result; }
  const VMAAResult &getResult() const { return *Result; }

  bool doFinalization(Module &M) override {
    Result.reset();
    return false;
  }

  bool doInitialization(Module &M) override;
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
