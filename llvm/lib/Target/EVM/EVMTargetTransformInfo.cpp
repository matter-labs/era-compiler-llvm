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
  unsigned BitWidth = II.getType()->getIntegerBitWidth();
  if (BitWidth != 256)
    return std::nullopt;

  // Unfold signextend(c, x) ->
  //        ashr(shl(x, 256 - (c + 1) * 8), 256 - (c + 1) * 8)
  // where c is a constant integer.
  ConstantInt *C = nullptr;
  if (match(II.getArgOperand(0), m_ConstantInt(C))) {
    const APInt &B = C->getValue();

    // If the signextend is larger than 31 bits, leave constant
    // folding to handle it.
    if (B.uge(APInt(BitWidth, (BitWidth / 8) - 1)))
      return std::nullopt;

    unsigned ShiftAmt = BitWidth - ((B.getZExtValue() + 1) * 8);
    auto *Shl = IC.Builder.CreateShl(II.getArgOperand(1), ShiftAmt);
    auto *Ashr = IC.Builder.CreateAShr(Shl, ShiftAmt);
    return IC.replaceInstUsesWith(II, Ashr);
  }

  // Fold signextend(b, signextend(b, x)) -> signextend(b, x)
  Value *B = nullptr, *X = nullptr;
  if (match(&II, m_Intrinsic<Intrinsic::evm_signextend>(
                     m_Value(B), m_Intrinsic<Intrinsic::evm_signextend>(
                                     m_Deferred(B), m_Value(X)))))
    return IC.replaceInstUsesWith(II, II.getArgOperand(1));

  return std::nullopt;
}

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
