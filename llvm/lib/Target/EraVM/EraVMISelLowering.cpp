//===-- EraVMISelLowering.cpp - EraVM DAG Lowering Impl ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVMTargetLowering class.
//
//===----------------------------------------------------------------------===//

#include "EraVMISelLowering.h"
#include "EraVM.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMSubtarget.h"
#include "EraVMTargetMachine.h"
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
#include "llvm/IR/IntrinsicsEraVM.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "eravm-lower"

EraVMTargetLowering::EraVMTargetLowering(const TargetMachine &TM,
                                         const EraVMSubtarget &STI)
    : TargetLowering(TM) {
  // Set up the register classes.
  addRegisterClass(MVT::i256, &EraVM::GR256RegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Provide all sorts of operation actions
  setStackPointerRegisterToSaveRestore(EraVM::SP);

  setOperationAction(ISD::GlobalAddress, MVT::i256, Custom);
  setOperationAction(ISD::BR_CC, MVT::i256, Custom);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::SETCC, MVT::i256, Expand);
  setOperationAction(ISD::SELECT, MVT::i256, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i256, Custom);
  setOperationAction(ISD::TRUNCATE, MVT::i64, Promote);

  // special handling of udiv/urem
  setOperationAction(ISD::UDIV, MVT::i256, Expand);
  setOperationAction(ISD::UREM, MVT::i256, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i256, Legal);

  // special handling of umulxx
  setOperationAction(ISD::MUL, MVT::i256, Expand);
  setOperationAction(ISD::MULHU, MVT::i256, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i256, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i256, Legal);

  // EraVM lacks of native support for signed operations.
  setOperationAction(ISD::SRA, MVT::i256, Custom);
  setOperationAction(ISD::SDIV, MVT::i256, Custom);
  setOperationAction(ISD::SREM, MVT::i256, Custom);

  // Support of truncate, sext, zext
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i64, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i128, Expand);

  // Intrinsics lowering
  setOperationAction(ISD::INTRINSIC_VOID, MVT::Other, Custom);

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::i256, VT, Custom);
  }

  setJumpIsExpensive(false);
}

//===----------------------------------------------------------------------===//
//                      Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "EraVMGenCallingConv.inc"

static void fail(const SDLoc &DL, SelectionDAG &DAG, const char *msg) {
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(
      DiagnosticInfoUnsupported(MF.getFunction(), msg, DL.getDebugLoc()));
}

/// Test whether the given calling convention is supported.
static bool CallingConvSupported(CallingConv::ID CallConv) {
  // TODO: EraVM currently doesn't distinguish between different calling
  // convensions.
  return CallConv == CallingConv::C || CallConv == CallingConv::Fast ||
         CallConv == CallingConv::Cold;
}

bool EraVMTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {

  SmallVector<CCValAssign, 1> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  // cannot support return convention or is variadic?
  if (!CCInfo.CheckReturn(Outs, RetCC_ERAVM) || IsVarArg)
    return false;

  // EraVM can't currently handle returning tuples.
  return Outs.size() <= 1;
}

SDValue
EraVMTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  assert(Outs.size() <= 1 && "EraVM can only return up to one value");
  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "EraVM doesn't support non-C calling conventions");

  // Record the number and types of the return values.
  for (const ISD::OutputArg &Out : Outs) {
    assert(!Out.Flags.isByVal() && "byval is not valid for return values");
    assert(!Out.Flags.isNest() && "nest is not valid for return values");
    assert(Out.IsFixed && "non-fixed return value is not valid");
    if (Out.Flags.isInAlloca())
      fail(DL, DAG, "EraVM hasn't implemented inalloca results");
    if (Out.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "EraVM hasn't implemented cons regs results");
    if (Out.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "EraVM hasn't implemented cons regs last results");
  }

  // CCValAssign - represent the assignment of the return value to a location
  SmallVector<CCValAssign, 1> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, CC_ERAVM);
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

    RetOps[0] = Chain; // Update chain.

    // Add the flag if we have it.
    if (Flag.getNode())
      RetOps.push_back(Flag);
  }

  return DAG.getNode(EraVMISD::RET, DL, MVT::Other, RetOps);
}

