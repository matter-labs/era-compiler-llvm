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
#include <unordered_map>

using namespace llvm;

#define DEBUG_TYPE "eravm-lower"

/// Helper function: wrap symbols according to their address space.
/// Precondition:
/// \p ValueToWrap is SDValue containing a symbol value (global address,
/// external symbol or block address)
SDValue EraVMTargetLowering::wrapSymbol(const SDValue &ValueToWrap,
                                        SelectionDAG &DAG, const SDLoc &DL,
                                        unsigned addrspace) const {
  auto VT = getPointerTy(DAG.getDataLayout());
  switch (addrspace) {
  case EraVMAS::AS_STACK:
    return DAG.getNode(EraVMISD::GAStack, DL, VT, ValueToWrap);
  case EraVMAS::AS_CODE:
    return DAG.getNode(EraVMISD::GACode, DL, VT, ValueToWrap);
  default:
    break;
  }
  llvm_unreachable("Global symbol in unexpected addr space");
}

/// Wrap a global address and lower to TargetGlobalAddress.
/// The \p ValueToWrap must be a GlobalAddressSDNode.
SDValue EraVMTargetLowering::wrapGlobalAddress(const SDValue &ValueToWrap,
                                               SelectionDAG &DAG,
                                               const SDLoc &DL) const {
  // convert to TargetGlobalAddress
  auto *GANode = cast<GlobalAddressSDNode>(ValueToWrap.getNode());
  auto TGA = DAG.getTargetGlobalAddress(
      GANode->getGlobal(), DL, ValueToWrap.getValueType(), GANode->getOffset());
  return wrapSymbol(TGA, DAG, DL, GANode->getAddressSpace());
}

/// Wrap a external symbol and lower to TargetExternalSymbol.
/// The \p ValueToWrap must be a ExternalSymbolSDNode.
SDValue EraVMTargetLowering::wrapExternalSymbol(const SDValue &ValueToWrap,
                                                SelectionDAG &DAG,
                                                const SDLoc &DL) const {
  // convert to TargetExternalSymbol
  auto *ESNode = cast<ExternalSymbolSDNode>(ValueToWrap.getNode());
  auto TES = DAG.getTargetExternalSymbol(ESNode->getSymbol(),
                                         ValueToWrap.getValueType());
  return wrapSymbol(TES, DAG, DL, EraVMAS::AS_CODE);
}

EraVMTargetLowering::EraVMTargetLowering(const TargetMachine &TM,
                                         const EraVMSubtarget &STI)
    : TargetLowering(TM) {
  // Set up the register classes.
  addRegisterClass(MVT::i256, &EraVM::GR256RegClass);
  addRegisterClass(MVT::fatptr, &EraVM::GRPTRRegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Provide all sorts of operation actions
  setStackPointerRegisterToSaveRestore(EraVM::SP);

  // By default, expand all i256bit operations
  for (unsigned Opc = 0; Opc < ISD::BUILTIN_OP_END; ++Opc)
    setOperationAction(Opc, MVT::i256, Expand);

  setOperationAction(
      {
          ISD::BR_JT,
          ISD::BRIND,
          ISD::BRCOND,
          ISD::VASTART,
          ISD::VAARG,
          ISD::VAEND,
          ISD::VACOPY,
      },
      MVT::Other, Expand);

  // Legal operations
  setOperationAction({ISD::ADD, ISD::SUB, ISD::AND, ISD::OR, ISD::XOR, ISD::SHL,
                      ISD::SRL, ISD::UDIVREM, ISD::UMUL_LOHI, ISD::Constant,
                      ISD::UNDEF, ISD::FRAMEADDR},
                     MVT::i256, Legal);

  // custom lowering operations
  setOperationAction(
      {
          ISD::SRA,
          ISD::SDIV,
          ISD::SREM,
          ISD::SDIVREM,
          ISD::STORE,
          ISD::LOAD,
          ISD::ZERO_EXTEND,
          ISD::ANY_EXTEND,
          ISD::GlobalAddress,
          ISD::BR_CC,
          ISD::SELECT_CC,
          ISD::BSWAP,
          ISD::CTPOP,
      },
      MVT::i256, Custom);

  setOperationAction({ISD::INTRINSIC_VOID, ISD::INTRINSIC_WO_CHAIN,
                      ISD::INTRINSIC_W_CHAIN, ISD::STACKSAVE, ISD::STACKRESTORE,
                      ISD::TRAP},
                     MVT::Other, Custom);

  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::i128}) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Expand);
    setOperationAction(ISD::LOAD, VT, Custom);
    setOperationAction(ISD::STORE, VT, Custom);
    setTruncStoreAction(MVT::i256, VT, Expand);
    setOperationAction(ISD::MERGE_VALUES, VT, Promote);
  }

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::EXTLOAD, MVT::i256, VT, Custom);
  }

  // fatptr operations
  setOperationAction(ISD::LOAD, MVT::fatptr, Legal);
  setOperationAction(ISD::STORE, MVT::fatptr, Legal);
  setOperationAction(ISD::SELECT_CC, MVT::fatptr, Custom);

  // special DAG combining handling for EraVM
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
  return CallConv == CallingConv::C || CallConv == CallingConv::Fast ||
         CallConv == CallingConv::Cold;
}

