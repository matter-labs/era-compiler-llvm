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
  class SyncVMTargetLowering : public TargetLowering {
  public:
    explicit SyncVMTargetLowering(const TargetMachine &TM);

    MVT getScalarShiftAmountTy(const DataLayout &, EVT) const override {
      return MVT::i8;
    }

    MVT::SimpleValueType getCmpLibcallReturnType() const override {
      return MVT::i16;
    }

    /// LowerOperation - Provide custom lowering hooks for some operations.
    SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override {}

    /// getTargetNodeName - This method returns the name of a target specific
    /// DAG node.
    const char *getTargetNodeName(unsigned Opcode) const override {}

    TargetLowering::ConstraintType
    getConstraintType(StringRef Constraint) const override {}

    std::pair<unsigned, const TargetRegisterClass *>
    getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                 StringRef Constraint, MVT VT) const override {}

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

    bool isLegalICmpImmediate(int64_t) const override { return false; }
    bool shouldAvoidTransformToShift(EVT VT, unsigned Amount) const override { return false; }

    MachineBasicBlock *
    EmitInstrWithCustomInserter(MachineInstr &MI,
                                MachineBasicBlock *BB) const override { return nullptr; }

  private:

    SDValue
    LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                         const SmallVectorImpl<ISD::InputArg> &Ins,
                         const SDLoc &dl, SelectionDAG &DAG,
                         SmallVectorImpl<SDValue> &InVals) const override {}
    SDValue
      LowerCall(TargetLowering::CallLoweringInfo &CLI,
                SmallVectorImpl<SDValue> &InVals) const override {}

    bool CanLowerReturn(CallingConv::ID CallConv,
                        MachineFunction &MF,
                        bool IsVarArg,
                        const SmallVectorImpl<ISD::OutputArg> &Outs,
                        LLVMContext &Context) const override { return false; }

    SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                        const SmallVectorImpl<ISD::OutputArg> &Outs,
                        const SmallVectorImpl<SDValue> &OutVals,
                        const SDLoc &dl, SelectionDAG &DAG) const override;

    bool getPostIndexedAddressParts(SDNode *N, SDNode *Op,
                                    SDValue &Base,
                                    SDValue &Offset,
                                    ISD::MemIndexedMode &AM,
                                    SelectionDAG &DAG) const override { return false; }
  };
} // namespace llvm

#endif
