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
#include "llvm/IR/IntrinsicsSyncVM.h"
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

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Provide all sorts of operation actions
  setStackPointerRegisterToSaveRestore(SyncVM::SP);

  setOperationAction(ISD::GlobalAddress, MVT::i256, Custom);
  setOperationAction(ISD::BR, MVT::Other, Custom);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::AND, MVT::i256, Custom);
  setOperationAction(ISD::XOR, MVT::i256, Custom);
  setOperationAction(ISD::SHL, MVT::i256, Custom);
  setOperationAction(ISD::SRL, MVT::i256, Custom);
  setOperationAction(ISD::SETCC, MVT::i256, Expand);
  setOperationAction(ISD::SELECT, MVT::i256, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i256, Custom);
  setOperationAction(ISD::TRUNCATE, MVT::i64, Promote);

  // Dynamic stack allocation: use the default expansion.

  // setOperationAction(ISD::CopyToReg, MVT::Other, Custom);

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
  // TODO: SyncVM currently doesn't distinguish between different calling
  // convensions.
  return CallConv == CallingConv::C || CallConv == CallingConv::Fast ||
         CallConv == CallingConv::Cold;
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
  CCInfo.AnalyzeReturn(Outs, CC_SYNCVM);
  SmallVector<SDValue, 4> RetOps(1, Chain);

  if (!RVLocs.empty()) {
    SDValue Flag;
    CCValAssign &VA = RVLocs[0];
    SDValue V = OutVals[0];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), V, Flag);
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
  }

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
  MachineFrameInfo &MFI = MF.getFrameInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeArguments(Ins, CC_SYNCVM);

  unsigned InIdx = 0;
  for (CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      Register VReg = RegInfo.createVirtualRegister(&SyncVM::GR256RegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
      ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
      if (Ins[InIdx].isOrigArg()) {
        unsigned Idx = Ins[InIdx].getOrigArgIndex();
        Type *T = MF.getFunction().getArg(Idx)->getType();
        if (T->isPointerTy() && T->getPointerAddressSpace() == 0) {
          auto AdjSPNode = DAG.getMachineNode(SyncVM::AdjSP, DL, MVT::i256,
                                              ArgValue);
          ArgValue = SDValue(AdjSPNode, 0);
        }
      }
      InVals.push_back(ArgValue);
    } else {
      // Sanity check
      assert(VA.isMemLoc());
      // Load the argument to a virtual register
      unsigned ObjSize = VA.getLocVT().getSizeInBits() / 8;
      // Create the frame index object for this incoming parameter...
      int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);

      // Create the SelectionDAG nodes corresponding to a load
      // from this parameter
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i256);
      SDValue InVal = DAG.getLoad(
          VA.getLocVT(), DL, Chain, FIN,
          MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI));
      InVals.push_back(InVal);
    }
    ++InIdx;
  }

  return Chain;
}

SDValue
SyncVMTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  SmallVector<SDValue, 12> MemOpChains;

  // TODO: SyncVM target does not yet support tail call optimization.
  IsTailCall = false;

  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "SyncVM doesn't support non-C calling conventions");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_SYNCVM);

  unsigned NumMemOps =
      std::count_if(std::begin(ArgLocs), std::end(ArgLocs),
                    [](const CCValAssign &VA) { return !VA.isRegLoc(); });
  SDValue InFlag;
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    if (ArgLocs[i].isRegLoc()) {
      Chain = DAG.getCopyToReg(Chain, DL, ArgLocs[i].getLocReg(), OutVals[i],
                               InFlag);
    } else {
      Chain = DAG.getNode(SyncVMISD::PUSH, DL, MVT::Other, Chain,
                          DAG.getTargetConstant(0, DL, MVT::i256), OutVals[i]);
    }
  }

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (auto &ArgLoc : ArgLocs) {
    if (ArgLoc.isRegLoc())
      Ops.push_back(DAG.getRegister(ArgLoc.getLocReg(), ArgLoc.getValVT()));
  }

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain = DAG.getNode(SyncVMISD::CALL, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  if (NumMemOps) {
    Chain = DAG.getNode(SyncVMISD::POP, DL, MVT::Other, Chain,
                        DAG.getTargetConstant(NumMemOps - 1, DL, MVT::i256),
                        InFlag);
  }

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                    *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, CC_SYNCVM);

  // TODO: Glue had been broken by this point, needs to be fixed.
  InFlag = SDValue();
  DenseMap<unsigned, SDValue> CopiedRegs;
  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    // Avoid copying a physreg twice since RegAllocFast is incompetent and only
    // allows one use of a physreg per block.
    SDValue Val = CopiedRegs.lookup(RVLocs[i].getLocReg());
    if (!Val) {
      Val = DAG.getCopyFromReg(Chain, DL, RVLocs[i].getLocReg(),
                               RVLocs[i].getValVT(), InFlag);
      Chain = Val.getValue(1);
      InFlag = Val.getValue(2);
      CopiedRegs[RVLocs[i].getLocReg()] = Val;
    }
    InVals.push_back(Val);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
