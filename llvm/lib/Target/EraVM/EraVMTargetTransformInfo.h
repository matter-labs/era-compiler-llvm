//===---- EraVMTargetTransformInfo.h - EraVM-specific TTI -*- C++ -*-------===//
//
/// \file
/// This file a TargetTransformInfo::Concept conforming object specific
/// to the EraVM target machine.
///
/// It uses the target's detailed information to provide more precise answers to
/// certain TTI queries, while letting the target independent and default TTI
/// implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMTARGETTRANSFORMINFO_H

#include "EraVMTargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include <algorithm>

namespace llvm {

class EraVMTTIImpl final : public BasicTTIImplBase<EraVMTTIImpl> {
  typedef BasicTTIImplBase<EraVMTTIImpl> BaseT;
  typedef TargetTransformInfo TTI;
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

  // TODO: CPR-1358 Implement more Scalar TTI for EraVM

  TTI::PopcntSupportKind getPopcntSupport(unsigned TyWidth) const;

  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind);

  bool hasDivRemOp(Type *DataType, bool IsSigned);

  InstructionCost getCmpSelInstrCost(unsigned Opcode, Type *ValTy, Type *CondTy,
                                     CmpInst::Predicate VecPred,
                                     TTI::TargetCostKind CostKind,
                                     const Instruction *I = nullptr);

  InstructionCost getMemoryOpCost(unsigned Opcode, Type *Src,
                                  MaybeAlign Alignment, unsigned AddressSpace,
                                  TTI::TargetCostKind CostKind,
                                  const Instruction *I = nullptr);

  InstructionCost getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
                                        TTI::TargetCostKind CostKind);

  bool allowsMisalignedMemoryAccesses(LLVMContext &Context, unsigned BitWidth,
                                      unsigned AddressSpace = 0,
                                      Align Alignment = Align(1),
                                      bool *Fast = nullptr) const {
    return true;
  }

  unsigned getAssumedAddrSpace(const Value *V) const;

  void getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                               TTI::UnrollingPreferences &UP,
                               OptimizationRemarkEmitter *ORE);

  /// @}

  /// \name Vector TTI Implementations
  /// @{

  enum EraVMRegisterClass { Vector /* Unsupported */, GPR };
  unsigned getNumberOfRegisters(unsigned ClassID) const {
    return ClassID == Vector ? 0 : 15;
  }
  TypeSize getRegisterBitWidth(TargetTransformInfo::RegisterKind RK) const {
    assert(RK == TargetTransformInfo::RGK_Scalar &&
           "Vector registers aren't supported");
    return TypeSize::Fixed(256);
  }
  unsigned getRegisterClassForType(bool IsVector, Type *Ty = nullptr) const {
    if (IsVector)
      return Vector;
    return GPR;
  }

  const char *getRegisterClassName(unsigned ClassID) const {
    if (ClassID == GPR) {
      return "EraVM::GPR";
    }
    llvm_unreachable("unknown register class");
  }

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
                                  unsigned SrcAlign, unsigned DestAlign,
                                  Optional<uint32_t> AtomicElementSize) const {
    return IntegerType::get(Context, 256);
  }

  void getMemcpyLoopResidualLoweringType(
      SmallVectorImpl<Type *> &OpsOut, LLVMContext &Context,
      unsigned RemainingBytes, unsigned SrcAddrSpace, unsigned DestAddrSpace,
      unsigned SrcAlign, unsigned DestAlign,
      Optional<uint32_t> AtomicCpySize) const {
    assert(RemainingBytes < 32);
    OpsOut.push_back(Type::getIntNTy(Context, RemainingBytes * 8));
  }

  // Since we prefer inlining, use larger threshold multiplier.
  unsigned getInliningThresholdMultiplier() const { return 11; }

  /// @}
};

} // end namespace llvm

#endif
