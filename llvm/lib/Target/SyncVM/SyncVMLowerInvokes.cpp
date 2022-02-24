//=== SyncVMLowerInvokes.cpp - Lower Invokes to calls and checks =//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file is copied and modified from WebAssembly/WebAssemblyLowerEmscriptenEHSjLj.cpp
/// The pass is tailored because we don't need:
/// * long jumps and set jumps because SyncVM does not have such capability.
/// The file descriptions are kept.
/// 
/// This file lowers exception-related instructions (`InvokeInst`'s) to SyncVM-compatible
/// handling mechanism.
///
/// First, we introduce a `__THREW__` global variable which serves as an indicator
/// of exception status. Should the function call `__cxa_throw` to throw a runtime
/// exception, instead the program sets `__THREW__` to be 1 and return via normal path.
///
/// Second, an `InvokeInst` is lowered to `CallInst` followed by a conditional branch. 
/// The predicate in the branch checks if the `__THREW__` value has been changed. If so
/// we will redirect our code to unwind BB label.
///
///===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "SyncVMTargetMachine.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/IntrinsicsSyncVM.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-lower-invokes"

namespace {
class SyncVMLowerInvokes final : public ModulePass {

  StringRef getPassName() const override {
    return "SyncVM Lower Invokes";
  }

  bool runEHOnFunction(Function &F);

public:
  static char ID;

  SyncVMLowerInvokes()
      : ModulePass(ID) {
    
  }
  bool runOnModule(Module &M) override;
};
} // End anonymous namespace

char SyncVMLowerInvokes::ID = 0;
INITIALIZE_PASS(SyncVMLowerInvokes, DEBUG_TYPE,
                "SyncVM Lower invokes",
                false, false)

ModulePass *llvm::createSyncVMLowerInvokesPass() {
  return new SyncVMLowerInvokes();
}

static Value *getAddrSizeInt(Module *M, uint64_t C) {
  IRBuilder<> IRB(M->getContext());
  return IRB.getIntN(M->getDataLayout().getPointerSizeInBits(), C);
}

bool SyncVMLowerInvokes::runOnModule(Module &M) {
  LLVM_DEBUG(dbgs() << "********** Lower `InvokeInst`s **********\n");

  LLVMContext &C = M.getContext();
  IRBuilder<> IRB(C);


  auto *TPC = getAnalysisIfAvailable<TargetPassConfig>();
  assert(TPC && "Expected a TargetPassConfig");

  bool Changed = false;

  for (Function & F : M) {
    if (F.isDeclaration()) continue;
    Changed |= runEHOnFunction(F);
  }

  return true;
}

static bool isThrowFunction(const Function* Callee) {
  return Callee->getName() == "__cxa_throw"; 
}

bool SyncVMLowerInvokes::runEHOnFunction(Function &F) {
  Module &M = *F.getParent();
  LLVMContext &C = F.getContext();
  IRBuilder<> IRB(C);
  bool Changed = false;
  SmallVector<Instruction *, 64> ToErase;
  SmallPtrSet<LandingPadInst *, 32> LandingPads;

  Function *ThrowF = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_throw_panic);

  // This loop will replace InvokeInsts:
  for (BasicBlock &BB : F) {
    auto *II = dyn_cast<InvokeInst>(BB.getTerminator());
    if (!II)
      continue;
    Changed = true;

    ToErase.push_back(II);

    IRB.SetInsertPoint(II);

    // if we are invoking __cxa_throw, we will generate a different pattern:
    // call __cxa_throw 
    // which will emit instruction: `ret.panic`
    {
      const Function *Callee = II->getCalledFunction();
      assert(Callee && "InvokeInst must have a callee function");
      if (isThrowFunction(Callee)) {
        CallInst *ThrowFCall = IRB.CreateCall(ThrowF, {});
        ThrowFCall->setTailCall();
        continue;
      }
    }

    //II->getLandingPadInst()->eraseFromParent();

    // change InvokeInst to CallInst
    // TODO: Call should be before Invoke because Invoke is a teminator
    std::vector<Value*> operands(II->arg_begin(), II->arg_end());
    auto* call_ret = IRB.CreateCall(II->getFunctionType(), II->getCalledOperand(), operands);

    // TODO: properly replace the invoke: if the invoke returns some value, we should replace it.
    if (!II->uses().empty()) {
      II->replaceAllUsesWith(call_ret);
    }

    // emit:
    // ```
    // gt_val = call int_syncvm_gtflag
    // %cmp = icmp gt_val 0
    // brcond gt %cmp, %normal, %catch
    // ```
    // immediate after the call
    Function* GtFlagFunction = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_gtflag);
    CallInst* GTFlag = IRB.CreateCall(GtFlagFunction, {});
    Value *CmpEqZero =
        IRB.CreateICmpEQ(GTFlag, getAddrSizeInt(&M, 0), "cmp.GT_flag");
    IRB.CreateCondBr(CmpEqZero, II->getNormalDest(), II->getUnwindDest());
  }

  // This loop will find all `__cxa_throw` calls and replace with: `ret.panic`
  for (BasicBlock &BB : F) {
    for (auto & I : BB) {
      auto *CI = dyn_cast<CallInst>(&I);
      if (!CI)
        continue;
      Changed = true;

      if (!isThrowFunction(CI->getCalledFunction())) {
        continue;
      } 

      // remove the subsequent unreachable inst.
      /*
      Instruction *nextInst = CI->getNextNode();
      UnreachableInst* unreachable = dyn_cast<UnreachableInst>(nextInst);
      assert(unreachable);
      ToErase.push_back(unreachable);
      */

      IRB.SetInsertPoint(CI);

      CallInst * t= IRB.CreateCall(ThrowF, {});
      t->setTailCall();

      ToErase.push_back(CI);
    }
  }

  // Erase everything we no longer need in this function
  for (Instruction *I : ToErase)
    I->eraseFromParent();

  return Changed;
}