//                      Other Lowerings
//===----------------------------------------------------------------------===//

static SDValue EmitCMP(SDValue &LHS, SDValue &RHS, ISD::CondCode CC,
                       const SDLoc &DL, SelectionDAG &DAG) {
  assert(!LHS.getValueType().isFloatingPoint() &&
         "SyncVM doesn't support floats");

  SyncVMCC::CondCodes TCC = SyncVMCC::COND_INVALID;
  switch (CC) {
  default:
    llvm_unreachable("Invalid integer condition!");
  case ISD::SETEQ:
    TCC = SyncVMCC::COND_E; // aka COND_Z
    // Minor optimization: if LHS is a constant, swap operands, then the
    // constant can be folded into comparison.
    if (LHS.getOpcode() == ISD::Constant)
      std::swap(LHS, RHS);
    break;
  case ISD::SETNE:
    TCC = SyncVMCC::COND_NE; // aka COND_NZ
    // Minor optimization: if LHS is a constant, swap operands, then the
    // constant can be folded into comparison.
    if (LHS.getOpcode() == ISD::Constant)
      std::swap(LHS, RHS);
    break;
  case ISD::SETULT:
    TCC = SyncVMCC::COND_LT;
    // Minor optimization: if LHS is a constant, swap operands, then the
    // constant can be folded into comparison.
    if (LHS.getOpcode() == ISD::Constant)
      std::swap(LHS, RHS);
    break;
  case ISD::SETULE:
    TCC = SyncVMCC::COND_LE;
    // Minor optimization: if LHS is a constant, swap operands, then the
    // constant can be folded into comparison.
    if (LHS.getOpcode() == ISD::Constant)
      std::swap(LHS, RHS);
    break;
  case ISD::SETUGT:
    TCC = SyncVMCC::COND_GT;
    // Minor optimization: if LHS is a constant, swap operands, then the
    // constant can be folded into comparison.
    if (LHS.getOpcode() == ISD::Constant)
      std::swap(LHS, RHS);
    break;
  case ISD::SETUGE:
    TCC = SyncVMCC::COND_GE;
    // Minor optimization: if LHS is a constant, swap operands, then the
    // constant can be folded into comparison.
    if (LHS.getOpcode() == ISD::Constant)
      std::swap(LHS, RHS);
    break;
  }

  return DAG.getConstant(TCC, DL, MVT::i256);
}

SDValue SyncVMTargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    llvm_unreachable("unimplemented operation lowering");
    return SDValue();
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::BR:
    return LowerBR(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::AND:
    return LowerAnd(Op, DAG);
  case ISD::XOR:
    return LowerXor(Op, DAG);
  case ISD::SHL:
    return LowerShl(Op, DAG);
  case ISD::SRL:
    return LowerSrl(Op, DAG);
  case ISD::CopyToReg:
    return LowerCopyToReg(Op, DAG);
  }
}

SDValue SyncVMTargetLowering::LowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  int64_t Offset = cast<GlobalAddressSDNode>(Op)->getOffset();
  auto PtrVT = getPointerTy(DAG.getDataLayout());

  // Create the TargetGlobalAddress node, folding in the constant offset.
  SDValue Result = DAG.getTargetGlobalAddress(GV, SDLoc(Op), PtrVT, Offset);
  return Result;
}

SDValue SyncVMTargetLowering::LowerBR(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue DestFalse = Op.getOperand(1);
  switch (Chain.getOpcode()) {
  default: {
    SDValue UnconditionalCC =
        DAG.getConstant(SyncVMCC::COND_NONE, DL, MVT::i256);
    return DAG.getNode(SyncVMISD::BR_CC, DL, Op.getValueType(), Chain,
                       DestFalse, DestFalse, UnconditionalCC);
  }
  case ISD::BR_CC:
    return LowerBrccBr(Chain, DestFalse, DL, DAG);
  case ISD::BRCOND:
    return LowerBrcondBr(Chain, DestFalse, DL, DAG);
  }
}

