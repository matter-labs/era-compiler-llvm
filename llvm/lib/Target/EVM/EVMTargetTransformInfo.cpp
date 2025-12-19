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

static std::optional<Instruction *> foldSignExtendToConst(InstCombiner &IC,
                                                          IntrinsicInst &II) {
  constexpr unsigned BitWidth = 256;
  if (!II.getType()->isIntegerTy(BitWidth))
    return std::nullopt;

  const auto *ByteIdxC = dyn_cast<ConstantInt>(II.getArgOperand(0));
  if (!ByteIdxC)
    return std::nullopt;

  // ByteIdx must be in range [0, 31].
  uint64_t ByteIdx = ByteIdxC->getZExtValue();
  if (ByteIdx >= BitWidth / 8)
    return std::nullopt;

  // Compute known bits of the input.
  KnownBits Known(BitWidth);
  IC.computeKnownBits(II.getArgOperand(1), Known, 0, &II);

  unsigned Width = (ByteIdx + 1) * 8;
  APInt LowMask = APInt::getLowBitsSet(BitWidth, Width);
  if (((Known.Zero | Known.One) & LowMask) == LowMask) {
    APInt Folded = (Known.One & LowMask).trunc(Width).sext(BitWidth);
    return IC.replaceInstUsesWith(II, ConstantInt::get(II.getType(), Folded));
  }
  return std::nullopt;
}

static std::optional<Instruction *> instCombineSignExtend(InstCombiner &IC,
                                                          IntrinsicInst &II) {
  constexpr unsigned BitWidth = 256;
  if (!II.getType()->isIntegerTy(BitWidth))
    return std::nullopt;

  // Fold signextend(b, signextend(b, x)) -> signextend(b, x)
  Value *B = nullptr, *X = nullptr;
  if (match(&II, m_Intrinsic<Intrinsic::evm_signextend>(
                     m_Value(B), m_Intrinsic<Intrinsic::evm_signextend>(
                                     m_Deferred(B), m_Value(X)))))
    return IC.replaceInstUsesWith(II, II.getArgOperand(1));

  // From now on, we only handle signextend with constant byte index.
  const auto *ByteIdxC = dyn_cast<ConstantInt>(II.getArgOperand(0));
  if (!ByteIdxC)
    return std::nullopt;

  // ByteIdx must be in range [0, 31].
  uint64_t ByteIdx = ByteIdxC->getZExtValue();
  if (ByteIdx >= BitWidth / 8)
    return std::nullopt;

  unsigned Width = (ByteIdx + 1) * 8;

  // Fold signextend into shifts, if second operand or the result is shift
  // with constant. LLVM will fuse those shifts, and will replace signextend
  // with a shift, which is cheaper.
  if (match(II.getArgOperand(1),
            m_OneUse(m_Shift(m_Value(), m_ConstantInt()))) ||
      (II.hasOneUse() &&
       match(II.user_back(), m_Shift(m_Specific(&II), m_ConstantInt())))) {
    unsigned ShiftAmt = BitWidth - Width;
    auto *Shl = IC.Builder.CreateShl(II.getArgOperand(1), ShiftAmt);
    auto *Ashr = IC.Builder.CreateAShr(Shl, ShiftAmt);
    return IC.replaceInstUsesWith(II, Ashr);
  }

  const APInt *AndVal = nullptr;
  // Match signextend(b, and(x, C))
  if (match(II.getArgOperand(1), m_And(m_Value(X), m_APInt(AndVal)))) {
    APInt LowMask = APInt::getLowBitsSet(BitWidth, Width);

    // signextend(b, x & C) -> signextend(b, x)
    // If and fully preservs low bits, we can drop it.
    if ((*AndVal & LowMask) == LowMask)
      return IC.replaceOperand(II, 1, X);

    // signextend(b, x & C) -> (x & C)
    // If and doesn't touch upper bits, and clears sign bit, we can drop
    // signextend.
    APInt SignBit = APInt(BitWidth, 1).shl(Width - 1);
    if ((*AndVal & ~LowMask).isZero() && (*AndVal & SignBit).isZero())
      return IC.replaceInstUsesWith(II, II.getArgOperand(1));

    // signextend(b, x & C) -> 0
    // If and clears all low bits, result is always 0.
    if ((*AndVal & LowMask).isZero())
      return IC.replaceInstUsesWith(II,
                                    ConstantInt::getNullValue(II.getType()));
  }

  // and(signextend(b, x), C) -> and(x, C)
  // If and doesn't touch upper bits, we can drop signextend.
  if (II.hasOneUse() &&
      match(II.user_back(), m_And(m_Specific(&II), m_APInt(AndVal)))) {
    APInt LowMask = APInt::getLowBitsSet(BitWidth, Width);
    if ((*AndVal & ~LowMask).isZero())
      return IC.replaceInstUsesWith(II, II.getArgOperand(1));
  }
  return foldSignExtendToConst(IC, II);
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
