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