SDValue SyncVMTargetLowering::LowerSELECT_CC(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc DL(Op);

  SDValue TargetCC = EmitCMP(LHS, RHS, CC, DL, DAG);
  SDValue Cmp = DAG.getNode(SyncVMISD::SUB, DL, MVT::Glue, LHS, RHS);

  SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Glue);
  SDValue Ops[] = {TrueV, FalseV, TargetCC, Cmp};

  return DAG.getNode(SyncVMISD::SELECT_CC, DL, VTs, Ops);
}

SDValue SyncVMTargetLowering::LowerAnd(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getValueType();
  SDValue RHS = Op.getOperand(1);
  if (RHS.getOpcode() != ISD::Constant)
    fail(DL, DAG, "And with two registers should not reach zkEVM backend");
  APInt Val = cast<ConstantSDNode>(RHS)->getAPIntValue();
  APInt PosPow2 = Val + 1;
  if (PosPow2.isPowerOf2())
    return DAG.getNode(ISD::UREM, DL, Ty, Op.getOperand(0),
                       DAG.getConstant(PosPow2, DL, Ty));
  if (Val.countTrailingZeros() + Val.countLeadingOnes() == Val.getBitWidth())
    return DAG.getNode(
        ISD::MUL, DL, Ty,
        DAG.getNode(ISD::UDIV, DL, Ty, Op.getOperand(0),
                    DAG.getConstant(Val.countLeadingOnes(), DL, Ty)),
        DAG.getConstant(Val.countLeadingOnes(), DL, Ty));
  std::vector<unsigned> SeparatorBits;
  while (!Val.isNullValue()) {
    unsigned NumZ = Val.countLeadingZeros();
    unsigned Pos = 255 - NumZ;
    SeparatorBits.push_back(Pos);
    APInt Check(255, 0, false);
    while (Val.ugt(APInt::getOneBitSet(256, Pos))) {
      Val.clearBit(Pos--);
    }
    if (Pos == 0 && Val.isOneValue()) {
      Val.clearBit(0);
      break;
    }
    if (Val.eq(APInt::getOneBitSet(256, Pos))) {
      // if the mask is like 0bxx00100 rather than 0bxx11100
      if (SeparatorBits.size() % 2) {
        SeparatorBits.push_back(Pos - 1);
      } else {
        SeparatorBits.push_back(Pos);
        SeparatorBits.push_back(Pos - 1);
      }
      break;
    }
    SeparatorBits.push_back(Pos);
  }
  SDValue Result;
  for (unsigned i = 0, e = SeparatorBits.size(); i < e; ++i) {
    SDValue Extract;
    APInt Pow2 = APInt::getOneBitSet(256, SeparatorBits[i] + 1);
    Extract = DAG.getNode(ISD::UREM, DL, Ty, Op.getOperand(0),
                          DAG.getConstant(Pow2, DL, Ty));
    if (Result) {
      if (i % 2)
        Result = DAG.getNode(ISD::SUB, DL, Ty, Result, Extract);
      else
        Result = DAG.getNode(ISD::ADD, DL, Ty, Result, Extract);
    } else {
      Result = Extract;
    }
  }
  return Result;
}

SDValue SyncVMTargetLowering::LowerXor(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getValueType();
  SDValue RHS = Op.getOperand(1);
  if (RHS.getOpcode() != ISD::Constant)
    fail(DL, DAG, "Xor with two registers should not reach zkEVM backend");
  if (cast<ConstantSDNode>(RHS)->getSExtValue() != -1)
    fail(DL, DAG, "Only xor rN, -1 is supported by zkEVM backend");
  return DAG.getNode(ISD::SUB, DL, Ty,
                     DAG.getNode(ISD::SUB, DL, Ty,
                                 DAG.getRegister(SyncVM::R0, Ty),
                                 Op.getOperand(0)),
                     DAG.getConstant(1, DL, Ty));
}

SDValue SyncVMTargetLowering::LowerShl(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getValueType();
  SDValue RHS = Op.getOperand(1);
  if (RHS.getOpcode() != ISD::Constant)
    fail(DL, DAG, "shl with two registers should not reach zkEVM backend");
  APInt Shift = cast<ConstantSDNode>(RHS)->getAPIntValue();
  auto Val = APInt(256, 1, false).shl(Shift);
  return DAG.getNode(ISD::MUL, DL, Ty, Op.getOperand(0),
                     DAG.getConstant(Val, DL, Ty));
}