SDValue EraVMTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "EraVM doesn't support non-C calling conventions");
  if (IsVarArg)
    fail(DL, DAG, "VarArg is not supported yet");

  for (const ISD::InputArg &In : Ins) {
    if (In.Flags.isInAlloca())
      fail(DL, DAG, "EraVM hasn't implemented inalloca arguments");
    if (In.Flags.isNest())
      fail(DL, DAG, "EraVM hasn't implemented nest arguments");
    if (In.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "EraVM hasn't implemented cons regs arguments");
    if (In.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "EraVM hasn't implemented cons regs last arguments");
    if (In.Flags.isByVal())
      fail(DL, DAG, "EraVM hasn't implemented by val arguments");
    // Ignore In.getOrigAlign() because all our arguments are passed in
    // registers.
  }

  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeArguments(Ins, CC_ERAVM);

  for (CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      Register VReg = RegInfo.createVirtualRegister(&EraVM::GR256RegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
      ArgValue = DAG.getNode(ISD::TRUNCATE, DL, VA.getValVT(), ArgValue);
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
  }

  return Chain;
}

SDValue EraVMTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
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

  // TODO: EraVM target does not yet support tail call optimization.
  IsTailCall = false;

  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "EraVM doesn't support non-C calling conventions");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_ERAVM);

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getStackSize();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;
  SDValue Zero = DAG.getTargetConstant(0, DL, MVT::i256);
  SDValue SPCtx = DAG.getTargetConstant(7, DL, MVT::i256);
  MachineSDNode *StackPtr =
      DAG.getMachineNode(EraVM::CTXr, DL, MVT::i256, SPCtx, Zero);

  SDValue InFlag;

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    unsigned revI = e - i - 1;
    auto VA = ArgLocs[revI];
    if (!VA.isRegLoc()) {
      SDValue PtrOff =
          DAG.getNode(ISD::ADD, DL, MVT::i256, SDValue(StackPtr, 0),
                      DAG.getConstant(VA.getLocMemOffset(), DL, MVT::i256));
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, OutVals[i], PtrOff, MachinePointerInfo()));
    }
  }

  // Transform all store nodes into one single node because all store nodes are
  // independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    if (ArgLocs[i].isRegLoc()) {
      Chain = DAG.getCopyToReg(Chain, DL, ArgLocs[i].getLocReg(), OutVals[i],
                               InFlag);
      InFlag = Chain.getValue(1);
    }
  }

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  if (isa<InvokeInst>(CLI.CB))
    Ops.push_back(CLI.UnwindBB);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (auto &ArgLoc : ArgLocs) {
    if (ArgLoc.isRegLoc())
      Ops.push_back(DAG.getRegister(ArgLoc.getLocReg(), ArgLoc.getValVT()));
  }

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  if (const auto *Invoke = dyn_cast_or_null<InvokeInst>(CLI.CB))
    Chain = DAG.getNode(EraVMISD::INVOKE, DL, NodeTys, Ops);
  else
    Chain = DAG.getNode(EraVMISD::CALL, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain =
      DAG.getCALLSEQ_END(Chain, DAG.getConstant(NumBytes, DL, MVT::i256, true),
                         DAG.getConstant(0, DL, MVT::i256, true), InFlag, DL);
  InFlag = Chain.getValue(1);

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                    *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, CC_ERAVM);

  InFlag = SDValue();
  DenseMap<unsigned, SDValue> CopiedRegs;
  // Copy all of the result registers out of their specified physreg.
  for (auto RVLoc : RVLocs) {
    // Avoid copying a physreg twice since RegAllocFast is incompetent and only
    // allows one use of a physreg per block.
    SDValue Val = CopiedRegs.lookup(RVLoc.getLocReg());
    if (!Val) {
      Val = DAG.getCopyFromReg(Chain, DL, RVLoc.getLocReg(),
                               RVLoc.getValVT(), InFlag);
      Chain = Val.getValue(1);
      InFlag = Val.getValue(2);
      CopiedRegs[RVLoc.getLocReg()] = Val;
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
         "EraVM doesn't support floats");

  auto Mask = DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);
  auto Zero = DAG.getConstant(0, DL, MVT::i256);
  auto SignLHS = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask);
  auto SignRHS = DAG.getNode(ISD::AND, DL, MVT::i256, RHS, Mask);
  auto SignXor = DAG.getNode(ISD::XOR, DL, MVT::i256, SignLHS, SignRHS);

  EraVMCC::CondCodes TCC = EraVMCC::COND_INVALID;
  switch (CC) {
  default:
    llvm_unreachable("Invalid integer condition!");
  case ISD::SETNE:
    TCC = EraVMCC::COND_NE;
    break;
  case ISD::SETEQ:
    TCC = EraVMCC::COND_E;
    break;
  case ISD::SETUGE:
    TCC = EraVMCC::COND_GE;
    break;
  case ISD::SETULT:
    TCC = EraVMCC::COND_LT;
    break;
  case ISD::SETULE:
    TCC = EraVMCC::COND_LE;
    break;
  case ISD::SETUGT:
    TCC = EraVMCC::COND_GT;
    break;
  case ISD::SETGT: {
    auto Ugt = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETUGT);
    auto SignUlt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETULT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUlt, Ugt, ISD::SETEQ);
    RHS = Zero;
    TCC = EraVMCC::COND_NE;
    break;
  }
  case ISD::SETGE: {
    auto Uge = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETUGE);
    auto SignUlt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETULT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUlt, Uge, ISD::SETEQ);
    RHS = Zero;
    TCC = EraVMCC::COND_NE;
    break;
  }
  case ISD::SETLT: {
    auto Ult = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETULT);
    auto SignUgt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETUGT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUgt, Ult, ISD::SETEQ);
    RHS = Zero;
    TCC = EraVMCC::COND_NE;
    break;
  }
  case ISD::SETLE: {
    auto Ule = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETULE);
    auto SignUgt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETUGT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUgt, Ule, ISD::SETEQ);
    RHS = Zero;
    TCC = EraVMCC::COND_NE;
    break;
  }
  }

  return DAG.getConstant(TCC, DL, MVT::i256);
}

