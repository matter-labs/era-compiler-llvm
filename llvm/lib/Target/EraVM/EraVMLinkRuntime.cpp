//===-- EraVMLinkRuntime.cpp - Link runtime library -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implement pass which links runtime (eravm-runtime.ll), stdlib
// (eravm-stdlib.ll) and internalize their contents. EraVM doesn't have a
// proper linker and all programs consist of a single module. The pass links
// the the necessary modules into the program module.
// It supposed to be called twice:
// * First, in the beginning of optimization pipeline, the pass links
//   the context of eravm-stdlib.ll and internalize its content, after that
//   global DCE is expected to be run to remove all unused functions.
// * Second, it runs before code generation, and it links and internalize
//   eravm-runtime.ll
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsEraVM.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/Scalar.h"

#include <memory>

#include "EraVM.h"

using namespace llvm;

namespace llvm {
ModulePass *createEraVMLinkRuntimePass();
} // namespace llvm

static ExitOnError ExitOnErr;

namespace {
/// Link std and runtime libraries into the module.
/// At the moment front ends work only with single source programs.
struct EraVMLinkRuntime : public ModulePass {
public:
  static char ID;
  explicit EraVMLinkRuntime(bool IsRuntimeLinkage = false)
      : ModulePass(ID), IsRuntimeLinkage(IsRuntimeLinkage) {}
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Link runtime library into the module";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
  bool IsRuntimeLinkage;
};
} // namespace

char EraVMLinkRuntime::ID = 0;

INITIALIZE_PASS(EraVMLinkRuntime, "eravm-link-runtime",
                "Link standard and runtime library into the module", false,
                false)

const char *RUNTIME_DATA =
#include "EraVMRT.inc"
    ;
const char *STDLIB_DATA =
#include "EraVMStdLib.inc"
    ;

static bool EraVMLinkRuntimeImpl(Module &M, const char *ModuleToLink) {
  Linker L(M);
  LLVMContext &C = M.getContext();
  unsigned Flags = Linker::Flags::None;

  std::unique_ptr<MemoryBuffer> Buffer =
      MemoryBuffer::getMemBuffer(ModuleToLink);
  SMDiagnostic Err;
  std::unique_ptr<Module> RTM = parseIR(*Buffer, Err, C);
  if (!RTM) {
    Err.print("Unable to parse eravm-runtime.ll, eravm-stdlib.ll", errs());
    exit(1);
  }
  bool LinkErr = false;
  LinkErr = L.linkInModule(
      std::move(RTM), Flags, [](Module &M, const StringSet<> &GVS) {
        internalizeModule(M, [&GVS](const GlobalValue &GV) {
          // Keep original symbols as they are
          return !GV.hasName() || (GVS.count(GV.getName()) == 0);
        });
      });
  if (LinkErr) {
    errs() << "Can't link EraVM runtime or stdlib \n";
    exit(1);
  }
  return true;
}

bool EraVMLinkRuntime::runOnModule(Module &M) {
  bool Changed =
      EraVMLinkRuntimeImpl(M, IsRuntimeLinkage ? RUNTIME_DATA : STDLIB_DATA);
  if (!IsRuntimeLinkage)
    return Changed;
  // sstore can throw, so externalize invoke sstore by wrapping into a function.
  Function *SStoreF = M.getFunction("__sstore");
  assert(SStoreF && "__sstore must be defined in eravm-runtime.ll");
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        auto *Invoke = dyn_cast<InvokeInst>(&I);
        if (!Invoke)
          continue;
        Intrinsic::ID IntID = Invoke->getIntrinsicID();
        if (IntID != Intrinsic::eravm_sstore)
          continue;
        Invoke->setCalledFunction(SStoreF);
        Changed = true;
      }
  return Changed;
}

ModulePass *llvm::createEraVMLinkRuntimePass(bool IsRuntimeLinkage) {
  return new EraVMLinkRuntime(IsRuntimeLinkage);
}

PreservedAnalyses EraVMLinkRuntimePass::run(Module &M,
                                            ModuleAnalysisManager &AM) {
  EraVMLinkRuntimeImpl(M, STDLIB_DATA);
  return PreservedAnalyses::all();
}
