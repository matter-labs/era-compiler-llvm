//----------------- EVMISelLowering.h - EVM DAG Lowering Interface -*- C++ -*-//
//
// This file defines the interfaces that EVM uses to lower LLVM
// code into a selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMISELLOWERING_H
#define LLVM_LIB_TARGET_EVM_EVMISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

namespace EVMISD {

enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
#define HANDLE_NODETYPE(NODE) NODE,
#include "EVMISD.def"
#undef HANDLE_NODETYPE
};

} // namespace EVMISD

class EVMSubtarget;

class EVMTargetLowering final : public TargetLowering {
public:
  EVMTargetLowering(const TargetMachine &TM, const EVMSubtarget &STI);

  /// getTargetNodeName - This method returns the name of a target specific
  /// DAG node.
  const char *getTargetNodeName(unsigned Opcode) const override;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override {
    return MVT::i256;
  }

  /// Return true if it is profitable to move this shift by a constant amount
  /// through its operand, adjusting any immediate operands as necessary to
  /// preserve semantics. This transformation may not be desirable if it
  /// disrupts a particularly auspicious target-specific tree (e.g. bitfield
  /// extraction in AArch64). By default, it returns true.
  /// For EVM this may result in the creation of a big immediate,
  /// which is not profitable.
  virtual bool
  isDesirableToCommuteWithShift(const SDNode *N,
                                CombineLevel Level) const override {
    return false;
  }

  /// Return true if creating a shift of the type by the given
  /// amount is not profitable.
  bool shouldAvoidTransformToShift(EVT VT, unsigned Amount) const override {
    return true;
  }

  /// Return true if it is profitable to fold a pair of shifts into a mask.
  /// This is usually true on most targets. But some targets, like Thumb1,
  /// have immediate shift instructions, but no immediate "and" instruction;
  /// this makes the fold unprofitable.
  /// For EVM this may result in the creation of a big immediate,
  /// which is not profitable.
  virtual bool
  shouldFoldConstantShiftPairToMask(const SDNode *N,
                                    CombineLevel Level) const override {
    return false;
  }

private:
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerLOAD(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerSTORE(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &dl, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
                      SelectionDAG &DAG) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  MachineBasicBlock *emitSelect(MachineInstr &MI, MachineBasicBlock *BB) const;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMISELLOWERING_H