bool EraVMTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {

  if (Outs.size() >= EraVM::GR256RegClass.getNumRegs() - 1)
    return false;

  SmallVector<CCValAssign, 1> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  // Cannot support return convention or is variadic?
  return CCInfo.CheckReturn(Outs, RetCC_ERAVM);
}

SDValue
EraVMTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
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
  SDValue Flag;

  for (unsigned i = 0, e = RVLocs.size(); i < e; ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    const SDValue &CurVal = OutVals[i];
    auto Val = isa<GlobalAddressSDNode>(CurVal.getNode())
                   ? wrapGlobalAddress(CurVal, DAG, DL)
                   : CurVal;
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Flag);

    // Guarantee that all emitted copies are stuck together,
    // avoiding something bad.
    Flag = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain; // Update chain.

  // Add the flag if we have it.
  if (Flag.getNode())
    RetOps.push_back(Flag);

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
      const auto *RC = VA.getValVT() == MVT::fatptr ? &EraVM::GRPTRRegClass
                                                    : &EraVM::GR256RegClass;
      Register VReg = RegInfo.createVirtualRegister(RC);
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

/// If Callee is a farcall "intrinsic" return corresponding opcode.
/// Return 0 otherwise.
static uint64_t farcallOpcode(SDValue Callee) {
  auto *GA = dyn_cast<GlobalAddressSDNode>(Callee.getNode());
  if (!GA)
    return 0;
  return StringSwitch<uint64_t>(GA->getGlobal()->getName())
      .Case("__farcall_int", EraVMISD::FARCALL)
      .Case("__staticcall_int", EraVMISD::STATICCALL)
      .Case("__delegatecall_int", EraVMISD::DELEGATECALL)
      .Case("__mimiccall_int", EraVMISD::MIMICCALL)
      .Default(0);
}

