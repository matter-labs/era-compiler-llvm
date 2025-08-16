//===----------- EVMTargetTransformInfo.cpp - EVM-specific TTI -*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EVM-specific TargetTransformInfo
// implementation.
//
//===----------------------------------------------------------------------===//

#include "EVMTargetTransformInfo.h"
#include "llvm/IR/IntrinsicsEVM.h"
#include "llvm/Transforms/InstCombine/InstCombiner.h"
using namespace llvm;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "evmtti"

static std::optional<Instruction *> instCombineSignExtend(InstCombiner &IC,
                                                          IntrinsicInst &II) {
  // Fold signextend(b, signextend(b, x)) -> signextend(b, x)
  Value *B1 = nullptr, *B2 = nullptr, *X = nullptr;
  if (!match(&II, m_Intrinsic<Intrinsic::evm_signextend>(
                     m_Value(B1), m_Intrinsic<Intrinsic::evm_signextend>(
                                     m_Value(B2), m_Value(X)))))
    return std::nullopt;
  if (B1 == B2)
    return IC.replaceInstUsesWith(II, II.getArgOperand(1));
  auto *C1 = dyn_cast<ConstantInt>(B1);
  auto *C2 = dyn_cast<ConstantInt>(B2);
  if (!C1 || !C2)
    return std::nullopt;
  if (C1 > C2) {
    cast<IntrinsicInst>(II.getArgOperand(1))->setArgOperand(0, C1);
    return IC.replaceInstUsesWith(II, II.getArgOperand(1));
    /*II.setArgOperand(1, X);
    return {&II};*/
  }
  return IC.replaceInstUsesWith(II, II.getArgOperand(1));
}

/*
static std::optional<Instruction *>
instCombineSignExtend(InstCombiner &IC, IntrinsicInst &II) {
  // Match: evm_signextend(B1, evm_signextend(B2, X))
  Value *B1 = nullptr, *B2 = nullptr, *X = nullptr;
  if (!match(&II,
             m_Intrinsic<Intrinsic::evm_signextend>(
                 m_Value(B1),
                 m_Intrinsic<Intrinsic::evm_signextend>(m_Value(B2), m_Value(X)))))
    return std::nullopt;

  // Same SSA index -> outer is redundant.
  if (B1 == B2)
    return IC.replaceInstUsesWith(II, II.getArgOperand(1));

  // Both indices constant -> fold to min(B1,B2) (unsigned semantics).
  if (auto *C1 = dyn_cast<ConstantInt>(B1))
    if (auto *C2 = dyn_cast<ConstantInt>(B2)) {
      APInt Min = APIntOps::umin(C1->getValue(), C2->getValue());

      // If inner already uses the min, just drop the outer.
      if (Min == C2->getValue())
        return IC.replaceInstUsesWith(II, II.getArgOperand(1));

      // Otherwise: signextend(Min, X).
      auto *MinC = ConstantInt::get(B1->getType(), Min);
      II.setArgOperand(0, MinC); // B := min(B1,B2)
      II.setArgOperand(1, X);    // Y := X
      return &II;
    }

  return std::nullopt;
}
*/

std::optional<Instruction *>
EVMTTIImpl::instCombineIntrinsic(InstCombiner &IC, IntrinsicInst &II) const {
  if (II.getIntrinsicID() == Intrinsic::evm_signextend)
    return instCombineSignExtend(IC, II);

  return std::nullopt;
}

unsigned EVMTTIImpl::getAssumedAddrSpace(const Value *V) const {
  const auto *LD = dyn_cast<LoadInst>(V);
  if (!LD)
    return 0;

  return LD->getPointerAddressSpace();
}

InstructionCost EVMTTIImpl::getVectorInstrCost(unsigned Opcode, Type *Val,
                                               TTI::TargetCostKind CostKind,
                                               unsigned Index, Value *,
                                               Value *) {
  InstructionCost Cost = BasicTTIImplBase::getVectorInstrCost(
      Opcode, Val, CostKind, Index, nullptr, nullptr);
  return Cost + 25 * TargetTransformInfo::TCC_Expensive;
}

void EVMTTIImpl::getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                                         TTI::UnrollingPreferences &UP,
                                         OptimizationRemarkEmitter *ORE) {
  BaseT::getUnrollingPreferences(L, SE, UP, ORE);

  // Only allow unrolling small loops.
  UP.Threshold = 4;
  UP.MaxIterationsCountToAnalyze = 4;

  // Disable runtime, partial unrolling and unrolling using
  // trip count upper bound.
  UP.Partial = UP.Runtime = UP.UpperBound = false;
  UP.PartialThreshold = 0;

  // Avoid unrolling when optimizing for size.
  UP.OptSizeThreshold = 0;
  UP.PartialOptSizeThreshold = 0;
}

bool EVMTTIImpl::isLSRCostLess(const TargetTransformInfo::LSRCost &C1,
                               const TargetTransformInfo::LSRCost &C2) const {
  return C1.NumRegs < C2.NumRegs;
}

bool EVMTTIImpl::isNumRegsMajorCostOfLSR() const { return true; }
