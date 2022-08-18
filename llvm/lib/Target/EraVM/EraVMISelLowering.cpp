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

  // special handling of udiv/urem
  setOperationAction(ISD::UDIV, MVT::i256, Expand);
  setOperationAction(ISD::UREM, MVT::i256, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i256, Legal);

  // special handling of umulxx
  setOperationAction(ISD::MUL, MVT::i256, Expand);
  setOperationAction(ISD::MULHU, MVT::i256, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i256, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i256, Legal);

  setOperationAction(ISD::MULHS, MVT::i256, Expand);
  setOperationAction(ISD::MULHU, MVT::i256, Expand);

  // EraVM lacks of native support for signed operations.
  setOperationAction(ISD::SRA, MVT::i256, Custom);
  setOperationAction(ISD::SDIV, MVT::i256, Custom);
  setOperationAction(ISD::SREM, MVT::i256, Custom);

  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::i128}) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Expand);
    setOperationAction(ISD::LOAD, VT, Custom);
    setOperationAction(ISD::STORE, VT, Custom);
    setTruncStoreAction(MVT::i256, VT, Expand);
    setOperationAction(ISD::MERGE_VALUES, VT, Promote);
  }

  setOperationAction(ISD::STACKSAVE, MVT::Other, Custom);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Custom);

  // Intrinsics lowering
  setOperationAction(ISD::INTRINSIC_VOID, MVT::Other, Custom);

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::EXTLOAD, MVT::i256, VT, Custom);
  }

  setOperationAction(ISD::STORE, MVT::i256, Custom);
  setOperationAction(ISD::LOAD, MVT::i256, Custom);

  setOperationAction(ISD::ZERO_EXTEND, MVT::i256, Custom);
  setOperationAction(ISD::ANY_EXTEND, MVT::i256, Custom);

  setTargetDAGCombine(ISD::ZERO_EXTEND);

  setJumpIsExpensive(false);
  setMaximumJumpTableSize(0);
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

  unsigned InIdx = 0;
  for (CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      Register VReg = RegInfo.createVirtualRegister(&EraVM::GR256RegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());

      // insert AssertZext if we are loading smaller types
      const ISD::InputArg &arg = Ins[InIdx];

      if (arg.ArgVT.isScalarInteger()) {
        unsigned bitwidth = arg.ArgVT.getSizeInBits();
        if (bitwidth < 256) {
          auto int_type = EVT::getIntegerVT(*DAG.getContext(), bitwidth);
          ArgValue =
              DAG.getNode(ISD::AssertZext, DL, arg.VT, ArgValue.getValue(0),
                          DAG.getValueType(int_type));
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

  if (auto *GA = dyn_cast<GlobalAddressSDNode>(Callee.getNode())) {
    auto farcall_pair = [&]() {
      if (GA->getGlobal()->getName() == "__farcall_int") {
        return std::make_pair<uint64_t, bool>(EraVMISD::FARCALL, true);
      }
      if (GA->getGlobal()->getName() == "__staticcall_int") {
        return std::make_pair<uint64_t, bool>(EraVMISD::STATICCALL, true);
      }
      if (GA->getGlobal()->getName() == "__delegatecall_int") {
        return std::make_pair<uint64_t, bool>(EraVMISD::DELEGATECALL, true);
      }
      if (GA->getGlobal()->getName() == "__mimiccall_int") {
        return std::make_pair<uint64_t, bool>(EraVMISD::MIMICCALL, true);
      }
      return std::make_pair<uint64_t, bool>(0, false);
    }();

    auto farcall_opc = farcall_pair.first;
    bool is_farcall = farcall_pair.second;

    bool is_mimic = farcall_opc == EraVMISD::MIMICCALL;

    if (is_farcall) {
      if (is_mimic)
        Chain = DAG.getCopyToReg(Chain, DL, EraVM::R3, OutVals[2], SDValue());
      SmallVector<SDValue, 8> Ops;
      Ops.push_back(Chain);
      Ops.push_back(OutVals[0]);
      Ops.push_back(OutVals[1]);
      if (is_mimic) {
        Ops.push_back(OutVals[2]);
      }
      Ops.push_back(CLI.UnwindBB);
      SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
      Chain = DAG.getNode(farcall_opc, DL, NodeTys, Ops);
      InVals.push_back(DAG.getCopyFromReg(Chain, DL, EraVM::R1, MVT::i256,
                                          Chain.getValue(1)));
      return Chain;
    }
  }

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
  SDValue SPCtx = DAG.getTargetConstant(EraVMCTX::SP, DL, MVT::i256);

  SDValue InFlag;

  SDValue AbiData = CLI.EraVMAbiData ? DAG.getRegister(EraVM::R15, MVT::i256)
                                     : DAG.getRegister(EraVM::R0, MVT::i256);

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    auto VA = ArgLocs[i];

    if (!VA.isRegLoc()) {
      assert(VA.isMemLoc());
      SDNode *StackPtr =
          DAG.getMachineNode(EraVM::CTXr, DL, MVT::i256, SPCtx, Zero);

      SDValue offset =
          DAG.getConstant(VA.getLocMemOffset() / 32, DL, MVT::i256);

      SDValue StackLocation =
          DAG.getNode(ISD::ADD, DL, MVT::i256, SDValue(StackPtr, 0), offset);
      MemOpChains.push_back(DAG.getStore(Chain, DL, OutVals[i], StackLocation,
                                         MachinePointerInfo()));
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

  if (CLI.EraVMAbiData) {
    Chain = DAG.getCopyToReg(Chain, DL, EraVM::R15, CLI.EraVMAbiData, InFlag);
    InFlag = Chain.getValue(1);
  }

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(AbiData);
  Ops.push_back(Callee);
  if (CLI.CB && isa<InvokeInst>(CLI.CB))
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
      // if the return type is of integer type, and the size is smaller than
      // 256, insert assert zext:
      llvm::Type *retTy = CLI.RetTy;
      if (retTy->isIntegerTy()) {
        unsigned bitwidth = retTy->getIntegerBitWidth();
        if (bitwidth < 256) {
          auto int_type = EVT::getIntegerVT(*DAG.getContext(), bitwidth);
          Val = DAG.getNode(ISD::AssertZext, DL, MVT::i256, Val.getValue(0),
                            DAG.getValueType(int_type));
        }
      }
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
  case ISD::STORE:              return LowerSTORE(Op, DAG);
  case ISD::LOAD:               return LowerLOAD(Op, DAG);
  case ISD::ZERO_EXTEND:        return LowerZERO_EXTEND(Op, DAG);
  case ISD::ANY_EXTEND:         return LowerANY_EXTEND(Op, DAG);
  case ISD::GlobalAddress:      return LowerGlobalAddress(Op, DAG);
  case ISD::BR_CC:              return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:          return LowerSELECT_CC(Op, DAG);
  case ISD::SRA:                return LowerSRA(Op, DAG);
  case ISD::SDIV:               return LowerSDIV(Op, DAG);
  case ISD::SREM:               return LowerSREM(Op, DAG);
  case ISD::INTRINSIC_VOID:     return LowerINTRINSIC_VOID(Op, DAG);
  case ISD::STACKSAVE:          return LowerSTACKSAVE(Op, DAG);
  case ISD::STACKRESTORE:       return LowerSTACKRESTORE(Op, DAG);
  default:
    llvm_unreachable("unimplemented operation lowering");
  }
  // clang-format on
}

SDValue EraVMTargetLowering::LowerANY_EXTEND(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue extending_value = Op.getOperand(0);
  if (extending_value.getValueSizeInBits() == 256) {
    return extending_value;
  }
  return {};
}

SDValue EraVMTargetLowering::LowerZERO_EXTEND(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue extending_value = Op.getOperand(0);
  if (extending_value.getValueType() == Op.getValueType()) {
    return extending_value;
  } else {
    // eliminate zext
    SDValue new_value = DAG.getNode(extending_value->getOpcode(), DL,
                                    Op.getValueType(), extending_value->ops());
    return new_value;
  }
  return {};
}

SDValue EraVMTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  StoreSDNode *Store = cast<StoreSDNode>(Op);

  SDValue BasePtr = Store->getBasePtr();
  SDValue Chain = Store->getChain();
  const MachinePointerInfo &PInfo = Store->getPointerInfo();

  // for now only handle cases where alignment == 1
  // only handle unindexed store
  assert(Store->getAddressingMode() == ISD::UNINDEXED);

  EVT MemVT = Store->getMemoryVT();
  unsigned MemVTSize = MemVT.getSizeInBits();
  assert(MemVT.isScalarInteger() && "Unexpected type to store");
  if (MemVTSize == 256) {
    return {};
  }
  assert(MemVTSize < 256 && "Only handle smaller sized store");

  LLVM_DEBUG(errs() << "Special handling STORE node:\n"; Op.dump(&DAG));

  // We don't have store of smaller data types, so a snippet of code is needed
  // to represent small stores

  // small store is implemented as follows:
  // 1. R = load 256 bits starting from the pointer
  // 2. r <<< n bits; r >>> n bits; to clear top bits
  // 3. V <<< 256 - n bits;
  // 4. R = AND R, V
  // 5. STORE i256 R

  SDValue OriginalValue =
      DAG.getLoad(MVT::i256, DL, Chain, BasePtr, PInfo, Store->getAlign());
  SDValue SHL = DAG.getNode(ISD::SHL, DL, MVT::i256, OriginalValue,
                            DAG.getConstant(MemVTSize, DL, MVT::i256));
  SDValue SRL = DAG.getNode(ISD::SRL, DL, MVT::i256, SHL,
                            DAG.getConstant(MemVTSize, DL, MVT::i256));

  SDValue ZeroExtendedValue =
      DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i256, Store->getValue());

  SDValue StoreValue =
      DAG.getNode(ISD::SHL, DL, MVT::i256, ZeroExtendedValue,
                  DAG.getConstant(256 - MemVTSize, DL, MVT::i256));
  SDValue OR = DAG.getNode(ISD::OR, DL, MVT::i256, StoreValue, SRL);
  SDValue FinalStore = DAG.getStore(Chain, DL, OR, BasePtr, PInfo);

  return FinalStore;
}

