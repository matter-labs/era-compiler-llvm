//===---- EVMRewriteToFreePtr.cpp - -----------------------------*- C++ -*-===//
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

#define DEBUG_TYPE "evm-rewrite-to-free-ptr"

using namespace llvm;

namespace {

class EVMRewriteToFreePtr final : public ModulePass {
public:
  static char ID; // Pass ID
  EVMRewriteToFreePtr() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "EVM rewrite to free pointer";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

static GlobalVariable *getOrCreateFreePtrGV(Module &M) {
  if (auto *G = M.getGlobalVariable("__evm_free_ptr", /*AllowInternal*/ true))
    return G;

  auto &Ctx = M.getContext();
  auto *Ty = IntegerType::get(Ctx, 256);
  auto *G = new GlobalVariable(M, Ty, /*isConstant=*/false,
                               GlobalValue::ExternalLinkage,
                               /*Initializer=*/nullptr, "__evm_free_ptr");
  return G;
}

static bool isValidConstAddr(const Value *Ptr) {
  if (const auto *CE = dyn_cast<ConstantExpr>(Ptr)) {
    if (CE->getOpcode() == Instruction::IntToPtr) {
      if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(0))) {
        APInt Val = CI->getValue();
        return Val.getBitWidth() == 256 && Val == APInt(256, 64);
      }
    }
  }
  return false;
}

static bool runImpl(Module &M) {
  bool Changed = false;
  GlobalVariable *FreePtrGV = nullptr;

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    for (Instruction &I : instructions(F)) {
      auto *Ptr = getLoadStorePointerOperand(&I);
      if (!Ptr || Ptr->getType()->getPointerAddressSpace() != EVMAS::AS_HEAP ||
          !isValidConstAddr(Ptr))
        continue;

      if (!FreePtrGV)
        FreePtrGV = getOrCreateFreePtrGV(M);

      I.setOperand(isa<LoadInst>(I) ? 0 : 1, FreePtrGV);
      Changed = true;
    }
  }

  return Changed;
}

bool EVMRewriteToFreePtr::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return runImpl(M);
}

char EVMRewriteToFreePtr::ID = 0;

INITIALIZE_PASS(EVMRewriteToFreePtr, DEBUG_TYPE, "EVM rewrite to free pointer",
                false, false)

ModulePass *llvm::createEVMRewriteToFreePtrPass() {
  return new EVMRewriteToFreePtr;
}

PreservedAnalyses EVMRewriteToFreePtrPass::run(Module &M,
                                               ModuleAnalysisManager &AM) {
  runImpl(M);
  return PreservedAnalyses::all();
}
