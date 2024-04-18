//===-- EraVMCodegenPrepare.cpp - EraVM CodeGen Prepare ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EraVM specific version of CodeGenPrepare.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IntrinsicsEraVM.h"

#include "EraVM.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-codegen-prepare"

namespace llvm {
FunctionPass *createEraVMCodegenPrepare();
} // namespace llvm

namespace {
struct EraVMCodegenPrepare : public FunctionPass {
public:
  static char ID;
  EraVMCodegenPrepare() : FunctionPass(ID) {
    initializeEraVMCodegenPreparePass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  bool convertPointerArithmetics(Function &F);

  StringRef getPassName() const override {
    return "Final transformations before code generation";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // namespace

char EraVMCodegenPrepare::ID = 0;

INITIALIZE_PASS(EraVMCodegenPrepare, "eravm-codegen-prepare",
                "Final transformations before code generation", false, false)

bool EraVMCodegenPrepare::runOnFunction(Function &F) {
  bool Changed = false;

  std::vector<Instruction *> Replaced;
  for (auto &BB : F)
    for (auto &I : BB) {
      auto *Call = dyn_cast<CallInst>(&I);
      if (!Call)
        continue;

      // TODO: CPR-1353 Move to the constant folding pass.
      Function *Callee = Call->getCalledFunction();
      if (!Callee && isa<BitCastOperator>(Call->getCalledOperand()))
        Callee = dyn_cast<Function>(
            cast<BitCastOperator>(Call->getCalledOperand())->getOperand(0));
      if (Callee && Callee->hasName()) {
        auto IsOpNZeroConst = [&Call](unsigned N) {
          return isa<ConstantInt>(Call->getOperand(N)) &&
                 cast<ConstantInt>(Call->getOperand(N))->isZero();
        };
        if ((Callee->getName() == "__memset_uma_as1" && IsOpNZeroConst(2)) ||
            (Callee->getName() == "__memset_uma_as2" && IsOpNZeroConst(2)) ||
            (Callee->getName() == "__small_store_as1" && IsOpNZeroConst(2)) ||
            (Callee->getName() == "__small_store_as2" && IsOpNZeroConst(2)) ||
            (Callee->getName() == "__small_store_as0" && IsOpNZeroConst(1))) {
          Changed = true;
          Replaced.push_back(&I);
        }
      }
    }
  for (auto *I : Replaced)
    I->eraseFromParent();

  // convert pointer arithmetics to intrinsics
  Changed |= convertPointerArithmetics(F);
  return Changed;
}

bool EraVMCodegenPrepare::convertPointerArithmetics(Function &F) {
  const DataLayout *DL = &F.getParent()->getDataLayout();
  auto computeOffset = [&](GetElementPtrInst *GEP, IRBuilder<> &Builder) {
    Value *TmpOffset = Builder.getIntN(256, 0);
    APInt Offset(256, 0);
    if (GEP->accumulateConstantOffset(*DL, Offset)) {
      // We allow arbitrary GEP offset (256-bit)
      TmpOffset = Builder.getInt(Offset.sext(256));
    } else {
      for (gep_type_iterator GTI = gep_type_begin(GEP), E = gep_type_end(GEP);
           GTI != E; ++GTI) {
        Value *Op = GTI.getOperand();
        uint64_t S = DL->getTypeAllocSize(GTI.getIndexedType());

        // rely on folding down the pipeline to fold it.
        Value *Rhs = Builder.CreateMul(Builder.getIntN(256, S), Op);
        TmpOffset = Builder.CreateAdd(TmpOffset, Rhs);
      }
    }
    assert(TmpOffset != nullptr);
    return TmpOffset;
  };

  std::vector<Instruction *> Replaced;

  // look for GEP and convert them to PTR_ADD
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
        // Address Space must be return data address space
        if (GEP->getPointerAddressSpace() != EraVMAS::AS_GENERIC)
          continue;

        Value *Base = GEP->getOperand(0);

        IRBuilder<> Builder(&I);
        Value *Offset = computeOffset(GEP, Builder);
        Base = Builder.CreateBitCast(Base, Builder.getInt8PtrTy(3));

        auto *PtrAdd =
            Intrinsic::getDeclaration(F.getParent(), Intrinsic::eravm_ptr_add);

        Value *NewI = Builder.CreateCall(PtrAdd, {Base, Offset});
        NewI = Builder.CreateBitCast(NewI, GEP->getType());

        LLVM_DEBUG(dbgs() << "    Converted GEP:"; GEP->dump();
                   dbgs() << "    to: "; NewI->dump());
        GEP->replaceAllUsesWith(NewI);
        Replaced.push_back(GEP);
      } else if (auto *PtrToInt = dyn_cast<PtrToIntInst>(&I)) {
        if (PtrToInt->getPointerAddressSpace() != EraVMAS::AS_GENERIC)
          continue;
        // convert it to `PTR_TO_INT` intrinsic internally
        auto *ptr = PtrToInt->getPointerOperand();
        IRBuilder<> Builder(&I);
        Value *NewI = Builder.CreateCall(
            Intrinsic::getDeclaration(F.getParent(), Intrinsic::eravm_ptrtoint),
            {ptr});
        LLVM_DEBUG(dbgs() << "    Converted PtrToInt:"; PtrToInt->dump();
                   dbgs() << "    to: "; NewI->dump());
        PtrToInt->replaceAllUsesWith(NewI);
        Replaced.push_back(PtrToInt);
      }
    }
  }

  for (Instruction *I : Replaced)
    I->eraseFromParent();

  return !Replaced.empty();
}

FunctionPass *llvm::createEraVMCodegenPreparePass() {
  return new EraVMCodegenPrepare();
}
