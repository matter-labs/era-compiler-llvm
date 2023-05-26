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

private:
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &dl, SelectionDAG &DAG,
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
