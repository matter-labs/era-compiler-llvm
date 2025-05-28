//===-- EVMSystemIntrinsicsPromotion.cpp - Intrinsics promotion -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass replaces system intrinsics with variants that include two
// additional null pointer arguments: one of type ptr addrspace(5) and the other
// ptr addrspace(6).
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsEVM.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-sys-intrinsics-promotion"

namespace {
class EVMSystemIntrinsicsPromotion final : public ModulePass {
public:
  static char ID; // Pass ID

  EVMSystemIntrinsicsPromotion() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "EVM system intrinsics promotion";
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

char EVMSystemIntrinsicsPromotion::ID = 0;

static void promoteIntrinsic(IRBuilder<> &Builder, Function &F) {
  static DenseMap<unsigned, unsigned> PromotionMap{
      {Intrinsic::evm_return, Intrinsic::evm_return_sptr},
      {Intrinsic::evm_revert, Intrinsic::evm_revert_sptr},
      {Intrinsic::evm_create, Intrinsic::evm_create_sptr},
      {Intrinsic::evm_create2, Intrinsic::evm_create2_sptr},
      {Intrinsic::evm_call, Intrinsic::evm_call_sptr},
      {Intrinsic::evm_callcode, Intrinsic::evm_callcode_sptr},
      {Intrinsic::evm_delegatecall, Intrinsic::evm_delegatecall_sptr},
      {Intrinsic::evm_staticcall, Intrinsic::evm_staticcall_sptr}};

  const Intrinsic::ID NewID = PromotionMap[F.getIntrinsicID()];
  for (User *U : make_early_inc_range(F.users())) {
    Function *NewF = Intrinsic::getDeclaration(F.getParent(), NewID);
    // Replace all users of the old instrinsics with the new one.
    if (auto *CI = dyn_cast<CallInst>(U)) {
      Builder.SetInsertPoint(CI->getParent(), CI->getIterator());
      SmallVector<Value *> Args(CI->args());
      Args.push_back(
          ConstantPointerNull::get(Builder.getPtrTy(EVMAS::AS_STORAGE)));
      Args.push_back(
          ConstantPointerNull::get(Builder.getPtrTy(EVMAS::AS_TSTORAGE)));
      CallInst *NewCall = Builder.CreateCall(NewF, Args);
      NewCall->setCallingConv(CI->getCallingConv());
      NewCall->takeName(CI);

      LLVM_DEBUG(dbgs() << "Promoting " << *CI << " to " << *NewCall << '\n');

      CI->replaceAllUsesWith(NewCall);
      CI->eraseFromParent();
    }
  }
  F.eraseFromParent();
}

static bool runImpl(Module &M) {
  LLVM_DEBUG({ dbgs() << "*** EVM system intrinsics promotion ***\n"; });

  bool Changed = false;
  LLVMContext &C = M.getContext();
  IRBuilder<> IRB(C);
  for (Function &F : make_early_inc_range(M)) {
    if (!F.isDeclaration())
      continue;

    switch (F.getIntrinsicID()) {
    case Intrinsic::evm_return:
    case Intrinsic::evm_revert:
    case Intrinsic::evm_create:
    case Intrinsic::evm_create2:
    case Intrinsic::evm_call:
    case Intrinsic::evm_callcode:
    case Intrinsic::evm_delegatecall:
    case Intrinsic::evm_staticcall: {
      promoteIntrinsic(IRB, F);
      Changed = true;
    } break;
    default:
      break;
    }
  }

  return Changed;
}

bool EVMSystemIntrinsicsPromotion::runOnModule(Module &M) { return runImpl(M); }

INITIALIZE_PASS(EVMSystemIntrinsicsPromotion, DEBUG_TYPE,
                "System intrinsics promotion", false, false)

ModulePass *llvm::createEVMSystemIntrinsicsPromotionPass() {
  return new EVMSystemIntrinsicsPromotion();
}

PreservedAnalyses
EVMSystemIntrinsicsPromotionPass::run(Module &M, ModuleAnalysisManager &AM) {
  return runImpl(M) ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
