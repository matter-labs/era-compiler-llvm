//===-- EraVMLowerIntrinsics.cpp - Lower intrinsics -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass lowers memcpy, memmove and memset intrinsics.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

#include "EraVMSubtarget.h"

#define DEBUG_TYPE "eravm-lower-intrinsics"

using namespace llvm;

namespace {

class EraVMLowerIntrinsics : public ModulePass {
public:
  static char ID;

  EraVMLowerIntrinsics() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  bool expandMemIntrinsicUses(Function &F);
  StringRef getPassName() const override { return "EraVM Lower Intrinsics"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }
};

void ExpandMemCpyAsLoop(MemCpyInst *Memcpy, const TargetTransformInfo &TTI) {
  if (auto *CI = dyn_cast<ConstantInt>(Memcpy->getLength())) {
    createEraVMMemCpyLoopKnownSize(
        /* InsertBefore */ Memcpy,
        /* SrcAddr */ Memcpy->getRawSource(),
        /* DstAddr */ Memcpy->getRawDest(),
        /* CopyLen */ CI,
        /* SrcAlign */ Memcpy->getSourceAlign().valueOrOne(),
        /* DestAlign */ Memcpy->getDestAlign().valueOrOne(),
        /* SrcIsVolatile */ Memcpy->isVolatile(),
        /* DstIsVolatile */ Memcpy->isVolatile(),
        /* TargetTransformInfo */ TTI);
  } else {
    createEraVMMemCpyLoopUnknownSize(
        /* InsertBefore */ Memcpy,
        /* SrcAddr */ Memcpy->getRawSource(),
        /* DstAddr */ Memcpy->getRawDest(),
        /* CopyLen */ Memcpy->getLength(),
        /* SrcAlign */ Memcpy->getSourceAlign().valueOrOne(),
        /* DestAlign */ Memcpy->getDestAlign().valueOrOne(),
        /* SrcIsVolatile */ Memcpy->isVolatile(),
        /* DstIsVolatile */ Memcpy->isVolatile(),
        /* TargetTransfomrInfo */ TTI);
  }
}

} // namespace

char EraVMLowerIntrinsics::ID = 0;

INITIALIZE_PASS(EraVMLowerIntrinsics, DEBUG_TYPE, "Lower intrinsics", false,
                false)

bool EraVMLowerIntrinsics::expandMemIntrinsicUses(Function &F) {
  Intrinsic::ID ID = F.getIntrinsicID();
  bool Changed = false;

  for (auto I = F.user_begin(), E = F.user_end(); I != E;) {
    auto *Inst = cast<Instruction>(*I);
    ++I;

    switch (ID) {
    case Intrinsic::memcpy: {
      auto *Memcpy = cast<MemCpyInst>(Inst);
      Function *ParentFunc = Memcpy->getParent()->getParent();
      const TargetTransformInfo &TTI =
          getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*ParentFunc);
      ExpandMemCpyAsLoop(Memcpy, TTI);
      Changed = true;
      Memcpy->eraseFromParent();

      break;
    }
    case Intrinsic::memmove: {
      auto *Memmove = cast<MemMoveInst>(Inst);
      expandMemMoveAsLoop(Memmove);
      Changed = true;
      Memmove->eraseFromParent();

      break;
    }
    case Intrinsic::memset: {
      auto *Memset = cast<MemSetInst>(Inst);
      expandMemSetAsLoop(Memset);
      Changed = true;
      Memset->eraseFromParent();

      break;
    }
    default:
      break;
    }
  }

  return Changed;
}

bool EraVMLowerIntrinsics::runOnModule(Module &M) {
  bool Changed = false;

  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;

    switch (F.getIntrinsicID()) {
    case Intrinsic::memcpy:
    case Intrinsic::memmove:
    case Intrinsic::memset:
      if (expandMemIntrinsicUses(F))
        Changed = true;
      break;

    default:
      break;
    }
  }

  return Changed;
}

ModulePass *llvm::createEraVMLowerIntrinsicsPass() {
  return new EraVMLowerIntrinsics();
}
