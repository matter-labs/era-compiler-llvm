//===---- EVMRewriteFromFreePtr.cpp - ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#define DEBUG_TYPE "evm-rewrite-from-free-ptr"

using namespace llvm;

namespace {

class EVMRewriteFromFreePtr final : public ModulePass {
public:
  static char ID; // Pass ID
  EVMRewriteFromFreePtr() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "EVM rewrite from free pointer";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

static bool runImpl(Module &M) {
  GlobalVariable *FreePtrGV =
      M.getGlobalVariable("__evm_free_ptr", /*AllowInternal*/ true);
  if (!FreePtrGV)
    return false;

  for (auto *U : make_early_inc_range(FreePtrGV->users())) {
    assert(isa<LoadInst>(U) ||
           isa<StoreInst>(U) && "Expected load or store instruction");
    auto *CE = ConstantExpr::getIntToPtr(
        ConstantInt::get(IntegerType::get(M.getContext(), 256), 64),
        PointerType::get(IntegerType::get(M.getContext(), 256),
                         EVMAS::AS_HEAP));
    U->setOperand(isa<LoadInst>(U) ? 0 : 1, CE);
  }

  FreePtrGV->eraseFromParent();
  return true;
}

bool EVMRewriteFromFreePtr::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return runImpl(M);
}

char EVMRewriteFromFreePtr::ID = 0;

INITIALIZE_PASS(EVMRewriteFromFreePtr, DEBUG_TYPE,
                "EVM rewrite from free pointer", false, false)

ModulePass *llvm::createEVMRewriteFromFreePtrPass() {
  return new EVMRewriteFromFreePtr;
}

PreservedAnalyses EVMRewriteFromFreePtrPass::run(Module &M,
                                                 ModuleAnalysisManager &AM) {
  runImpl(M);
  return PreservedAnalyses::all();
}
