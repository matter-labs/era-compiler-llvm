//----------------- EVMISelLowering.h - EVM DAG Lowering Interface -*- C++ -*-//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that EVM uses to lower LLVM
// code into a selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMISELLOWERING_H
#define LLVM_LIB_TARGET_EVM_EVMISELLOWERING_H

#include "EVM.h"
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
  bool isDesirableToCommuteWithShift(const SDNode *N,
                                     CombineLevel Level) const override {
    return false;
  }

  /// Return true if integer divide is usually cheaper than a sequence of
  /// several shifts, adds, and multiplies for this target.
  /// The definition of "cheaper" may depend on whether we're optimizing
  /// for speed or for size.
  bool isIntDivCheap(EVT VT, AttributeList Attr) const override { return true; }

  /// There are two ways to clear extreme bits (either low or high):
  /// Mask:  x & (-1 << y) (the instcombine canonical form)
  /// Shifts: x >> y << y
  /// Return true if the variant with 2 variable shifts is preferred.
  /// Return false if there is no preference.
  bool shouldFoldMaskToVariableShiftPair(SDValue X) const override {
    return true;
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
  bool shouldFoldConstantShiftPairToMask(const SDNode *N,
                                         CombineLevel Level) const override {
    return false;
  }

  /// Determines the optimal series of memory ops to replace the
  /// memset / memcpy. Return true if the number of memory ops is below the
  /// threshold (Limit). Note that this is always the case when Limit is ~0.
  /// It returns the types of the sequence of memory ops to perform
  /// memset / memcpy by reference.
  bool
  findOptimalMemOpLowering(std::vector<EVT> &MemOps, unsigned Limit,
                           const MemOp &Op, unsigned DstAS, unsigned SrcAS,
                           const AttributeList &FuncAttributes) const override {
    // Don't expand memcpy into scalar loads/stores, as it is mapped 1-to-1 to
    // the corresponding EVM instruction (MCOPY, CALLDATACOPY, RETURNDATACOPY,
    // or CODECOPY).
    return false;
  }

  /// allowsMisalignedMemoryAccesses - Returns true if the target allows
  /// unaligned memory accesses of the specified type. Returns whether it
  /// is "fast" by reference in the second argument.
  bool allowsMisalignedMemoryAccesses(EVT VT, unsigned AddrSpace,
                                      Align Alignment,
                                      MachineMemOperand::Flags Flags,
                                      unsigned *Fast) const override {
    return AddrSpace != EVMAS::AS_STACK;
  }

  // MVT::i256 is the only legal type, so DAG combiner should never reduce
  // loads.
  bool shouldReduceLoadWidth(SDNode *Load, ISD::LoadExtType ExtTy,
                             EVT NewVT) const override {
    return false;
  }

private:
  const EVMSubtarget *Subtarget;

  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;

  SDValue lowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;

  SDValue lowerIntrinsicDataSize(unsigned IntrID, SDValue Op,
                                 SelectionDAG &DAG) const;

  SDValue LowerLOAD(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerSTORE(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerUMUL_LOHI(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerINTRINSIC_VOID(SDValue Op, SelectionDAG &DAG) const;

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
