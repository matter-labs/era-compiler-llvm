//===---- EVMAlwaysInline.cpp - Add alwaysinline attribute ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass adds alwaysinline attribute to functions with one call site.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#define DEBUG_TYPE "evm-always-inline"

using namespace llvm;

namespace {

class EVMAlwaysInline final : public ModulePass {
public:
  static char ID; // Pass ID
  EVMAlwaysInline() : ModulePass(ID) {}

  StringRef getPassName() const override { return "EVM always inline"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

static bool runImpl(Module &M) {
  bool Changed = false;
  for (auto &F : M) {
    if (F.isDeclaration() || F.hasOptNone() ||
        F.hasFnAttribute(Attribute::NoInline) || !F.hasOneUse())
      continue;

    auto *Call = dyn_cast<CallInst>(*F.user_begin());

    // Skip non call instructions, recursive calls, or calls with noinline
    // attribute.
    if (!Call || Call->getFunction() == &F || Call->isNoInline())
      continue;

    F.addFnAttr(Attribute::AlwaysInline);
    Changed = true;
  }

  return Changed;
}

bool EVMAlwaysInline::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return runImpl(M);
}

char EVMAlwaysInline::ID = 0;

INITIALIZE_PASS(EVMAlwaysInline, "evm-always-inline",
                "Add alwaysinline attribute to functions with one call site",
                false, false)

ModulePass *llvm::createEVMAlwaysInlinePass() { return new EVMAlwaysInline; }

PreservedAnalyses EVMAlwaysInlinePass::run(Module &M,
                                           ModuleAnalysisManager &AM) {
  runImpl(M);
  return PreservedAnalyses::all();
}
