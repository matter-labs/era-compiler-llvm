//===--------------- EVMTargetTransformInfo.cpp - EVM-specific TTI --------===//
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
                                               unsigned Index) {
  const InstructionCost Cost =
      BasicTTIImplBase::getVectorInstrCost(Opcode, Val, Index);
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