static SDValue lowerFarCall(TargetLowering::CallLoweringInfo &CLI,
                            SmallVectorImpl<SDValue> &InVals) {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  // TODO: CPR-410 EraVM target does not yet support tail call optimization.
  CLI.IsTailCall = false;
  CallingConv::ID CallConv = CLI.CallConv;
  // We meddle with number of parameters, set vararg to true to prevent
  // assertion that the number of parameters before lowering is equal to the
  // number of parameters after lowering.
  bool IsVarArg = CLI.IsVarArg = true;

  uint64_t FarcallOpcode = farcallOpcode(Callee);
  if (!FarcallOpcode)
    return {};
  bool IsMimicCall = FarcallOpcode == EraVMISD::MIMICCALL;

  SmallVector<CCValAssign, 13> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_ERAVM);
  SDValue InFlag;

  Chain = DAG.getCALLSEQ_START(Chain, 0, 0, DL);

  // The last argument of a mimic call should be in R15. Handle it separately.
  unsigned LastNormalArgument = IsMimicCall ? Outs.size() - 1 : Outs.size();
  for (unsigned i = 0, e = LastNormalArgument; i != e; ++i) {
    if (!ArgLocs[i].isRegLoc())
      fail(DL, DAG,
           "Only operands passed through register expected in a far call");
    Chain =
        DAG.getCopyToReg(Chain, DL, ArgLocs[i].getLocReg(), OutVals[i], InFlag);
    InFlag = Chain.getValue(1);
  }

  // Pass whom to mimic in R15.
  if (IsMimicCall) {
    Chain = DAG.getCopyToReg(Chain, DL, EraVM::R15, OutVals.back(), InFlag);
    InFlag = Chain.getValue(1);
  }

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 14> Ops;

  Ops.push_back(Chain);
  Ops.push_back(CLI.UnwindBB);
  for (auto &ArgLoc :
       make_range(ArgLocs.begin(),
                  IsMimicCall ? std::prev(ArgLocs.end()) : ArgLocs.end())) {
    Ops.push_back(DAG.getRegister(ArgLoc.getLocReg(), ArgLoc.getValVT()));
  }
  if (IsMimicCall)
    Ops.push_back(DAG.getRegister(EraVM::R15, MVT::i256));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain = DAG.getNode(FarcallOpcode, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain =
      DAG.getCALLSEQ_END(Chain, DAG.getConstant(0, DL, MVT::i256, true),
                         DAG.getConstant(0, DL, MVT::i256, true), InFlag, DL);
  InFlag = Chain.getValue(1);

  InVals.push_back(DAG.getCopyFromReg(Chain, DL, EraVM::R1, MVT::fatptr, {}));
  InVals.push_back(DAG.getCopyFromReg(InVals[0].getValue(1), DL, EraVM::R1,
                                      MVT::fatptr, InVals[0].getValue(2)));
  return InVals[1].getValue(1);
}

SDValue EraVMTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SDValue FarCall = lowerFarCall(CLI, InVals);
  if (FarCall)
    return FarCall;

  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  // TODO: CPR-410 EraVM target does not yet support tail call optimization.
  CLI.IsTailCall = false;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_ERAVM);

  SDValue InFlag;

  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "EraVM doesn't support non-C calling conventions");

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getNextStackOffset();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;
  SDValue Zero = DAG.getTargetConstant(0, DL, MVT::i256);
  SDValue SPCtx = DAG.getTargetConstant(EraVMCTX::SP, DL, MVT::i256);

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

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = wrapGlobalAddress(Callee, DAG, DL);
  } else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = wrapExternalSymbol(Callee, DAG, DL);
  }

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    if (ArgLocs[i].isRegLoc()) {
      SDValue &CurVal = OutVals[i];
      auto Val = isa<GlobalAddressSDNode>(CurVal.getNode())
                     ? wrapGlobalAddress(CurVal, DAG, DL)
                     : CurVal;

      Chain = DAG.getCopyToReg(Chain, DL, ArgLocs[i].getLocReg(), Val, InFlag);
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

  if (const auto *Invoke = dyn_cast_or_null<InvokeInst>(CLI.CB)) {
    Chain = DAG.getNode(EraVMISD::INVOKE, DL, NodeTys, Ops);
  } else {
    Chain = DAG.getNode(EraVMISD::CALL, DL, NodeTys, Ops);
  }
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
      Val = DAG.getCopyFromReg(Chain, DL, RVLoc.getLocReg(), RVLoc.getValVT(),
                               InFlag);
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
  case ISD::BlockAddress:       return LowerBlockAddress(Op, DAG);
  case ISD::ExternalSymbol:     return LowerExternalSymbol(Op, DAG);
  case ISD::BR_CC:              return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:          return LowerSELECT_CC(Op, DAG);
  case ISD::SRA:                return LowerSRA(Op, DAG);
  case ISD::SDIV:               return LowerSDIV(Op, DAG);
  case ISD::SREM:               return LowerSREM(Op, DAG);
  case ISD::SDIVREM:            return LowerSDIVREM(Op, DAG);
  case ISD::INTRINSIC_VOID:     return LowerINTRINSIC_VOID(Op, DAG);
  case ISD::INTRINSIC_W_CHAIN:  return LowerINTRINSIC_W_CHAIN(Op, DAG);
  case ISD::INTRINSIC_WO_CHAIN: return LowerINTRINSIC_WO_CHAIN(Op, DAG);
  case ISD::STACKSAVE:          return LowerSTACKSAVE(Op, DAG);
  case ISD::STACKRESTORE:       return LowerSTACKRESTORE(Op, DAG);
  case ISD::BSWAP:              return LowerBSWAP(Op, DAG);
  case ISD::CTPOP:              return LowerCTPOP(Op, DAG);
  case ISD::TRAP:               return LowerTRAP(Op, DAG);
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
  SDValue ExtendingValue = Op.getOperand(0);
  if (ExtendingValue.getValueType() == Op.getValueType())
    return ExtendingValue;

  // eliminate zext
  SDValue NewValue = DAG.getNode(ExtendingValue->getOpcode(), DL,
                                 Op.getValueType(), ExtendingValue->ops());
  return NewValue;
}