SDValue EraVMTargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  // clang-format off
  switch (Op.getOpcode()) {
  case ISD::LOAD:               return LowerLOAD(Op, DAG);
  case ISD::GlobalAddress:      return LowerGlobalAddress(Op, DAG);
  case ISD::BR_CC:              return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:          return LowerSELECT_CC(Op, DAG);
  case ISD::SRA:                return LowerSRA(Op, DAG);
  case ISD::SDIV:               return LowerSDIV(Op, DAG);
  case ISD::SREM:               return LowerSREM(Op, DAG);
  case ISD::INTRINSIC_VOID:     return LowerINTRINSIC_VOID(Op, DAG);
  default:
    llvm_unreachable("unimplemented operation lowering");
  }
  // clang-format on
}

SDValue EraVMTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *Load = cast<LoadSDNode>(Op);
  if (Load->getExtensionType() != ISD::SEXTLOAD)
    return {};

  EVT MemVT = Load->getMemoryVT();
  assert(MemVT.isScalarInteger() && "Unexpected type to load");
  assert(Load->getMemOperand()->getAlign() >= MemVT.getStoreSize());
  if (MemVT.getSizeInBits() == 256)
    return {};

  SDValue BasePtr = Load->getBasePtr();
  SDValue Chain = Load->getChain();
  const MachinePointerInfo &PInfo = Load->getPointerInfo();
  Align A = Load->getAlign();

  SDValue MemVTNode = DAG.getValueType(MemVT);
  Op = DAG.getLoad(MVT::i256, DL, Chain, BasePtr, PInfo, A);
  SDValue Sext =
      DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i256, Op, MemVTNode);

  std::array Ops = {Sext, Op.getValue(1)};

  return DAG.getMergeValues(Ops, DL);
}