SDValue SyncVMTargetLowering::LowerSrl(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getValueType();
  SDValue RHS = Op.getOperand(1);
  if (RHS.getOpcode() != ISD::Constant)
    fail(DL, DAG, "srl with two registers should not reach zkEVM backend");
  APInt Shift = cast<ConstantSDNode>(RHS)->getAPIntValue();
  auto Val = APInt(256, 1, false).shl(Shift);
  return DAG.getNode(ISD::UDIV, DL, Ty, Op.getOperand(0),
                     DAG.getConstant(Val, DL, Ty));
}

SDValue SyncVMTargetLowering::LowerCopyToReg(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Src = Op.getOperand(2);
  if (Src.getOpcode() == ISD::FrameIndex ||
      (Src.getOpcode() == ISD::ADD &&
       Src.getOperand(0).getOpcode() == ISD::FrameIndex)) {
    unsigned Reg = cast<RegisterSDNode>(Op.getOperand(1))->getReg();
    // TODO: It's really a hack:
    // If we put an expression involving stack frame, we replase the address in
    // bytes with the address in cells. Probably we need to reconsider that
    // desing.
    SDValue Div = DAG.getNode(ISD::UDIV, DL, MVT::i256, Src,
                              DAG.getConstant(32, DL, MVT::i256));
    SDValue CTR = DAG.getCopyToReg(Op.getOperand(0), DL, Reg, Div,
                                   Op.getNumOperands() == 4 ? Op.getOperand(3)
                                                            : SDValue());
    return CTR;
  }

  return SDValue();
}

SDValue SyncVMTargetLowering::LowerBrccBr(SDValue Op, SDValue DestFalse,
                                          SDLoc DL, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue DestTrue = Op.getOperand(4);

  if (CC == ISD::SETNE) {
    CC = ISD::SETEQ;
    std::swap(DestFalse, DestTrue);
  }

  if (LHS.getOpcode() == ISD::AND && CC == ISD::SETEQ &&
      RHS.getOpcode() == ISD::Constant &&
      cast<ConstantSDNode>(RHS)->isNullValue() &&
      LHS.getOperand(1).getOpcode() == ISD::Constant &&
      cast<ConstantSDNode>(LHS.getOperand(1))->isOne()) {
    SDValue LHSInner = LHS.getOperand(0);
    if (LHSInner.getOpcode() == ISD::INTRINSIC_W_CHAIN) {
      SDValue BrFlag =
          LowerBrFlag(LHSInner, Chain, DestFalse, DestTrue, DL, DAG);
      if (BrFlag)
        return BrFlag;
    } else if (LHSInner.getOpcode() == ISD::TRUNCATE) {
      LHS = LHSInner.getOperand(0);
      RHS = DAG.getConstant(0, DL, MVT::i256);
    }
  }

  SDValue TargetCC = EmitCMP(LHS, RHS, CC, DL, DAG);
  SDValue Cmp = DAG.getNode(SyncVMISD::SUB, DL, MVT::Glue, LHS, RHS);
  return DAG.getNode(SyncVMISD::BR_CC, DL, Op.getValueType(), Chain, DestTrue,
                     DestFalse, TargetCC, Cmp);
}

SDValue SyncVMTargetLowering::LowerBrcondBr(SDValue Op, SDValue DestFalse,
                                            SDLoc DL, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDValue Cond = Op.getOperand(1);
  SDValue DestTrue = Op.getOperand(2);
  SDValue Zero = DAG.getConstant(0, DL, Cond.getValueType());
  // TODO: Code duplication.
  if (Cond.getOpcode() == ISD::INTRINSIC_W_CHAIN) {
    SDValue BrFlag = LowerBrFlag(Cond, Chain, DestFalse, DestTrue, DL, DAG);
    if (BrFlag)
      return BrFlag;
  }

  SDValue TargetCC = EmitCMP(Cond, Zero, ISD::SETNE, DL, DAG);
  SDValue Cmp = DAG.getNode(SyncVMISD::SUB, DL, MVT::Glue, Cond, Zero);
  return DAG.getNode(SyncVMISD::BR_CC, DL, Op.getValueType(), Chain, DestTrue,
                     DestFalse, TargetCC, Cmp);
}