SDValue EraVMTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *Store = cast<StoreSDNode>(Op);

  SDValue BasePtr = Store->getBasePtr();
  SDValue Chain = Store->getChain();
  const MachinePointerInfo &PInfo = Store->getPointerInfo();

  // for now only handle cases where alignment == 1
  // indexed loads and stores are illegal before ISEL,
  // the EraVMCombineToIndexedMemops pass is used to do the trick.
  assert(Store->getAddressingMode() == ISD::UNINDEXED);

  EVT MemVT = Store->getMemoryVT();
  unsigned MemVTSize = MemVT.getSizeInBits();
  assert((MemVT.isScalarInteger() || MemVT.getSimpleVT() == MVT::fatptr) &&
         "Unexpected type to store");
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

struct SignedDivisionLowerResult {
  SDValue UDivDividend;
  SDValue UDivDivisor;
  SDValue Result;
};

// extract the signs from LHS and RHS, and apply an
// operation (passed in Opcode) on LHS, RHS. Return all the three values for
// subsequent computation. It exists because it is a common part for sdiv, srem,
// sdivrem.
static SignedDivisionLowerResult
lowerCommonSDIVREMOperations(SDValue Op, SelectionDAG &DAG, int Opcode) {
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

  SmallVector<EVT, 2> ResultVTs{MVT::i256};
  if (Opcode == ISD::UDIVREM)
    ResultVTs.push_back(MVT::i256);

  const SDValue Result = DAG.getNode(Opcode, DL, DAG.getVTList(ResultVTs),
                                     DividendVal, DivisorVal);
  return {UDivDividend, UDivDivisor, Result};
}

/// \brief Lower sdiv to udiv and bitwise operations
/// A signed division is expanded as follows:
///
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

  SDValue Zero = DAG.getConstant(APInt(256, 0, false), DL, MVT::i256);
  SDValue Const255 = DAG.getConstant(APInt(256, 255, false), DL, MVT::i256);
  auto [UDivDividend, UDivDivisor, Result] =
      lowerCommonSDIVREMOperations(Op, DAG, ISD::UDIV);

  SDValue Sign = DAG.getNode(
      ISD::SHL, DL, MVT::i256,
      DAG.getNode(ISD::XOR, DL, MVT::i256, UDivDividend, UDivDivisor),
      Const255);
  return DAG.getSelectCC(
      DL, Result, Zero, Result,
      DAG.getSelectCC(
          DL, Sign, Zero, Result,
          DAG.getNode(ISD::OR, DL, MVT::i256,
                      DAG.getNode(ISD::SUB, DL, MVT::i256, Sign, Result), Sign),
          ISD::SETEQ),
      ISD::SETEQ);
}

