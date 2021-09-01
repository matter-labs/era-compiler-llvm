//===--------- SyncVMTargetTransformInfo.cpp - SyncVM-specific TTI --------===//
//
/// \file
/// This file defines the SyncVM-specific TargetTransformInfo
/// implementation.
///
//===----------------------------------------------------------------------===//

#include "SyncVMTargetTransformInfo.h"
#include "llvm/CodeGen/CostTable.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "syncvmtti"

TargetTransformInfo::PopcntSupportKind
SyncVMTTIImpl::getPopcntSupport(unsigned TyWidth) const {
  assert(isPowerOf2_32(TyWidth) && "Ty width must be power of 2");
  return TargetTransformInfo::PSK_FastHardware;
}

unsigned SyncVMTTIImpl::getNumberOfRegisters(unsigned ClassID) const {
  unsigned Result = BaseT::getNumberOfRegisters(ClassID);
  return Result;
}

unsigned SyncVMTTIImpl::getRegisterBitWidth(bool Vector) const {
  (void)Vector;
  return 256;
}

unsigned SyncVMTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
    TTI::OperandValueKind Opd1Info, TTI::OperandValueKind Opd2Info,
    TTI::OperandValueProperties Opd1PropInfo,
    TTI::OperandValueProperties Opd2PropInfo, ArrayRef<const Value *> Args,
    const Instruction *CxtI) {

  unsigned Cost = BasicTTIImplBase<SyncVMTTIImpl>::getArithmeticInstrCost(
      Opcode, Ty, CostKind, Opd1Info, Opd2Info, Opd1PropInfo, Opd2PropInfo);

  switch (Opcode) {
  case Instruction::AShr:
    Cost = TargetTransformInfo::TCC_Expensive;
    break;
  }
  return Cost;
}

unsigned SyncVMTTIImpl::getVectorInstrCost(unsigned Opcode, Type *Val,
                                           unsigned Index) {
  unsigned Cost = BasicTTIImplBase::getVectorInstrCost(Opcode, Val, Index);
  return Cost + 25 * TargetTransformInfo::TCC_Expensive;
}