SDValue SyncVMTargetLowering::LowerBrFlag(SDValue Cond, SDValue Chain,
                                          SDValue DestFalse, SDValue DestTrue,
                                          SDLoc DL, SelectionDAG &DAG) const {
  ISD::CondCode CC;
  ConstantSDNode *CN = cast<ConstantSDNode>(Cond.getOperand(1));
  Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
  switch (IntID) {
  case Intrinsic::syncvm_gtflag:
    CC = ISD::SETUGT;
    break;
  case Intrinsic::syncvm_ltflag:
    CC = ISD::SETULT;
    break;
  case Intrinsic::syncvm_eqflag:
    CC = ISD::SETEQ;
    break;
  }
  if (IntID == Intrinsic::syncvm_gtflag || IntID == Intrinsic::syncvm_ltflag ||
      IntID == Intrinsic::syncvm_eqflag) {
    SDValue RHS = DAG.getConstant(0, DL, Cond.getValueType()),
            LHS = DAG.getConstant(0, DL, Cond.getValueType());
    SDValue TargetCC = EmitCMP(LHS, RHS, CC, DL, DAG);
    if (Chain.getOpcode() != ISD::TokenFactor)
      Chain = Chain.getOperand(0);
    else {
      std::vector<SDValue> Vals;
      for (unsigned i = 0, e = Chain.getNumOperands(); i < e; ++i) {
        SDValue Val = Chain.getOperand(i);
        if (Val == Cond.getValue(1))
          Vals.push_back(Val.getOperand(0));
        else
          Vals.push_back(Val);
      }
      Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Vals);
    }
    return DAG.getNode(SyncVMISD::BR_CC, DL, MVT::Other, Chain, DestTrue,
                       DestFalse, TargetCC, Cond.getValue(1));
  }
  return {};
}

MachineBasicBlock *
SyncVMTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                  MachineBasicBlock *BB) const {
  unsigned Opc = MI.getOpcode();
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  assert(Opc == SyncVM::Select && "Unexpected instr type to insert");

  // To "insert" a SELECT instruction, we actually have to insert the diamond
  // control-flow pattern.  The incoming instruction knows the destination vreg
  // to set, the condition code register to branch on, the true/false values to
  // select between, and a branch opcode to use.
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator I = ++BB->getIterator();

  //  thisMBB:
  //  ...
  //   sub r0, r1, r2
  //   jCC copy1MBB, copy0MBB
  //  copy0MBB:
  //   j copy1MBB
  //  copy1MBB:
  //   val = PHI [TrueVal, thisMBB], [FalseVal, copy0MBB]

  MachineBasicBlock *thisMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *copy0MBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *copy1MBB = F->CreateMachineBasicBlock(LLVM_BB);
  F->insert(I, copy0MBB);
  F->insert(I, copy1MBB);
  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi node for the select.
  copy1MBB->splice(copy1MBB->begin(), BB,
                   std::next(MachineBasicBlock::iterator(MI)), BB->end());
  copy1MBB->transferSuccessorsAndUpdatePHIs(BB);
  // Next, add the true and fallthrough blocks as its successors.
  BB->addSuccessor(copy0MBB);
  BB->addSuccessor(copy1MBB);

  BuildMI(BB, DL, TII.get(SyncVM::JCC))
      .addMBB(copy1MBB)
      .addMBB(copy0MBB)
      .addImm(MI.getOperand(3).getCImm()->getZExtValue());

  //  copy0MBB:
  //   %FalseValue = ...
  //   # fallthrough to copy1MBB
  BB = copy0MBB;
  BuildMI(BB, DL, TII.get(SyncVM::JCC))
      .addMBB(copy1MBB)
      .addMBB(copy1MBB)
      .addImm(SyncVMCC::COND_NONE);

  // Update machine-CFG edges
  BB->addSuccessor(copy1MBB);

  //  copy1MBB:
  //   %Result = phi [ %FalseValue, copy0MBB ], [ %TrueValue, thisMBB ]
  //  ...
  BB = copy1MBB;
  BuildMI(*BB, BB->begin(), DL, TII.get(SyncVM::PHI), MI.getOperand(0).getReg())
      .addReg(MI.getOperand(2).getReg())
      .addMBB(copy0MBB)
      .addReg(MI.getOperand(1).getReg())
      .addMBB(thisMBB);

  MI.eraseFromParent(); // The pseudo instruction is gone now.
  return BB;
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