SDValue EraVMTargetLowering::LowerSDIVREM(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue Zero = DAG.getConstant(APInt(256, 0, false), DL, MVT::i256);
  SDValue Const255 = DAG.getConstant(APInt(256, 255, false), DL, MVT::i256);
  SDValue MaskSign =
      DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);

  auto [UDivDividend, UDivDivisor, QuotientAndRemainder] =
      lowerCommonSDIVREMOperations(Op, DAG, ISD::UDIVREM);

  // Quotient
  SDValue QuotientResult = QuotientAndRemainder.getValue(0);
  SDValue QuotientSign = DAG.getNode(
      ISD::SHL, DL, MVT::i256,
      DAG.getNode(ISD::XOR, DL, MVT::i256, UDivDividend, UDivDivisor),
      Const255);
  SDValue Quotient = DAG.getSelectCC(
      DL, QuotientResult, Zero, QuotientResult,
      DAG.getSelectCC(DL, QuotientSign, Zero, QuotientResult,
                      DAG.getNode(ISD::OR, DL, MVT::i256,
                                  DAG.getNode(ISD::SUB, DL, MVT::i256,
                                              QuotientSign, QuotientResult),
                                  QuotientSign),
                      ISD::SETEQ),
      ISD::SETEQ);

  // Remainder
  SDValue RemainderSign = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, MaskSign);
  SDValue RemainderResult = QuotientAndRemainder.getValue(1);

  SDValue SubSRem = DAG.getNode(ISD::SUB, DL, MVT::i256, Zero, RemainderResult);
  SDValue SRem = DAG.getSelectCC(DL, RemainderSign, Zero, RemainderResult,
                                 SubSRem, ISD::SETEQ);
  SDValue Remainder = DAG.getSelectCC(DL, RemainderResult, Zero,
                                      RemainderResult, SRem, ISD::SETEQ);

  return DAG.getNode(ISD::MERGE_VALUES, DL, {MVT::i256, MVT::i256},
                     {Quotient, Remainder});
}

// Lower SREM to unsigned operators.
// * remainder's sign is same as dividend's sign.
SDValue EraVMTargetLowering::LowerSREM(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue Zero = DAG.getConstant(APInt(256, 0, false), DL, MVT::i256);
  SDValue MaskSign =
      DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);

  auto LoweredResults = lowerCommonSDIVREMOperations(Op, DAG, ISD::UREM);

  SDValue Result = LoweredResults.Result;

  SDValue Sign = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, MaskSign);

  SDValue SubSRem = DAG.getNode(ISD::SUB, DL, MVT::i256, Zero, Result);
  SDValue SRem = DAG.getSelectCC(DL, Sign, Zero, Result, SubSRem, ISD::SETEQ);
  return DAG.getSelectCC(DL, Result, Zero, Result, SRem, ISD::SETEQ);
}

SDValue EraVMTargetLowering::LowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  return wrapGlobalAddress(Op, DAG, SDLoc(Op));
}

SDValue EraVMTargetLowering::LowerExternalSymbol(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc dl(Op);
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetExternalSymbol(Sym, PtrVT);

  // EraVM doesn't support external symbols, but it make sense to enable
  // generic codegen tests to pass. In case an external symbol persist linker
  // will emit a diagnostic.
  return DAG.getNode(EraVMISD::GACode, dl, PtrVT, Result);
}

SDValue EraVMTargetLowering::LowerBlockAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDLoc dl(Op);
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetBlockAddress(BA, PtrVT);

  return DAG.getNode(EraVMISD::GACode, dl, PtrVT, Result);
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

SDValue EraVMTargetLowering::LowerINTRINSIC_W_CHAIN(SDValue Op,
                                                    SelectionDAG &DAG) const {
  unsigned IntrinsicID = cast<ConstantSDNode>(Op.getOperand(1))->getZExtValue();
  SDLoc DL(Op);

  if (IntrinsicID != Intrinsic::eravm_decommit)
    return {};
  return DAG.getNode(EraVMISD::LOG_DECOMMIT, DL,
                     {Op.getValueType(), MVT::Other},
                     {Op->getOperand(0), Op.getOperand(2), Op.getOperand(3)});
}