SDValue EraVMTargetLowering::LowerSRA(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  auto Val = Op.getOperand(0);
  auto Shift = Op.getOperand(1);
  auto Zero = DAG.getConstant(0, DL, MVT::i256);
  auto One = DAG.getConstant(APInt(256, -1, true), DL, MVT::i256);
  auto Mask = DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);
  auto Sign = DAG.getNode(ISD::AND, DL, MVT::i256, Val, Mask);
  auto Init = DAG.getSelectCC(DL, Sign, Mask, One, Zero, ISD::SETEQ);
  Mask = DAG.getNode(
      ISD::SHL, DL, MVT::i256, Init,
      DAG.getNode(ISD::SUB, DL, MVT::i256,
                  DAG.getConstant(APInt(256, 256, false), DL, MVT::i256),
                  Shift));
  auto Value = DAG.getNode(ISD::SRL, DL, MVT::i256, Val, Shift);
  auto Shifted = DAG.getNode(ISD::OR, DL, MVT::i256, Value, Mask);
  return DAG.getSelectCC(DL, Shift, Zero, Val, Shifted, ISD::SETEQ);
}

/// Lower sdiv to udiv and bitwise operations
/// sign.x, val.x = udiv! x, 0x80..00
/// val.x = sub.neq 0x80..00, val.x; abs of 2's complement
/// sign.y, val.y = udiv! y, 0x80..00
/// val.y = sub.neq 0x80..00, val.y; abs of 2's complement
/// sign = xor sign.x, sign.y
/// sign = shl sign, 255
/// val = udiv! val.x, val.y
/// val = sign ? sign - val : sign
/// val = mov.eq 0
SDValue EraVMTargetLowering::LowerSDIV(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue Zero = DAG.getConstant(APInt(256, 0, false), DL, MVT::i256);
  SDValue Const255 = DAG.getConstant(APInt(256, 255, false), DL, MVT::i256);
  SDValue MaskSign =
      DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);

  SDValue UDivDividend =
      DAG.getNode(ISD::UDIVREM, DL, {MVT::i256, MVT::i256}, {LHS, MaskSign});
  SDValue DividendVal = DAG.getSelectCC(
      DL, UDivDividend, Zero,
      DAG.getNode(ISD::SUB, DL, MVT::i256, MaskSign, UDivDividend.getValue(1)),
      UDivDividend.getValue(1), ISD::SETNE);
  SDValue UDivDivisor =
      DAG.getNode(ISD::UDIVREM, DL, {MVT::i256, MVT::i256}, {RHS, MaskSign});
  SDValue DivisorVal = DAG.getSelectCC(
      DL, UDivDivisor, Zero,
      DAG.getNode(ISD::SUB, DL, MVT::i256, MaskSign, UDivDivisor.getValue(1)),
      UDivDivisor.getValue(1), ISD::SETNE);
  SDValue Sign = DAG.getNode(
      ISD::SHL, DL, MVT::i256,
      DAG.getNode(ISD::XOR, DL, MVT::i256, UDivDividend, UDivDivisor),
      Const255);
  SDValue Result =
      DAG.getNode(ISD::UDIV, DL, MVT::i256, DividendVal, DivisorVal);
  return DAG.getSelectCC(
      DL, Result, Zero, Result,
      DAG.getSelectCC(
          DL, Sign, Zero, Result,
          DAG.getNode(ISD::OR, DL, MVT::i256,
                      DAG.getNode(ISD::SUB, DL, MVT::i256, Sign, Result), Sign),
          ISD::SETEQ),
      ISD::SETEQ);
}

