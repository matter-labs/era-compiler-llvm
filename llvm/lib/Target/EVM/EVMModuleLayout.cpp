//===--- EVMModuleLayout.cpp - Proper arrangement of functions --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Ensures the '__entry' function is positioned at the beginning of the
//  module’s function list.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "evm-module-layout"

using namespace llvm;

namespace {

class EVMModuleLayout final : public ModulePass {
public:
  static char ID; // Pass ID
  EVMModuleLayout() : ModulePass(ID) {}

  StringRef getPassName() const override { return "EVM module layout"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

static bool runImpl(Module &M) {
  LLVM_DEBUG({ dbgs() << "********** Module: " << M.getName() << '\n'; });
  auto &Functions = M.getFunctionList();
  auto It = find_if(Functions, [](const Function &F) {
    return F.getName() == StringRef("__entry");
  });
  if (It == M.begin() || It == M.end())
    return false;

  LLVM_DEBUG({
    dbgs() << "Moving " << It->getName()
           << " to the beginning of the functions list\n";
  });

  Functions.splice(Functions.begin(), Functions, It);
  return true;
}

bool EVMModuleLayout::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return runImpl(M);
}

char EVMModuleLayout::ID = 0;

INITIALIZE_PASS(EVMModuleLayout, "evm-module-layout",
                "Places the '__entry' function at the beginning of the"
                "function list",
                false, false)

ModulePass *llvm::createEVMModuleLayoutPass() { return new EVMModuleLayout; }