SDValue EraVMTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *Load = cast<LoadSDNode>(Op);

  SDValue BasePtr = Load->getBasePtr();
  SDValue Chain = Load->getChain();
  const MachinePointerInfo &PInfo = Load->getPointerInfo();

  EVT MemVT = Load->getMemoryVT();
  EVT OpVT = Op->getValueType(0);
  unsigned MemVTSize = MemVT.getSizeInBits();
  if (MemVTSize == 256)
    return {};
  assert(MemVT.isScalarInteger() && "Unexpected type to load");
  assert(MemVTSize < 256 && "Only handle smaller sized load");

  LLVM_DEBUG(errs() << "Special handling LOAD node:\n"; Op.dump(&DAG));

  Op = DAG.getLoad(MVT::i256, DL, Chain, BasePtr, PInfo, Load->getAlign());

  // right-shifting:
  unsigned SRLValue = 256 - MemVTSize;
  SDValue SHR = DAG.getNode(ISD::SRL, DL, MVT::i256, Op,
                            DAG.getConstant(SRLValue, DL, MVT::i256));
  SDValue TRUNCATE = DAG.getNode(ISD::TRUNCATE, DL, OpVT, SHR);

  std::array Ops = {TRUNCATE, Op.getValue(1)};
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

  if (IntNo != Intrinsic::eravm_throw && IntNo != Intrinsic::eravm_return &&
      IntNo != Intrinsic::eravm_revert)
    return {};
  SDLoc DL(Op);
  auto CTR =
      DAG.getCopyToReg(Op.getOperand(0), DL, EraVM::R1, Op.getOperand(2));
  switch (IntNo) {
  default:
    llvm_unreachable("Unexpected intrinsic");
    return {};
  case Intrinsic::eravm_throw:
    return DAG.getNode(EraVMISD::THROW, DL, MVT::Other, CTR,
                       DAG.getRegister(EraVM::R1, MVT::i256));
  case Intrinsic::eravm_return:
    return DAG.getNode(EraVMISD::RETURN, DL, MVT::Other, CTR,
                       DAG.getRegister(EraVM::R1, MVT::i256));
  case Intrinsic::eravm_revert:
    return DAG.getNode(EraVMISD::REVERT, DL, MVT::Other, CTR,
                       DAG.getRegister(EraVM::R1, MVT::i256));
  }
}

