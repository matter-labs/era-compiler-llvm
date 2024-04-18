//===-- EraVMPostCodegenPrepare.cpp - EraVM Post CodeGenPrepare -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass performs optimizations after CodeGenPrepare pass.
// Currently, it is only performing icmp slt/sgt x, C (where C is a constant)
// to fewer unsigned compare instructions.
//
// TODOs:
//   1. #524: Rename this pass to EraVMCodegenPrepare after CPR-1353.
//   2. #491: Move rearrangeOverflowHandlingBranches from overflow reverted
//      patches here, since CodeGenPrepare can generate overflow intrinsics.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-post-codegen-prepare"
#define ERAVM_POST_CODEGEN_PREPARE_NAME                                        \
  "EraVM optimizations after CodeGenPrepare pass"

namespace {
struct EraVMPostCodegenPrepare : public FunctionPass {
public:
  static char ID; // Pass ID
  EraVMPostCodegenPrepare() : FunctionPass(ID) {
    initializeEraVMPostCodegenPreparePass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return ERAVM_POST_CODEGEN_PREPARE_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // end anonymous namespace

// Generate unsigned compare instructions based on the predicates and extend
// first operand if needed. If Pred2 is valid, generate second compare
// instruction with binary operation Opc.
static Value *generateUnsignedCmps(ICmpInst &Cmp, const APInt &Val,
                                   CmpInst::Predicate Pred1,
                                   CmpInst::Predicate Pred2,
                                   Instruction::BinaryOps Opc) {
  IRBuilder<> Builder(&Cmp);
  assert(Val.getBitWidth() == 256 && "Expected 256-bit value for comparison");
  assert(ICmpInst::isUnsigned(Pred1) && "Expected unsigned predicate in Pred1");

  auto *Val256 = ConstantInt::get(Builder.getInt256Ty(), Val);
  auto *Op256 = Builder.CreateZExt(Cmp.getOperand(0), Builder.getInt256Ty());
  auto *NewCmp = Builder.CreateICmp(Pred1, Op256, Val256);

  // Generate second compare instruction if needed.
  if (Pred2 != CmpInst::BAD_ICMP_PREDICATE) {
    auto *CmpVal = dyn_cast<ConstantInt>(Cmp.getOperand(1));
    assert(CmpVal && "Expected constant operand for signed compare");
    assert(ICmpInst::isUnsigned(Pred2) &&
           "Expected unsigned predicate in Pred2");
    auto *Cmp0 = Builder.CreateICmp(
        Pred2, Op256, Builder.getInt(CmpVal->getValue().zext(256)));
    NewCmp = Builder.CreateBinOp(Opc, NewCmp, Cmp0);
  }
  return NewCmp;
}

// Since we don't have signed compare instructions and we are doing expensive
// lowering of them, try to transform them into fewer unsigned compares where
// possible. Currently, it is trying to do following transformations:
//   1. x <s 0 -> x >u MaxSignedValue
//   2. x <s C (where C > 0) -> (x >u MaxSignedValue) | (x <u C)
//   3. x >s -1 -> x <u MinSignedValue
//   4. x >s C (where C > -1) -> (x <u MinSignedValue) & (x >u C)
static bool optimizeICmp(ICmpInst &Cmp) {
  auto *CmpVal = dyn_cast<ConstantInt>(Cmp.getOperand(1));
  if (!CmpVal || CmpVal->getBitWidth() > 256)
    return false;

  Value *NewCmp = nullptr;
  if (Cmp.getPredicate() == CmpInst::ICMP_SLT && !CmpVal->isNegative()) {
    APInt MaxSignedValue =
        APInt(CmpVal->getBitWidth(), -1, true).lshr(1).zext(256);
    NewCmp = generateUnsignedCmps(
        Cmp, MaxSignedValue, CmpInst::ICMP_UGT,
        !CmpVal->isZero() ? CmpInst::ICMP_ULT : CmpInst::BAD_ICMP_PREDICATE,
        Instruction::Or);
  } else if (Cmp.getPredicate() == CmpInst::ICMP_SGT &&
             (CmpVal->isMinusOne() || !CmpVal->isNegative())) {
    APInt MinSignedValue = APInt(CmpVal->getBitWidth(), 1)
                               .shl(CmpVal->getBitWidth() - 1)
                               .zext(256);
    NewCmp = generateUnsignedCmps(
        Cmp, MinSignedValue, CmpInst::ICMP_ULT,
        !CmpVal->isMinusOne() ? CmpInst::ICMP_UGT : CmpInst::BAD_ICMP_PREDICATE,
        Instruction::And);
  }

  if (!NewCmp)
    return false;

  Cmp.replaceAllUsesWith(NewCmp);
  Cmp.eraseFromParent();
  return true;
}

bool EraVMPostCodegenPrepare::runOnFunction(Function &F) {
  bool Changed = false;
  for (auto &BB : F)
    for (auto &I : llvm::make_early_inc_range(BB))
      if (auto *Cmp = dyn_cast<ICmpInst>(&I))
        Changed |= optimizeICmp(*Cmp);

  return Changed;
}

char EraVMPostCodegenPrepare::ID = 0;

INITIALIZE_PASS(EraVMPostCodegenPrepare, DEBUG_TYPE,
                ERAVM_POST_CODEGEN_PREPARE_NAME, false, false)

FunctionPass *llvm::createEraVMPostCodegenPreparePass() {
  return new EraVMPostCodegenPrepare();
}
