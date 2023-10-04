//===--------- EraVMTargetTransformInfo.cpp - EraVM-specific TTI ----------===//
//
/// \file
/// This file defines the EraVM-specific TargetTransformInfo
/// implementation.
///
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

void EraVMTTIImpl::getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                                            TTI::UnrollingPreferences &UP,
                                            OptimizationRemarkEmitter *ORE) {
  BaseT::getUnrollingPreferences(L, SE, UP, ORE);

  // Only allow unrolling small loops.
  UP.Threshold = 40;
  UP.MaxIterationsCountToAnalyze = 32;

  // Disable runtime, partial unrolling and unrolling using
  // trip count upper bound.
  UP.Partial = UP.Runtime = UP.UpperBound = false;
  UP.PartialThreshold = 0;

  // Avoid unrolling when optimizing for size.
  UP.OptSizeThreshold = 0;
  UP.PartialOptSizeThreshold = 0;
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

InstructionCost
EraVMTTIImpl::getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
                                    TTI::TargetCostKind CostKind) {
  // TODO: CPR-1065 Implement.
  return BaseT::getIntrinsicInstrCost(ICA, CostKind);
}

InstructionCost EraVMTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
    TTI::OperandValueKind Opd1Info, TTI::OperandValueKind Opd2Info,
    TTI::OperandValueProperties Opd1PropInfo,
    TTI::OperandValueProperties Opd2PropInfo, ArrayRef<const Value *> Args,
    const Instruction *CxtI) {

  auto Cost = BasicTTIImplBase<EraVMTTIImpl>::getArithmeticInstrCost(
      Opcode, Ty, CostKind, Opd1Info, Opd2Info, Opd1PropInfo, Opd2PropInfo);

  switch (Opcode) {
  // signed instructions are generally expensive on EraVM
  case Instruction::SDiv:
  case Instruction::SRem:
  // arithmetic shifts are expensive
  case Instruction::AShr: {
    Cost = TargetTransformInfo::TCC_Expensive;
    break;
  }
  }
  return Cost;
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
  case Instruction::ICmp: {
    if (I) {
      const CmpInst *CI = cast<CmpInst>(I);
      switch (CI->getPredicate()) {
      case CmpInst::Predicate::ICMP_SGE:
      case CmpInst::Predicate::ICMP_SLE:
      case CmpInst::Predicate::ICMP_SGT:
      case CmpInst::Predicate::ICMP_SLT: {
        // signed comparisons are expensive
        return TargetTransformInfo::TCC_Expensive;
      }
      default: {
        return TargetTransformInfo::TCC_Basic;
      }
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
  return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind, I);
}

InstructionCost EraVMTTIImpl::getMemoryOpCost(unsigned Opcode, Type *Src,
                                              MaybeAlign Alignment,
                                              unsigned AddressSpace,
                                              TTI::TargetCostKind CostKind,
                                              const Instruction *I) {
  auto alignment_value = Alignment.valueOrOne().value();
  switch (AddressSpace) {
  case EraVMAS::AS_STACK:
  case EraVMAS::AS_CODE: {
    if ((alignment_value % 32) > 0) {
      // Estimate of the call to runtime function `__unaligned_store/load`
      return TargetTransformInfo::TCC_Expensive * 10;
    }
    break;
  }
  case EraVMAS::AS_HEAP:
  case EraVMAS::AS_HEAP_AUX:
  case EraVMAS::AS_GENERIC: {
    if ((alignment_value % 32) > 0) {
      return TargetTransformInfo::TCC_Basic * 2;
    }
  }
  }
  // aligned basic instructions cost 1 cycle
  return TargetTransformInfo::TCC_Basic;
}

InstructionCost EraVMTTIImpl::getVectorInstrCost(unsigned Opcode, Type *Val,
                                                 unsigned Index) {
  InstructionCost Cost =
      BasicTTIImplBase::getVectorInstrCost(Opcode, Val, Index);
  return Cost + 25 * TargetTransformInfo::TCC_Expensive;
}
