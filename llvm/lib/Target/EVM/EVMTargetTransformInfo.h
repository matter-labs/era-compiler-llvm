//===---------- EVMTargetTransformInfo.h - EVM-specific TTI -*- C++ -*-----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file a TargetTransformInfo::Concept conforming object specific
// to the EVM target machine.
//
// It uses the target's detailed information to provide more precise answers to
// certain TTI queries, while letting the target independent and default TTI
// implementations handle the rest.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_EVM_EVMTARGETTRANSFORMINFO_H

#include "EVMTargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/Module.h"

namespace llvm {

class EVMTTIImpl final : public BasicTTIImplBase<EVMTTIImpl> {
  using BaseT = BasicTTIImplBase<EVMTTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;

  const EVMSubtarget *ST;
  const EVMTargetLowering *TLI;

  const EVMSubtarget *getST() const { return ST; }
  const EVMTargetLowering *getTLI() const { return TLI; }

public:
  enum EVMRegisterClass { Vector /* Unsupported */, GPR };

  EVMTTIImpl(const EVMTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  bool allowsMisalignedMemoryAccesses(LLVMContext &Context, unsigned BitWidth,
                                      unsigned AddressSpace = 0,
                                      Align Alignment = Align(1),
                                      unsigned *Fast = nullptr) const {
    return true;
  }

  unsigned getAssumedAddrSpace(const Value *V) const;

  unsigned getNumberOfRegisters(unsigned ClassID) const {
    return ClassID == Vector ? 0 : 1;
  }

  TypeSize getRegisterBitWidth(TargetTransformInfo::RegisterKind RK) const {
    return TypeSize::getFixed(256);
  }

  unsigned getRegisterClassForType(bool IsVector, Type *Ty = nullptr) const {
    if (IsVector)
      return Vector;
    return GPR;
  }

  const char *getRegisterClassName(unsigned ClassID) const {
    if (ClassID == GPR) {
      return "EVM::GPR";
    }
    llvm_unreachable("unknown register class");
  }

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

  void getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                               TTI::UnrollingPreferences &UP,
                               OptimizationRemarkEmitter *ORE);

  /// Return true if LSR cost of C1 is lower than C1.
  bool isLSRCostLess(const TargetTransformInfo::LSRCost &C1,
                     const TargetTransformInfo::LSRCost &C2) const;

  /// Return true if LSR major cost is number of registers. Targets which
  /// implement their own isLSRCostLess and unset number of registers as major
  /// cost should return false, otherwise return true.
  bool isNumRegsMajorCostOfLSR() const;
};

} // end namespace llvm

#endif