// Lower SREM to unsigned operators.
// * remainder's sign is same as dividend's sign.
SDValue EraVMTargetLowering::LowerSREM(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue Zero = DAG.getConstant(APInt(256, 0, false), DL, MVT::i256);
  SDValue MaskSign =
      DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);

  SDValue UDivDividend =
      DAG.getNode(ISD::UDIVREM, DL, {MVT::i256, MVT::i256}, {LHS, MaskSign});
  SDValue DividendVal = DAG.getSelectCC(
      DL, UDivDividend, Zero,
      DAG.getNode(ISD::SUB, DL, MVT::i256, MaskSign, UDivDividend.getValue(1)),
      UDivDividend.getValue(1), ISD::SETNE);
  SDValue UDivDivisor =
      DAG.getNode(ISD::UDIVREM, DL, {MVT::i256, MVT::i256}, {RHS, MaskSign});
  SDValue DivisorVal = DAG.getSelectCC(
      DL, UDivDivisor, Zero,
      DAG.getNode(ISD::SUB, DL, MVT::i256, MaskSign, UDivDivisor.getValue(1)),
      UDivDivisor.getValue(1), ISD::SETNE);

  SDValue Sign = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, MaskSign);

  SDValue Result =
      DAG.getNode(ISD::UREM, DL, MVT::i256, DividendVal, DivisorVal);

  SDValue SubSRem = DAG.getNode(ISD::SUB, DL, MVT::i256, Zero, Result);
  SDValue SRem = DAG.getSelectCC(DL, Sign, Zero, Result, SubSRem, ISD::SETEQ);
  return DAG.getSelectCC(DL, Result, Zero, Result, SRem, ISD::SETEQ);
}

SDValue EraVMTargetLowering::LowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  int64_t Offset = cast<GlobalAddressSDNode>(Op)->getOffset();
  auto PtrVT = getPointerTy(DAG.getDataLayout());

  // Create the TargetGlobalAddress node, folding in the constant offset.
  SDValue Result = DAG.getTargetGlobalAddress(GV, SDLoc(Op), PtrVT, Offset);
  return Result;
}

SDValue EraVMTargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  SDValue TargetCC = EmitCMP(LHS, RHS, CC, DL, DAG);
  SDValue Cmp = DAG.getNode(EraVMISD::CMP, DL, MVT::Glue, LHS, RHS);
  return DAG.getNode(EraVMISD::BR_CC, DL, Op.getValueType(), Chain, Dest,
                     TargetCC, Cmp);
}

SDValue EraVMTargetLowering::LowerSELECT_CC(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc DL(Op);

  SDValue TargetCC = EmitCMP(LHS, RHS, CC, DL, DAG);
  SDValue Cmp = DAG.getNode(EraVMISD::CMP, DL, MVT::Glue, LHS, RHS);

  SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Glue);
  std::array Ops = {TrueV, FalseV, TargetCC, Cmp};

  return DAG.getNode(EraVMISD::SELECT_CC, DL, VTs, Ops);
}

SDValue EraVMTargetLowering::LowerINTRINSIC_VOID(SDValue Op,
                                                 SelectionDAG &DAG) const {
  unsigned IntNo =
      cast<ConstantSDNode>(
          Op.getOperand(Op.getOperand(0).getValueType() == MVT::Other))
          ->getZExtValue();
  if (IntNo != Intrinsic::eravm_throw)
    return {};
  SDLoc DL(Op);
  auto CTR =
      DAG.getCopyToReg(Op.getOperand(0), DL, EraVM::R1, Op.getOperand(2));
  return DAG.getNode(EraVMISD::THROW, DL, MVT::Other, CTR,
                     DAG.getRegister(EraVM::R1, MVT::i256));
}

const char *EraVMTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<EraVMISD::NodeType>(Opcode)) {
  case EraVMISD::FIRST_NUMBER:
    break;
#define HANDLE_NODETYPE(NODE)                                                  \
  case EraVMISD::NODE:                                                         \
    return "EraVMISD::" #NODE;
#include "EraVMISD.def"
#undef HANDLE_NODETYPE
  }
  return nullptr;
}
