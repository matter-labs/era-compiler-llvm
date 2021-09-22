//===---- SyncVMTargetTransformInfo.h - SyncVM-specific TTI -*- C++ -*-----===//
//
/// \file
/// This file a TargetTransformInfo::Concept conforming object specific
/// to the SyncVM target machine.
///
/// It uses the target's detailed information to provide more precise answers to
/// certain TTI queries, while letting the target independent and default TTI
/// implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMTARGETTRANSFORMINFO_H

#include "SyncVMTargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include <algorithm>

namespace llvm {

class SyncVMTTIImpl final : public BasicTTIImplBase<SyncVMTTIImpl> {
  typedef BasicTTIImplBase<SyncVMTTIImpl> BaseT;
  typedef TargetTransformInfo TTI;
  friend BaseT;

  const SyncVMSubtarget *ST;
  const SyncVMTargetLowering *TLI;

  const SyncVMSubtarget *getST() const { return ST; }
  const SyncVMTargetLowering *getTLI() const { return TLI; }

public:
  SyncVMTTIImpl(const SyncVMTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  /// \name Scalar TTI Implementations
  /// @{

  // TODO: Implement more Scalar TTI for SyncVM

  TTI::PopcntSupportKind getPopcntSupport(unsigned TyWidth) const;

  /// @}

  /// \name Vector TTI Implementations
  /// @{

  unsigned getNumberOfRegisters(unsigned ClassID) const;
  TypeSize getRegisterBitWidth(bool Vector) const;
  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty,
      TTI::TargetCostKind CostKind = TTI::TCK_SizeAndLatency,
      TTI::OperandValueKind Opd1Info = TTI::OK_AnyValue,
      TTI::OperandValueKind Opd2Info = TTI::OK_AnyValue,
      TTI::OperandValueProperties Opd1PropInfo = TTI::OP_None,
      TTI::OperandValueProperties Opd2PropInfo = TTI::OP_None,
      ArrayRef<const Value *> Args = ArrayRef<const Value *>(),
      const Instruction *CxtI = nullptr);
  InstructionCost getVectorInstrCost(unsigned Opcode, Type *Val,
                                     unsigned Index);

  Type *getMemcpyLoopLoweringType(LLVMContext &Context, Value *Length,
                                  unsigned SrcAddrSpace, unsigned DestAddrSpace,
                                  unsigned SrcAlign, unsigned DestAlign) const {
    return IntegerType::get(Context, 256);
  }

  void getMemcpyLoopResidualLoweringType(
      SmallVectorImpl<Type *> &OpsOut, LLVMContext &Context,
      unsigned RemainingBytes, unsigned SrcAddrSpace, unsigned DestAddrSpace,
      unsigned SrcAlign, unsigned DestAlign) const {
    assert(RemainingBytes < 32);
    OpsOut.push_back(Type::getIntNTy(Context, RemainingBytes * 8));
  }

  // TODO: The value is copied from AMDGPU, needs to be configured.
  unsigned getInliningThresholdMultiplier() const { return 11; }

  /// @}
};

} // end namespace llvm

#endif
