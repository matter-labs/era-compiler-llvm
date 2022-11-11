//===-- EraVMTargetTransformInfo.h - EraVM-specific TTI ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EraVM-specific TargetTransformInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMTARGETTRANSFORMINFO_H

#include "EraVMTargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include <algorithm>

namespace llvm {

class EraVMTTIImpl final : public BasicTTIImplBase<EraVMTTIImpl> {
  using BaseT = BasicTTIImplBase<EraVMTTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;

  const EraVMSubtarget *ST;
  const EraVMTargetLowering *TLI;

  const EraVMSubtarget *getST() const { return ST; }
  const EraVMTargetLowering *getTLI() const { return TLI; }

public:
  EraVMTTIImpl(const EraVMTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  /// \name Scalar TTI Implementations
  /// @{

  // TODO: Implement more Scalar TTI for EraVM

  TTI::PopcntSupportKind getPopcntSupport(unsigned TyWidth) const;

  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind);

  bool hasDivRemOp(Type *DataType, bool IsSigned);

  InstructionCost getCmpSelInstrCost(unsigned Opcode, Type *ValTy, Type *CondTy,
                                     CmpInst::Predicate VecPred,
                                     TTI::TargetCostKind CostKind,
                                     const Instruction *I = nullptr);

  InstructionCost getMemoryOpCost(
      unsigned Opcode, Type *Src, MaybeAlign Alignment, unsigned AddressSpace,
      TTI::TargetCostKind CostKind,
      TTI::OperandValueInfo OpdInfo = {TTI::OK_AnyValue, TTI::OP_None},
      const Instruction *I = nullptr);

  InstructionCost getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
                                        TTI::TargetCostKind CostKind);

  bool allowsMisalignedMemoryAccesses(LLVMContext &Context, unsigned BitWidth,
                                      unsigned AddressSpace = 0,
                                      Align Alignment = Align(1),
                                      unsigned *Fast = nullptr) const {
    return true;
  }

  unsigned getAssumedAddrSpace(const Value *V) const;

  /// @}

  /// \name Vector TTI Implementations
  /// @{

  unsigned getNumberOfRegisters(unsigned ClassID) const;
  TypeSize getRegisterBitWidth(bool Vector) const;
  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty,
      TTI::TargetCostKind CostKind = TTI::TCK_SizeAndLatency,
      TTI::OperandValueInfo Opd1Info = {TTI::OK_AnyValue, TTI::OP_None},
      TTI::OperandValueInfo Opd2Info = {TTI::OK_AnyValue, TTI::OP_None},
      ArrayRef<const Value *> Args = ArrayRef<const Value *>(),
      const Instruction *CxtI = nullptr);
  InstructionCost getVectorInstrCost(unsigned Opcode, Type *Val,
                                     TTI::TargetCostKind CostKind,
                                     unsigned Index = -1, Value *Op0 = nullptr,
                                     Value *Op1 = nullptr);
  InstructionCost getVectorInstrCost(const Instruction &I, Type *Val,
                                     TTI::TargetCostKind CostKind,
                                     unsigned Index = -1) {
    return getVectorInstrCost(I.getOpcode(), Val, CostKind, Index);
  }

  Type *
  getMemcpyLoopLoweringType(LLVMContext &Context, Value *Length,
                            unsigned SrcAddrSpace, unsigned DestAddrSpace,
                            unsigned SrcAlign, unsigned DestAlign,
                            std::optional<uint32_t> AtomicElementSize) const {
    return IntegerType::get(Context, 256);
  }

  void getMemcpyLoopResidualLoweringType(
      SmallVectorImpl<Type *> &OpsOut, LLVMContext &Context,
      unsigned RemainingBytes, unsigned SrcAddrSpace, unsigned DestAddrSpace,
      unsigned SrcAlign, unsigned DestAlign,
      std::optional<uint32_t> AtomicCpySize) const {
    assert(RemainingBytes < 32);
    OpsOut.push_back(Type::getIntNTy(Context, RemainingBytes * 8));
  }

  // TODO: The value is copied from AMDGPU, needs to be configured.
  unsigned getInliningThresholdMultiplier() const { return 11; }

  /// @}
};

} // end namespace llvm

#endif
