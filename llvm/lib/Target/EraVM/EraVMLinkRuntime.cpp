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
/// The attribute to internalize a runtime function in the beginning of opt
/// pipeline. Functions that are used directly by a frontend are generally
/// marked with it.
static const char INTERNALIZE_EARLY[] = "internalize-early";
/// The attribute to internalize a runtime function in the beginning of llc
/// pipeline. LLVM instructions and intrinsics that are lowered to a runtime
/// call are marked with it.
static const char INTERNALIZE_LATE[] = "internalize-late";

namespace {
/// Link the runtime library into the module.
/// At the moment front ends work only with single source programs.
struct EraVMLinkRuntime : public ModulePass {
public:
  static char ID;
  explicit EraVMLinkRuntime(bool OnlyInternalize = false)
      : ModulePass(ID), OnlyInternalize(OnlyInternalize) {}
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Link runtime library into the module";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
  bool OnlyInternalize;
};
} // namespace

char EraVMLinkRuntime::ID = 0;

INITIALIZE_PASS(EraVMLinkRuntime, "eravm-link-runtime",
                "Link runtime library into the module", false, false)

const char *LL_DATA =
#include "EraVMRT.inc"
    ;

static bool EraVMLinkRuntimeImpl(Module &M) {
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
          // Keep original symbols as they are
          if (!GV.hasName() || (GVS.count(GV.getName()) == 0))
            return true;
          // Internalize linked-in symbols with "internalize-early" attribute.
          if (auto *F = llvm::dyn_cast<Function>(&GV))
            return !F->hasFnAttribute(INTERNALIZE_EARLY);
          return false;
        });
      });
  if (LinkErr) {
    errs() << "Can't link EraVM runtime \n";
    exit(1);
  }
  return true;
}

// TODO: CPR-988 Replace with __sstore runtime function.
static Function *createSStoreFunction(Module &M) {
  LLVMContext &C = M.getContext();
  Type *Int256Ty = Type::getInt256Ty(C);
  std::vector<Type *> ArgTy = {Int256Ty, Int256Ty};
  M.getOrInsertFunction(
      "__sstore",
      FunctionType::get(Type::getVoidTy(C), {Int256Ty, Int256Ty}, false));
  Function *Result = M.getFunction("__sstore");
  auto *Entry = BasicBlock::Create(C, "entry", Result);
  auto *SStoreIntFn = Intrinsic::getDeclaration(&M, Intrinsic::eravm_sstore);

  IRBuilder<> Builder(Entry);
  Builder.CreateCall(SStoreIntFn, {Result->getArg(0), Result->getArg(1)});
  Builder.CreateRetVoid();
  return Result;
}

bool EraVMLinkRuntime::runOnModule(Module &M) {
  if (!OnlyInternalize)
    return EraVMLinkRuntimeImpl(M);
  bool Changed = internalizeModule(M, [](const GlobalValue &GV) {
    // Only keep entry public
    if (!GV.hasName())
      return true;
    // Internalize linked-in symbols with "internalize-late" attribute.
    if (auto *F = llvm::dyn_cast<Function>(&GV))
      return !F->hasFnAttribute(INTERNALIZE_LATE);
    return false;
  });
  // sstore can throw, so externalize invoke sstore by wrapping into a function.
  Function *SStoreF = nullptr;
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        auto Invoke = dyn_cast<InvokeInst>(&I);
        if (!Invoke)
          continue;
        Intrinsic::ID IntID = Invoke->getIntrinsicID();
        if (IntID != Intrinsic::eravm_sstore)
          continue;
        if (!SStoreF)
          SStoreF = createSStoreFunction(M);
        Invoke->setCalledFunction(SStoreF);
        Changed = true;
      }
  return Changed;
}

ModulePass *llvm::createEraVMLinkRuntimePass(bool OnlyInternalize) {
  return new EraVMLinkRuntime(OnlyInternalize);
}

PreservedAnalyses EraVMLinkRuntimePass::run(Module &M,
                                            ModuleAnalysisManager &AM) {
  EraVMLinkRuntimeImpl(M);
  return PreservedAnalyses::all();
}
