//===-- EVMAliasAnalysis.h - EVM alias analysis -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EVM address space based alias analysis pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMALIASANALYSIS_H
#define LLVM_LIB_TARGET_EVM_EVMALIASANALYSIS_H

#include "llvm/Analysis/VMAliasAnalysis.h"

namespace llvm {

/// EVM-specific AA result. Note that we override certain non-virtual methods
/// from AAResultBase, as clarified in its documentation.
class EVMAAResult : public VMAAResult {
public:
  explicit EVMAAResult(const DataLayout &DL);

  ModRefInfo getModRefInfo(const CallBase *Call, const MemoryLocation &Loc,
                           AAQueryInfo &AAQI);

  ModRefInfo getArgModRefInfo(const CallBase *Call, unsigned ArgIdx);

  ModRefInfo getModRefInfo(const CallBase *Call1, const CallBase *Call2,
                           AAQueryInfo &AAQI) {
    return AAResultBase::getModRefInfo(Call1, Call2, AAQI);
  }
};

/// Analysis pass providing a never-invalidated alias analysis result.
class EVMAA : public AnalysisInfoMixin<EVMAA> {
  friend AnalysisInfoMixin<EVMAA>;

  static AnalysisKey Key;

public:
  using Result = EVMAAResult;
  EVMAAResult run(Function &F, AnalysisManager<Function> &AM);
};

/// Legacy wrapper pass to provide the EVMAAResult object.
class EVMAAWrapperPass : public ImmutablePass {
  std::unique_ptr<EVMAAResult> Result;

public:
  static char ID;

  EVMAAWrapperPass();

  EVMAAResult &getResult() { return *Result; }

  bool doFinalization(Module &M) override {
    Result.reset();
    return false;
  }

  bool doInitialization(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

// Wrapper around ExternalAAWrapperPass so that the default constructor gets the
// callback.
class EVMExternalAAWrapper : public ExternalAAWrapperPass {
public:
  static char ID;

  bool runEarly() override { return true; }

  EVMExternalAAWrapper()
      : ExternalAAWrapperPass([](Pass &P, Function &, AAResults &AAR) {
          if (auto *WrapperPass = P.getAnalysisIfAvailable<EVMAAWrapperPass>())
            AAR.addAAResult(WrapperPass->getResult());
        }) {}

  StringRef getPassName() const override {
    return "EVM Address space based Alias Analysis Wrapper";
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMALIASANALYSIS_H
