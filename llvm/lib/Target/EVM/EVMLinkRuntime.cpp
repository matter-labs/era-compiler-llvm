//===----- EVMLinkRuntime.cpp - inject runtime library into the module ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//============================================================================//
//
// TODO: CPR-1556, make this pass common for both EVM and EraVM BEs.
// This pas links stdlib (evm-stdlib.ll) and internalize their contents.
// EVM doesn't have a proper linker and all programs consist of a single module.
// The pass links the the necessary modules into the program module.
// It's called at the beginning of optimization pipeline. The pass links
// the context of evm-stdlib.ll and internalize its content, after that
// global DCE is expected to be run to remove all unused functions.
//
//============================================================================//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/Scalar.h"

#include <memory>

#include "EVM.h"

#define DEBUG_TYPE "evm-link-runtime"

using namespace llvm;

static ExitOnError ExitOnErr;

namespace {
/// Link std and runtime libraries into the module.
/// At the moment front ends work only with single source programs.
struct EVMLinkRuntime final : public ModulePass {
public:
  static char ID;
  EVMLinkRuntime() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Link runtime library into the module";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }
};
} // namespace

char EVMLinkRuntime::ID = 0;

INITIALIZE_PASS(EVMLinkRuntime, "evm-link-runtime",
                "Link standard and runtime library into the module", false,
                false)

static const char *STDLIB_DATA =
#include "EVMStdLib.inc"
    ;

static bool EVMLinkRuntimeImpl(Module &M, const char *ModuleToLink) {
  Linker L(M);
  LLVMContext &C = M.getContext();
  unsigned Flags = Linker::Flags::None;

  std::unique_ptr<MemoryBuffer> Buffer =
      MemoryBuffer::getMemBuffer(ModuleToLink);
  SMDiagnostic Err;
  std::unique_ptr<Module> RTM = parseIR(*Buffer, Err, C);
  if (!RTM) {
    Err.print("Unable to parse evm-stdlib.ll", errs());
    exit(1);
  }

  // TODO: remove this after ensuring the stackification
  // algorithm can deal with a high register pressure.
  for (auto &F : M.functions()) {
    if (!F.isDeclaration()) {
      F.addFnAttr(Attribute::NoInline);
    }
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
    errs() << "Can't link EVM runtime or stdlib \n";
    exit(1);
  }
  return true;
}

bool EVMLinkRuntime::runOnModule(Module &M) {
  return EVMLinkRuntimeImpl(M, STDLIB_DATA);
}

ModulePass *llvm::createEVMLinkRuntimePass() { return new EVMLinkRuntime(); }

PreservedAnalyses EVMLinkRuntimePass::run(Module &M,
                                          ModuleAnalysisManager &AM) {
  if (EVMLinkRuntimeImpl(M, STDLIB_DATA))
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
