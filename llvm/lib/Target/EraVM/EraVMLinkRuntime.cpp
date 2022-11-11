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
/// Link the runtime library into the module.
/// At the moment front ends work only with single source programs.
struct EraVMLinkRuntime : public ModulePass {
public:
  static char ID;
  explicit EraVMLinkRuntime() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Link runtime library into the module";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
};
} // namespace

char EraVMLinkRuntime::ID = 0;

INITIALIZE_PASS(EraVMLinkRuntime, "eravm-link-runtime",
                "Link runtime library into the module", false, false)

const char *LL_DATA =
#include "EraVMRT.inc"
    ;

bool EraVMLinkRuntime::runOnModule(Module &M) {
  Linker L(M);
  LLVMContext &C = M.getContext();
  unsigned Flags = Linker::Flags::None;

  std::unique_ptr<MemoryBuffer> Buffer = MemoryBuffer::getMemBuffer(LL_DATA);
  SMDiagnostic Err;
  std::unique_ptr<Module> RTM = parseIR(*Buffer, Err, C);
  if (!RTM) {
    Err.print("Unable to parse eravm-runtime.ll", errs());
    exit(1);
  }
  bool LinkErr = false;
  LinkErr = L.linkInModule(
      std::move(RTM), Flags, [](Module &M, const StringSet<> &GVS) {
        internalizeModule(M, [&GVS](const GlobalValue &GV) {
          return !GV.hasName() || (GVS.count(GV.getName()) == 0);
        });
      });
  if (LinkErr) {
    errs() << "Can't link EraVM runtime \n";
    exit(1);
  }
  return true;
}

ModulePass *llvm::createEraVMLinkRuntimePass() {
  return new EraVMLinkRuntime();
}
