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
using namespace llvm;

#define DEBUG_TYPE "evmtti"

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
