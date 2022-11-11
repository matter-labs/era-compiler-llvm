//===-- EraVMTargetTransformInfo.cpp - EraVM-specific TTI -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM-specific TargetTransformInfo.
//
//===----------------------------------------------------------------------===//

#include "EraVMTargetTransformInfo.h"
#include "llvm/CodeGen/CostTable.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "eravmtti"

TargetTransformInfo::PopcntSupportKind
EraVMTTIImpl::getPopcntSupport(unsigned TyWidth) const {
  assert(isPowerOf2_32(TyWidth) && "Ty width must be power of 2");
  return TargetTransformInfo::PSK_FastHardware;
}

unsigned EraVMTTIImpl::getAssumedAddrSpace(const Value *V) const {
  const auto *LD = dyn_cast<LoadInst>(V);
  if (!LD)
    return 0;

  return LD->getPointerAddressSpace();
}

InstructionCost EraVMTTIImpl::getIntImmCost(const APInt &Imm, Type *Ty,
                                            TTI::TargetCostKind CostKind) {
  assert(Ty->isIntegerTy());

  unsigned BitSize = Ty->getPrimitiveSizeInBits();
  // There is no cost model for constants with a bit size of 0. Return TCC_Free
  // here, so that constant hoisting will ignore this constant.
  if (BitSize == 0)
    return TTI::TCC_Free;
  // No cost model for operations on integers larger than 256 bit implemented
  // yet.
  if (BitSize > 256)
    return TTI::TCC_Free;

  if (Imm == 0) // $r0 is free
    return TTI::TCC_Free;

  if (isInt<128>(Imm.getSExtValue()) || isInt<128>(Imm.getZExtValue())) {
    return TTI::TCC_Basic;
  }

  return TTI::TCC_Expensive;
}

unsigned EraVMTTIImpl::getNumberOfRegisters(unsigned ClassID) const {
  unsigned Result = BaseT::getNumberOfRegisters(ClassID);
  return Result;
}

TypeSize EraVMTTIImpl::getRegisterBitWidth(bool) const {
  return TypeSize::getFixed(256);
}

InstructionCost
EraVMTTIImpl::getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
                                    TTI::TargetCostKind CostKind) {
  // TODO
  return BaseT::getIntrinsicInstrCost(ICA, CostKind);
}

InstructionCost EraVMTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
    TTI::OperandValueInfo Opd1Info, TTI::OperandValueInfo Opd2Info,
    ArrayRef<const Value *> Args, const Instruction *CxtI) {

  switch (Opcode) {
  default:
    return BasicTTIImplBase<EraVMTTIImpl>::getArithmeticInstrCost(
        Opcode, Ty, CostKind, Opd1Info, Opd2Info);
  // signed instructions are generally expensive on EraVM
  case Instruction::SDiv:
  case Instruction::SRem:
  // arithmetic shifts are expensive
  case Instruction::AShr:
    return TargetTransformInfo::TCC_Expensive;
  }
  llvm_unreachable("unhandled instruction");
}

bool EraVMTTIImpl::hasDivRemOp(Type *DataType, bool IsSigned) {
  EVT VT = TLI->getValueType(DL, DataType);
  return (VT.isScalarInteger() && TLI->isTypeLegal(VT));
}

InstructionCost EraVMTTIImpl::getCmpSelInstrCost(unsigned Opcode, Type *ValTy,
                                                 Type *CondTy,
                                                 CmpInst::Predicate VecPred,
                                                 TTI::TargetCostKind CostKind,
                                                 const Instruction *I) {

  switch (Opcode) {
  default:
    return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind,
                                     I);
  case Instruction::ICmp: {
    if (I) {
      const auto *CI = cast<CmpInst>(I);
      switch (CI->getPredicate()) {
      case CmpInst::Predicate::ICMP_SGE:
      case CmpInst::Predicate::ICMP_SLE:
      case CmpInst::Predicate::ICMP_SGT:
      case CmpInst::Predicate::ICMP_SLT: {
        // signed comparisons are expensive
        return TargetTransformInfo::TCC_Expensive;
      }
      default:
        return TargetTransformInfo::TCC_Basic;
      }
    } else {
      // signed comparisons are expensive. However we cannot distinguish
      // signed and unsigned predicate types (because the predicate is not
      // passed in, and the `VecPred` is actually not used for Scalar), so
      // here we can only have one rough estimate.

      // For unsigned predicates, it is simply 1 instruction; but for signed,
      // it will be much more
      return TargetTransformInfo::TCC_Expensive;
    }
  }
  case Instruction::Select: {
    return TargetTransformInfo::TCC_Expensive;
  }
  }
  llvm_unreachable("unhandled instruction");
}

InstructionCost EraVMTTIImpl::getMemoryOpCost(unsigned Opcode, Type *Src,
                                              MaybeAlign Alignment,
                                              unsigned AddressSpace,
                                              TTI::TargetCostKind CostKind,
                                              TTI::OperandValueInfo OpdInfo,
                                              const Instruction *I) {
  auto AlignmentValue = Alignment.valueOrOne().value();
  switch (AddressSpace) {
  default:
    llvm_unreachable("unsupported");
  case EraVMAS::AS_STACK:
  case EraVMAS::AS_CODE: {
    if ((AlignmentValue % 32) > 0) {
      // Estimate of the call to runtime function `__unaligned_store/load`
      return TargetTransformInfo::TCC_Expensive * 10;
    }
    break;
  }
  case EraVMAS::AS_HEAP:
  case EraVMAS::AS_CALLDATA:
  case EraVMAS::AS_RETDATA: {
    if ((AlignmentValue % 32) > 0)
      return TargetTransformInfo::TCC_Basic * 2;
  }
  }
  // aligned basic instructions cost 1 cycle
  return TargetTransformInfo::TCC_Basic;
}

InstructionCost EraVMTTIImpl::getVectorInstrCost(unsigned Opcode, Type *Val,
                                                 TTI::TargetCostKind CostKind,
                                                 unsigned Index, Value *,
                                                 Value *) {
  InstructionCost Cost = BasicTTIImplBase::getVectorInstrCost(
      Opcode, Val, CostKind, Index, nullptr, nullptr);
  return Cost + 25 * TargetTransformInfo::TCC_Expensive;
}
