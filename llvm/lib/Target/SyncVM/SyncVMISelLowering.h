//===-- SyncVMISelLowering.h - SyncVM DAG Lowering Interface ----*- C++ -*-===//
//
// This file defines the interfaces that SyncVM uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMISELLOWERING_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMISELLOWERING_H

#include "SyncVM.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

namespace SyncVMISD {

enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
#define HANDLE_NODETYPE(NODE) NODE,
#include "SyncVMISD.def"
#undef HANDLE_NODETYPE
};

} // namespace SyncVMISD

class SyncVMSubtarget;
class SyncVMTargetLowering : public TargetLowering {
public:
  explicit SyncVMTargetLowering(const TargetMachine &TM,
                                const SyncVMSubtarget &STI);

  MVT getScalarShiftAmountTy(const DataLayout &, EVT) const override {
    return MVT::i256;
  }

  MVT::SimpleValueType getCmpLibcallReturnType() const override {
    return MVT::i256;
  }

  /// LowerOperation - Provide custom lowering hooks for some operations.
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  /// getTargetNodeName - This method returns the name of a target specific
  /// DAG node.
  const char *getTargetNodeName(unsigned Opcode) const override;

  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerCopyToReg(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerLOAD(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSTORE(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerZERO_EXTEND(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerANY_EXTEND(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerSRA(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSDIV(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSREM(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerINTRINSIC_VOID(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSTACKSAVE(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSTACKRESTORE(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerConstant(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerBrFlag(SDValue Cond, SDValue Chain, SDValue DestFalse,
                      SDValue DestTrue, SDLoc DL, SelectionDAG &DAG) const;

  TargetLowering::ConstraintType
  getConstraintType(StringRef Constraint) const override {
    return TargetLowering::ConstraintType::C_Unknown;
  }

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override {
    std::pair<unsigned, const TargetRegisterClass *> result;
    return result;
  }

  /// isTruncateFree - Return true if it's free to truncate a value of type
  /// Ty1 to type Ty2. e.g. On syncvm it's free to truncate a i16 value in
  /// register R15W to i8 by referencing its sub-register R15B.
  bool isTruncateFree(Type *Ty1, Type *Ty2) const override { return false; }
  bool isTruncateFree(EVT VT1, EVT VT2) const override { return false; }

  /// isZExtFree - Return true if any actual instruction that defines a value
  /// of type Ty1 implicit zero-extends the value to Ty2 in the result
  /// register. This does not necessarily include registers defined in unknown
  /// ways, such as incoming arguments, or copies from unknown virtual
  /// registers. Also, if isTruncateFree(Ty2, Ty1) is true, this does not
  /// necessarily apply to truncate instructions. e.g. on syncvm, all
  /// instructions that define 8-bit values implicit zero-extend the result
  /// out to 16 bits.
  bool isZExtFree(Type *Ty1, Type *Ty2) const override { return false; }
  bool isZExtFree(EVT VT1, EVT VT2) const override { return false; }
  bool isZExtFree(SDValue Val, EVT VT2) const override { return false; }

  bool isIntDivCheap(EVT VT, AttributeList Attr) const override { return true; }

  bool isLegalICmpImmediate(int64_t) const override { return false; }
  bool shouldAvoidTransformToShift(EVT VT, unsigned Amount) const override {
    return true;
  }

  /// Returns true if we should normalize
  /// select(N0&N1, X, Y) => select(N0, select(N1, X, Y), Y) and
  /// select(N0|N1, X, Y) => select(N0, select(N1, X, Y, Y)) if it is likely
  /// that it saves us from materializing N0 and N1 in an integer register.
  /// Targets that are able to perform and/or on flags should return false here.
  bool shouldNormalizeToSelectSequence(LLVMContext &Context,
                                       EVT VT) const override {
    return false;
  }

  /// Use bitwise logic to make pairs of compares more efficient. For example:
  /// and (seteq A, B), (seteq C, D) --> seteq (or (xor A, B), (xor C, D)), 0
  /// This should be true when it takes more than one instruction to lower
  /// setcc (cmp+set on x86 scalar), when bitwise ops are faster than logic on
  /// condition bits (crand on PowerPC), and/or when reducing cmp+br is a win.
  bool convertSetCCLogicToBitwiseLogic(EVT VT) const override { return true; }

  /// There are two ways to clear extreme bits (either low or high):
  /// Mask:    x &  (-1 << y)  (the instcombine canonical form)
  /// Shifts:  x >> y << y
  /// Return true if the variant with 2 variable shifts is preferred.
  /// Return false if there is no preference.
  bool shouldFoldMaskToVariableShiftPair(SDValue X) const override {
    return true;
  }

  /// Return true if it is beneficial to convert a load of a constant to
  /// just the constant itself.
  bool shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                         Type *Ty) const override {
    // 16-bit or smaller immediates can be loaded with 1 instruction
    if (isInt<128>(Imm.getSExtValue()) || isInt<128>(Imm.getZExtValue())) {
      return true;
    }
    return false;
  }

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  bool allowsMemoryAccess(LLVMContext &Context, const DataLayout &DL, EVT VT,
                          unsigned AddrSpace, Align Alignment,
                          MachineMemOperand::Flags Flags,
                          bool *Fast) const override {
    return true;
  }

  /// allowsMisalignedMemoryAccesses - Returns true if the target allows
  /// unaligned memory accesses of the specified type. Returns whether it
  /// is "fast" by reference in the second argument.
  bool allowsMisalignedMemoryAccesses(EVT VT, unsigned AddrSpace,
                                      Align Alignment,
                                      MachineMemOperand::Flags Flags,
                                      bool *Fast) const override {
    return AddrSpace == SyncVMAS::AS_HEAP ||
           AddrSpace == SyncVMAS::AS_HEAP_AUX ||
           AddrSpace == SyncVMAS::AS_GENERIC;
  }

  /// If a physical register, this returns the register that receives the
  /// exception address on entry to an EH pad.
  Register
  getExceptionPointerRegister(const Constant *PersonalityFn) const override {
    return SyncVM::R1;
  }

private:
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &dl, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
                      SelectionDAG &DAG) const override;

  bool getPostIndexedAddressParts(SDNode *N, SDNode *Op, SDValue &Base,
                                  SDValue &Offset, ISD::MemIndexedMode &AM,
                                  SelectionDAG &DAG) const override {
    return false;
  }

  /// SyncVM does nit benefit from mulhs, so change BuildSDIV implementation
  /// to preserve sdiv.
  SDValue BuildSDIV(SDNode *N, SelectionDAG &DAG, bool IsAfterLegalization,
                    SmallVectorImpl<SDNode *> &Created) const override {
    return {};
  }

  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;
  SDValue combineADD(SDNode *N, DAGCombinerInfo &DCI) const;
  SDValue combineSUB(SDNode *N, DAGCombinerInfo &DCI) const;
};
} // namespace llvm

#endif
