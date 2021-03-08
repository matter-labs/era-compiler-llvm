//===-- SyncVMISelLowering.cpp - SyncVM DAG Lowering Implementation  ------===//
//
// This file implements the SyncVMTargetLowering class.
//
//===----------------------------------------------------------------------===//

#include "SyncVMISelLowering.h"
#include "SyncVM.h"
#include "SyncVMMachineFunctionInfo.h"
#include "SyncVMSubtarget.h"
#include "SyncVMTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "syncvm-lower"

SyncVMTargetLowering::SyncVMTargetLowering(const TargetMachine &TM,
                                           const SyncVMSubtarget &STI)
    : TargetLowering(TM) {
  // Set up the register classes.
  addRegisterClass(MVT::i256, &SyncVM::GR256RegClass);
  addRegisterClass(MVT::i16, &SyncVM::GR16RegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Support of truncate, sext, zext
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i64, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i128, Expand);
}

//===----------------------------------------------------------------------===//
//                      Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "SyncVMGenCallingConv.inc"

static void fail(const SDLoc &DL, SelectionDAG &DAG, const char *msg) {
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(
      DiagnosticInfoUnsupported(MF.getFunction(), msg, DL.getDebugLoc()));
}

/// Test whether the given calling convention is supported.
static bool CallingConvSupported(CallingConv::ID CallConv) {
  return CallConv == CallingConv::C;
}

bool SyncVMTargetLowering::CanLowerReturn(
    CallingConv::ID /*CallConv*/, MachineFunction & /*MF*/, bool /*IsVarArg*/,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    LLVMContext & /*Context*/) const {
  // SyncVM can't currently handle returning tuples.
  return Outs.size() <= 1;
}

SDValue
SyncVMTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                  bool IsVarArg,
                                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                                  const SmallVectorImpl<SDValue> &OutVals,
                                  const SDLoc &DL, SelectionDAG &DAG) const {
  assert(Outs.size() <= 1 && "SyncVM can only return up to one value");
  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "SyncVM doesn't support non-C calling conventions");

  // Record the number and types of the return values.
  for (const ISD::OutputArg &Out : Outs) {
    assert(!Out.Flags.isByVal() && "byval is not valid for return values");
    assert(!Out.Flags.isNest() && "nest is not valid for return values");
    assert(Out.IsFixed && "non-fixed return value is not valid");
    if (Out.Flags.isInAlloca())
      fail(DL, DAG, "SyncVM hasn't implemented inalloca results");
    if (Out.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "SyncVM hasn't implemented cons regs results");
    if (Out.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "SyncVM hasn't implemented cons regs last results");
  }

  // CCValAssign - represent the assignment of the return value to a location
  SmallVector<CCValAssign, 1> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_SYNCVM);

  SmallVector<SDValue, 4> RetOps(1, Chain);
  SDValue Flag;
  CCValAssign &VA = RVLocs[0];
  Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[0], Flag);
  // Guarantee that all emitted copies are stuck together,
  // avoiding something bad.
  Flag = Chain.getValue(1);
  RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));

  // OutVals.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  // TODO: Return instruction
  RetOps[0] = Chain; // Update chain.

  // Add the flag if we have it.
  if (Flag.getNode())
    RetOps.push_back(Flag);
  return DAG.getNode(SyncVMISD::RET, DL, MVT::Other, RetOps);
}

SDValue SyncVMTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "SyncVM doesn't support non-C calling conventions");
  if (IsVarArg)
    fail(DL, DAG, "VarArg is not supported yet");

  for (const ISD::InputArg &In : Ins) {
    if (In.Flags.isInAlloca())
      fail(DL, DAG, "SyncVM hasn't implemented inalloca arguments");
    if (In.Flags.isNest())
      fail(DL, DAG, "SyncVM hasn't implemented nest arguments");
    if (In.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "SyncVM hasn't implemented cons regs arguments");
    if (In.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "SyncVM hasn't implemented cons regs last arguments");
    if (In.Flags.isByVal())
      fail(DL, DAG, "SyncVM hasn't implemented by val arguments");
    // Ignore In.getOrigAlign() because all our arguments are passed in
    // registers.
  }

  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  static const MCPhysReg CRegList[] = {SyncVM::R1, SyncVM::R2, SyncVM::R3,
                                       SyncVM::R4, SyncVM::R5, SyncVM::R6};

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());

  unsigned ValNo = 0;
  assert(Ins.size());
  for (const auto &In : Ins) {
    unsigned Reg = CCInfo.AllocateReg(CRegList);
    CCInfo.addLoc(
        CCValAssign::getReg(ValNo++, In.VT, Reg, In.VT, CCValAssign::Full));
  }

  for (CCValAssign &VA : ArgLocs) {
    Register VReg = RegInfo.createVirtualRegister(&SyncVM::GR256RegClass);
    RegInfo.addLiveIn(VA.getLocReg(), VReg);
    SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
    ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
    InVals.push_back(ArgValue);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
//                      Other Lowerings
//===----------------------------------------------------------------------===//

SDValue SyncVMTargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    llvm_unreachable("unimplemented operand");
  }
}

const char *SyncVMTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<SyncVMISD::NodeType>(Opcode)) {
  case SyncVMISD::FIRST_NUMBER:
    break;
#define HANDLE_NODETYPE(NODE)                                                  \
  case SyncVMISD::NODE:                                                        \
    return "SyncVMISD::" #NODE;
#include "SyncVMISD.def"
#undef HANDLE_NODETYPE
  }
  return nullptr;
}