SDValue EraVMTargetLowering::LowerINTRINSIC_WO_CHAIN(SDValue Op,
                                                     SelectionDAG &DAG) const {
  unsigned IntrinsicID = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  EVT VT = Op.getValueType();
  SDLoc DL(Op);

  switch (IntrinsicID) {
  default:
    break;
  case Intrinsic::eravm_ptrtoint: {
    return DAG.getNode(EraVMISD::PTR_TO_INT, DL, VT, Op.getOperand(1));
  }
  case Intrinsic::eravm_ptr_add: {
    return DAG.getNode(EraVMISD::PTR_ADD, DL, VT, Op.getOperand(1),
                       Op.getOperand(2));
  }
  case Intrinsic::eravm_ptr_shrink: {
    return DAG.getNode(EraVMISD::PTR_SHRINK, DL, VT, Op.getOperand(1),
                       Op.getOperand(2));
  }
  case Intrinsic::eravm_ptr_pack: {
    return DAG.getNode(EraVMISD::PTR_PACK, DL, VT, Op.getOperand(1),
                       Op.getOperand(2));
  }
  }
  return SDValue();
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

SDValue EraVMTargetLowering::LowerBSWAP(SDValue BSWAP,
                                        SelectionDAG &DAG) const {
  SDNode *N = BSWAP.getNode();
  SDLoc dl(N);
  EVT VT = N->getValueType(0);
  SDValue Op = N->getOperand(0);
  EVT SHVT = getShiftAmountTy(VT, DAG.getDataLayout());

  assert(VT == MVT::i256 && "Unexpected type for bswap");

  std::array<SDValue, 33> Tmp;

  for (int i = 32; i >= 17; i--) {
    Tmp[i] = DAG.getNode(ISD::SHL, dl, VT, Op,
                         DAG.getConstant((i - 17) * 16 + 8, dl, SHVT));
  }

  for (int i = 16; i >= 1; i--) {
    Tmp[i] = DAG.getNode(ISD::SRL, dl, VT, Op,
                         DAG.getConstant((16 - i) * 16 + 8, dl, SHVT));
  }

  APInt FFMask = APInt(256, 255);

  // mask off unwanted bytes
  for (int i = 2; i < 32; i++) {
    Tmp[i] = DAG.getNode(ISD::AND, dl, VT, Tmp[i],
                         DAG.getConstant(FFMask << ((i - 1) * 8), dl, VT));
  }

  // OR everything together
  for (int i = 2; i <= 32; i += 2) {
    Tmp[i] = DAG.getNode(ISD::OR, dl, VT, Tmp[i], Tmp[i - 1]);
  }

  for (int i = 4; i <= 32; i += 4) {
    Tmp[i] = DAG.getNode(ISD::OR, dl, VT, Tmp[i], Tmp[i - 2]);
  }

  for (int i = 8; i <= 32; i += 8) {
    Tmp[i] = DAG.getNode(ISD::OR, dl, VT, Tmp[i], Tmp[i - 4]);
  }

  Tmp[32] = DAG.getNode(ISD::OR, dl, VT, Tmp[32], Tmp[24]);
  Tmp[16] = DAG.getNode(ISD::OR, dl, VT, Tmp[16], Tmp[8]);

  return DAG.getNode(ISD::OR, dl, VT, Tmp[32], Tmp[16]);
}

// This function only counts the number of bits in the lower 128 bits
// It is a slightly modified version of TargetLowering::expandCTPOP
static SDValue countPOP128(SDValue Op, SelectionDAG &DAG) {
  SDLoc dl(Op);
  EVT VT = Op->getValueType(0);

  // This is the "best" algorithm from
  // http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
  SDValue Mask55 = DAG.getConstant(
      APInt::getSplat(VT.getScalarSizeInBits(), APInt(8, 0x55)), dl, VT);
  SDValue Mask33 = DAG.getConstant(
      APInt::getSplat(VT.getScalarSizeInBits(), APInt(8, 0x33)), dl, VT);
  SDValue Mask0F = DAG.getConstant(
      APInt::getSplat(VT.getScalarSizeInBits(), APInt(8, 0x0F)), dl, VT);

  // v = v - ((v >> 1) & 0x55555555...)
  Op = DAG.getNode(ISD::SUB, dl, VT, Op,
                   DAG.getNode(ISD::AND, dl, VT,
                               DAG.getNode(ISD::SRL, dl, VT, Op,
                                           DAG.getConstant(1, dl, MVT::i256)),
                               Mask55));
  // v = (v & 0x33333333...) + ((v >> 2) & 0x33333333...)
  Op = DAG.getNode(ISD::ADD, dl, VT, DAG.getNode(ISD::AND, dl, VT, Op, Mask33),
                   DAG.getNode(ISD::AND, dl, VT,
                               DAG.getNode(ISD::SRL, dl, VT, Op,
                                           DAG.getConstant(2, dl, MVT::i256)),
                               Mask33));
  // v = (v + (v >> 4)) & 0x0F0F0F0F...
  Op = DAG.getNode(ISD::AND, dl, VT,
                   DAG.getNode(ISD::ADD, dl, VT, Op,
                               DAG.getNode(ISD::SRL, dl, VT, Op,
                                           DAG.getConstant(4, dl, MVT::i256))),
                   Mask0F);

  // v = (v * 0x01010101...) >> (Len - 8)
  SDValue Mask01 = DAG.getConstant(
      APInt::getSplat(VT.getScalarSizeInBits(), APInt(8, 0x01)), dl, VT);
  return DAG.getNode(ISD::SRL, dl, VT,
                     DAG.getNode(ISD::MUL, dl, VT, Op, Mask01),
                     DAG.getConstant(120, dl, MVT::i256));
}

SDValue EraVMTargetLowering::LowerCTPOP(SDValue CTPOP,
                                        SelectionDAG &DAG) const {
  SDNode *Node = CTPOP.getNode();
  SDLoc dl(Node);
  EVT VT = Node->getValueType(0);
  SDValue Op = Node->getOperand(0);

  // split the Op into two parts and count separately
  SDValue hiPart =
      DAG.getNode(ISD::SRL, dl, VT, Op, DAG.getConstant(128, dl, MVT::i256));
  SDValue loPart =
      DAG.getNode(ISD::AND, dl, VT, Op,
                  DAG.getConstant(APInt::getLowBitsSet(256, 128), dl, VT));
  auto loSum = DAG.getNode(ISD::AND, dl, VT, countPOP128(loPart, DAG),
                           DAG.getConstant(255, dl, VT));
  auto hiSum = DAG.getNode(ISD::AND, dl, VT, countPOP128(hiPart, DAG),
                           DAG.getConstant(255, dl, VT));
  return DAG.getNode(ISD::ADD, dl, VT, loSum, hiSum);
}

SDValue EraVMTargetLowering::LowerTRAP(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);
  SDValue Chain = Op.getOperand(0);
  return DAG.getNode(EraVMISD::TRAP, dl, MVT::Other, Chain);
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

  SDValue Val;
  switch (N->getOpcode()) {
  default:
    break;
  case ISD::ZERO_EXTEND: {
    SDLoc DL(N);
    SDValue extending_value = N->getOperand(0);
    // combine with SETCC
    if (extending_value->getOpcode() != ISD::SETCC) {
      return Val;
    }
    Val = DAG.getNode(extending_value->getOpcode(), DL, N->getValueType(0),
                      extending_value->ops());
    break;
  }
  }
  return Val;
}

Register
EraVMTargetLowering::getRegisterByName(const char *RegName, LLT VT,
                                       const MachineFunction &MF) const {
  Register Reg = StringSwitch<unsigned>(RegName)
                     .Case("r0", EraVM::R0)
                     .Case("r1", EraVM::R1)
                     .Default(0);
  if (Reg)
    return Reg;

  report_fatal_error(
      Twine("Invalid register name \"" + StringRef(RegName) + "\"."));
}

void EraVMTargetLowering::AdjustInstrPostInstrSelection(MachineInstr &MI,
                                                        SDNode *Node) const {
  assert(MI.hasPostISelHook() && "Expected instruction to have post-isel hook");

  // Set NoMerge to gasleft instructions. This has to be in sync with nomerge
  // attribute in IntrinsicsEraVM.td for this intrinsic.
  if (MI.getOpcode() == EraVM::CTXr_se &&
      getImmOrCImm(MI.getOperand(1)) == EraVMCTX::GAS_LEFT)
    MI.setFlag(MachineInstr::MIFlag::NoMerge);
}