SDValue EraVMTargetLowering::LowerSTACKSAVE(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDVTList RetTys = DAG.getVTList(MVT::i256, MVT::Other);
  return DAG.getNode(EraVMISD::GET_SP, SDLoc(Op), RetTys, Op.getOperand(0));
}

SDValue EraVMTargetLowering::LowerSTACKRESTORE(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDVTList GetSPTys = DAG.getVTList(MVT::i256, MVT::Other);
  SDValue CurrentSP =
      DAG.getNode(EraVMISD::GET_SP, DL, GetSPTys, Op.getOperand(0));
  SDValue SPDelta =
      DAG.getNode(ISD::SUB, DL, MVT::i256, Op.getOperand(1), CurrentSP);
  return DAG.getNode(EraVMISD::CHANGE_SP, DL, MVT::Other, CurrentSP.getValue(1),
                     SPDelta);
}

void EraVMTargetLowering::ReplaceNodeResults(SDNode *N,
                                             SmallVectorImpl<SDValue> &Results,
                                             SelectionDAG &DAG) const {
  LowerOperationWrapper(N, Results, DAG);
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

SDValue EraVMTargetLowering::PerformDAGCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  SelectionDAG &DAG = DCI.DAG;

  SDValue val;
  switch (N->getOpcode()) {
  default:
    break;
  case ISD::ZERO_EXTEND: {
    SDLoc DL(N);
    SDValue extending_value = N->getOperand(0);
    // combine with SETCC
    if (extending_value->getOpcode() != ISD::SETCC) {
      return val;
    }
    val = DAG.getNode(extending_value->getOpcode(), DL, N->getValueType(0),
                      extending_value->ops());
    break;
  }
  }
  return val;
}
