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

  // SyncVM lacks of native support for signed operations.
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

  setOperationAction(ISD::STACKSAVE, MVT::Other, Custom);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Custom);

  // Intrinsics lowering
  setOperationAction(ISD::INTRINSIC_VOID, MVT::Other, Custom);
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::EXTLOAD, MVT::i256, VT, Custom);
  }

  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::i128}) {
    setOperationAction(ISD::LOAD, VT, Custom);
    setOperationAction(ISD::STORE, VT, Custom);
    setTruncStoreAction(MVT::i256, VT, Expand);
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
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {

  SmallVector<CCValAssign, 1> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  // cannot support return convention or is variadic?
  if (!CCInfo.CheckReturn(Outs, RetCC_SYNCVM) || IsVarArg)
    return false;

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

  if (auto GA = dyn_cast<GlobalAddressSDNode>(Callee.getNode())) {
    auto farcall_pair = [&]() {
      if (GA->getGlobal()->getName() == "__farcall_int") {
        return std::make_pair<uint64_t, bool>(SyncVMISD::FARCALL, true);
      }
      if (GA->getGlobal()->getName() == "__staticcall_int") {
        return std::make_pair<uint64_t, bool>(SyncVMISD::STATICCALL, true);
      }
      if (GA->getGlobal()->getName() == "__delegatecall_int") {
        return std::make_pair<uint64_t, bool>(SyncVMISD::DELEGATECALL, true);
      }
      if (GA->getGlobal()->getName() == "__mimiccall_int") {
        return std::make_pair<uint64_t, bool>(SyncVMISD::MIMICCALL, true);
      }
      return std::make_pair<uint64_t, bool>(0, false);
    }();

    auto farcall_opc = farcall_pair.first;
    bool is_farcall = farcall_pair.second;

    bool is_mimic = farcall_opc == SyncVMISD::MIMICCALL;

    if (is_farcall) {
      if (is_mimic)
        Chain = DAG.getCopyToReg(Chain, DL, SyncVM::R3, OutVals[2], SDValue());
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
      InVals.push_back(DAG.getCopyFromReg(Chain, DL, SyncVM::R1, MVT::i256,
                                          Chain.getValue(1)));
      return Chain;
    }
  }

  // TODO: SyncVM target does not yet support tail call optimization.
  IsTailCall = false;

  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "SyncVM doesn't support non-C calling conventions");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_SYNCVM);

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getNextStackOffset();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;
  SDValue Zero = DAG.getTargetConstant(0, DL, MVT::i256);
  SDValue SPCtx = DAG.getTargetConstant(SyncVMCTX::SP, DL, MVT::i256);

  SDValue InFlag;

  SDValue AbiData = CLI.SyncVMAbiData ? DAG.getRegister(SyncVM::R15, MVT::i256)
                                      : DAG.getRegister(SyncVM::R0, MVT::i256);

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    auto VA = ArgLocs[i];

    if (!VA.isRegLoc()) {
      assert(VA.isMemLoc());
      SDNode *StackPtr =
          DAG.getMachineNode(SyncVM::CTXr, DL, MVT::i256, SPCtx, Zero);

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

  if (CLI.SyncVMAbiData) {
    Chain = DAG.getCopyToReg(Chain, DL, SyncVM::R15, CLI.SyncVMAbiData, InFlag);
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

  if (auto *Invoke = dyn_cast_or_null<InvokeInst>(CLI.CB)) {
    Chain = DAG.getNode(SyncVMISD::INVOKE, DL, NodeTys, Ops);
  } else {
    Chain = DAG.getNode(SyncVMISD::CALL, DL, NodeTys, Ops);
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
  RetCCInfo.AnalyzeCallResult(Ins, CC_SYNCVM);

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

  auto Mask = DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);
  auto Zero = DAG.getConstant(0, DL, MVT::i256);
  auto SignLHS = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask);
  auto SignRHS = DAG.getNode(ISD::AND, DL, MVT::i256, RHS, Mask);
  auto SignXor = DAG.getNode(ISD::XOR, DL, MVT::i256, SignLHS, SignRHS);

  SyncVMCC::CondCodes TCC = SyncVMCC::COND_INVALID;
  switch (CC) {
  default:
    llvm_unreachable("Invalid integer condition!");
  case ISD::SETNE:
    TCC = SyncVMCC::COND_NE;
    break;
  case ISD::SETEQ:
    TCC = SyncVMCC::COND_E;
    break;
  case ISD::SETUGE:
    TCC = SyncVMCC::COND_GE;
    break;
  case ISD::SETULT:
    TCC = SyncVMCC::COND_LT;
    break;
  case ISD::SETULE:
    TCC = SyncVMCC::COND_LE;
    break;
  case ISD::SETUGT:
    TCC = SyncVMCC::COND_GT;
    break;
  case ISD::SETGT: {
    auto Ugt = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETUGT);
    auto SignUlt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETULT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUlt, Ugt, ISD::SETEQ);
    RHS = Zero;
    TCC = SyncVMCC::COND_NE;
    break;
  }
  case ISD::SETGE: {
    auto Uge = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETUGE);
    auto SignUlt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETULT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUlt, Uge, ISD::SETEQ);
    RHS = Zero;
    TCC = SyncVMCC::COND_NE;
    break;
  }
  case ISD::SETLT: {
    auto Ult = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETULT);
    auto SignUgt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETUGT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUgt, Ult, ISD::SETEQ);
    RHS = Zero;
    TCC = SyncVMCC::COND_NE;
    break;
  }
  case ISD::SETLE: {
    auto Ule = DAG.getSelectCC(DL, LHS, RHS, Mask, Zero, ISD::SETULE);
    auto SignUgt =
        DAG.getSelectCC(DL, SignLHS, SignRHS, Mask, Zero, ISD::SETUGT);
    LHS = DAG.getSelectCC(DL, SignXor, Mask, SignUgt, Ule, ISD::SETEQ);
    RHS = Zero;
    TCC = SyncVMCC::COND_NE;
    break;
  }
  }

  return DAG.getConstant(TCC, DL, MVT::i256);
}

SDValue SyncVMTargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    llvm_unreachable("unimplemented operation lowering");
    return SDValue();
  case ISD::STORE:
    return LowerSTORE(Op, DAG);
  case ISD::LOAD:
    return LowerLOAD(Op, DAG);
  case ISD::ZERO_EXTEND:
    return LowerZERO_EXTEND(Op, DAG);
  case ISD::ANY_EXTEND:
    return LowerANY_EXTEND(Op, DAG);
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::CopyToReg:
    return LowerCopyToReg(Op, DAG);
  case ISD::SRA:
    return LowerSRA(Op, DAG);
  case ISD::SDIV:
    return LowerSDIV(Op, DAG);
  case ISD::SREM:
    return LowerSREM(Op, DAG);
  case ISD::INTRINSIC_VOID:
    return LowerINTRINSIC_VOID(Op, DAG);
  case ISD::INTRINSIC_WO_CHAIN:
    return LowerINTRINSIC_WO_CHAIN(Op, DAG);
  case ISD::STACKSAVE:
    return LowerSTACKSAVE(Op, DAG);
  case ISD::STACKRESTORE:
    return LowerSTACKRESTORE(Op, DAG);
  }
}

SDValue SyncVMTargetLowering::LowerANY_EXTEND(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue extending_value = Op.getOperand(0);
  if (extending_value.getValueSizeInBits() == 256) {
    return extending_value;
  }
  return {};
}

SDValue SyncVMTargetLowering::LowerZERO_EXTEND(SDValue Op,
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

SDValue SyncVMTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  StoreSDNode *Store = cast<StoreSDNode>(Op);

  SDValue BasePtr = Store->getBasePtr();
  SDValue Chain = Store->getChain();
  const MachinePointerInfo &PInfo = Store->getPointerInfo();

  const MachineMemOperand *MemOp = Store->getMemOperand();
  auto *MemVal = [MemOp]() {
    if (!MemOp)
      return (const Value *)nullptr;
    return MemOp->getValue();
  }();

  // Generic (fat) pointers need to be stored via ptr.add instead of add.
  if (MemVal && Store->getAddressSpace() == SyncVMAS::AS_STACK &&
      cast<PointerType>(MemVal->getType())->getElementType()->isPointerTy() &&
      cast<PointerType>(MemVal->getType())
              ->getElementType()
              ->getPointerAddressSpace() == SyncVMAS::AS_GENERIC) {
    auto Zero = DAG.getTargetConstant(0, DL, MVT::i256);
    // TODO: Something like SelectAddress is here, need to be reconsidered.
    if (isa<GlobalAddressSDNode>(BasePtr))
      return SDValue(DAG.getMachineNode(SyncVM::PTR_ADDrrs_p, DL, MVT::Other,
                                        {Store->getValue(),
                                         DAG.getRegister(SyncVM::R0, MVT::i256),
                                         Zero, Zero, BasePtr}),
                     0);
    if (auto *FI = dyn_cast<FrameIndexSDNode>(BasePtr))
      return SDValue(DAG.getMachineNode(SyncVM::PTR_ADDrrs_p, DL, MVT::Other,
                                        {Store->getValue(),
                                         DAG.getRegister(SyncVM::R0, MVT::i256),
                                         DAG.getTargetFrameIndex(FI->getIndex(), getPointerTy(DAG.getDataLayout())), Zero, Zero}),
                     0);
    return SDValue(DAG.getMachineNode(SyncVM::PTR_ADDrrs_p, DL, MVT::Other,
                                      {Store->getValue(),
                                       DAG.getRegister(SyncVM::R0, MVT::i256),
                                       Zero, BasePtr, Zero}),
                   0);
  }

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

SDValue SyncVMTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *Load = cast<LoadSDNode>(Op);

  SDValue BasePtr = Load->getBasePtr();
  SDValue Chain = Load->getChain();
  const MachinePointerInfo &PInfo = Load->getPointerInfo();

  const MachineMemOperand *MemOp = Load->getMemOperand();
  auto *MemVal = [MemOp]() {
    if (!MemOp)
      return (const Value *)nullptr;
    return MemOp->getValue();
  }();

  // Generic (fat) pointers need to be loaded via ptr.add instead of add.
  if (MemVal && Load->getAddressSpace() == SyncVMAS::AS_STACK &&
      cast<PointerType>(MemVal->getType())->getElementType()->isPointerTy() &&
      cast<PointerType>(MemVal->getType())
              ->getElementType()
              ->getPointerAddressSpace() == SyncVMAS::AS_GENERIC) {
    auto Zero = DAG.getTargetConstant(0, DL, MVT::i64);
    SDVTList RetTys = DAG.getVTList(MVT::i256, MVT::Other);
    // TODO: Something like SelectAddress is here, need to be reconsidered.
    /*
    MemVal->dump();
    Op.dump();
    BasePtr.dump();
    */
    if (isa<GlobalAddressSDNode>(BasePtr))
      return SDValue(DAG.getMachineNode(SyncVM::PTR_ADDsrr_p, DL, RetTys,
                                        {Zero, Zero, BasePtr,
                                         DAG.getRegister(SyncVM::R0, MVT::i256)}),
                     0);
    if (auto *FI = dyn_cast<FrameIndexSDNode>(BasePtr))
      return SDValue(DAG.getMachineNode(SyncVM::PTR_ADDsrr_p, DL, RetTys,
                                        {DAG.getTargetFrameIndex(FI->getIndex(), getPointerTy(DAG.getDataLayout())), Zero, Zero,
                                         DAG.getRegister(SyncVM::R0, MVT::i256)}),
                     0);
    return SDValue(DAG.getMachineNode(SyncVM::PTR_ADDsrr_p, DL, RetTys,
                                      {Zero, BasePtr, Zero,
                                       DAG.getRegister(SyncVM::R0, MVT::i256)}),
                   0);
  }

  EVT MemVT = Load->getMemoryVT();
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

  SDValue TRUNCATE = DAG.getNode(ISD::TRUNCATE, DL, Load->getMemoryVT(), SHR);

  SDValue Ops[] = {TRUNCATE, Op.getValue(1)};
  return DAG.getMergeValues(Ops, DL);
}

SDValue SyncVMTargetLowering::LowerSRA(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  auto LHS = Op.getOperand(0);
  auto RHS = Op.getOperand(1);
  auto Zero = DAG.getConstant(0, DL, MVT::i256);
  auto One = DAG.getConstant(APInt(256, -1, true), DL, MVT::i256);
  auto Mask = DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);
  auto Sign = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask);
  auto Init = DAG.getSelectCC(DL, Sign, Mask, One, Zero, ISD::SETEQ);
  Mask = DAG.getNode(
      ISD::SHL, DL, MVT::i256, Init,
      DAG.getNode(ISD::SUB, DL, MVT::i256,
                  DAG.getConstant(APInt(256, 256, false), DL, MVT::i256), RHS));
  auto Value = DAG.getNode(ISD::SRL, DL, MVT::i256, LHS, RHS);
  auto Shifted = DAG.getNode(ISD::OR, DL, MVT::i256, Value, Mask);
  return DAG.getSelectCC(DL, RHS, Zero, LHS, Shifted, ISD::SETEQ);
}

SDValue SyncVMTargetLowering::LowerSDIV(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  auto LHS = Op.getOperand(0);
  auto RHS = Op.getOperand(1);
  auto Mask = DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);
  auto Mask2 = DAG.getConstant(APInt(256, -1, true).lshr(1), DL, MVT::i256);
  auto SignLHS = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask);
  auto SignRHS = DAG.getNode(ISD::AND, DL, MVT::i256, RHS, Mask);
  LHS = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask2);
  RHS = DAG.getNode(ISD::AND, DL, MVT::i256, RHS, Mask2);
  auto LHS2Compl = DAG.getNode(ISD::SUB, DL, MVT::i256, Mask, LHS);
  LHS = DAG.getSelectCC(DL, SignLHS, Mask, LHS2Compl, LHS, ISD::SETEQ);
  auto RHS2Compl = DAG.getNode(ISD::SUB, DL, MVT::i256, Mask, RHS);
  RHS = DAG.getSelectCC(DL, SignRHS, Mask, RHS2Compl, RHS, ISD::SETEQ);
  auto Sign = DAG.getNode(ISD::XOR, DL, MVT::i256, SignLHS, SignRHS);
  auto Value = DAG.getNode(ISD::UDIV, DL, MVT::i256, LHS, RHS);
  auto Value2Compl = DAG.getNode(ISD::SUB, DL, MVT::i256, Mask, Value);
  Value2Compl = DAG.getNode(ISD::OR, DL, MVT::i256, Value2Compl, Mask);
  return DAG.getSelectCC(DL, Sign, Mask, Value2Compl, Value, ISD::SETEQ);
}

SDValue SyncVMTargetLowering::LowerSREM(SDValue Op, SelectionDAG &DAG) const {
  auto DL = SDLoc(Op);
  auto LHS = Op.getOperand(0);
  auto RHS = Op.getOperand(1);
  auto Zero = DAG.getConstant(APInt(256, 0, false), DL, MVT::i256);
  auto Mask = DAG.getConstant(APInt(256, 1, false).shl(255), DL, MVT::i256);
  auto Mask2 = DAG.getConstant(APInt(256, -1, true).lshr(1), DL, MVT::i256);
  auto SignLHS = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask);
  auto SignRHS = DAG.getNode(ISD::AND, DL, MVT::i256, RHS, Mask);
  LHS = DAG.getNode(ISD::AND, DL, MVT::i256, LHS, Mask2);
  RHS = DAG.getNode(ISD::AND, DL, MVT::i256, RHS, Mask2);
  auto LHS2Compl = DAG.getNode(ISD::SUB, DL, MVT::i256, Mask, LHS);
  LHS = DAG.getSelectCC(DL, SignLHS, Mask, LHS2Compl, LHS, ISD::SETEQ);
  auto RHS2Compl = DAG.getNode(ISD::SUB, DL, MVT::i256, Mask, RHS);
  RHS = DAG.getSelectCC(DL, SignRHS, Mask, RHS2Compl, RHS, ISD::SETEQ);
  auto Value = DAG.getNode(ISD::UREM, DL, MVT::i256, LHS, RHS);
  auto Value2Compl = DAG.getNode(ISD::SUB, DL, MVT::i256, Mask, Value);
  Value2Compl = DAG.getNode(ISD::OR, DL, MVT::i256, Value2Compl, Mask);
  auto Result =
      DAG.getSelectCC(DL, SignLHS, Mask, Value2Compl, Value, ISD::SETEQ);
  return DAG.getSelectCC(DL, Value, Zero, Value, Result, ISD::SETEQ);
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

SDValue SyncVMTargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  SDValue TargetCC = EmitCMP(LHS, RHS, CC, DL, DAG);
  SDValue Cmp = DAG.getNode(SyncVMISD::CMP, DL, MVT::Glue, LHS, RHS);
  return DAG.getNode(SyncVMISD::BR_CC, DL, Op.getValueType(), Chain, Dest,
                     TargetCC, Cmp);
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
  SDValue Cmp = DAG.getNode(SyncVMISD::CMP, DL, MVT::Glue, LHS, RHS);

  SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Glue);
  SDValue Ops[] = {TrueV, FalseV, TargetCC, Cmp};

  return DAG.getNode(SyncVMISD::SELECT_CC, DL, VTs, Ops);
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

MachineBasicBlock *
SyncVMTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                  MachineBasicBlock *BB) const {
  llvm_unreachable("No instruction require custom inserter");
  return nullptr;
}
SDValue SyncVMTargetLowering::LowerINTRINSIC_WO_CHAIN(SDValue Op,
                                                      SelectionDAG &DAG) const {
  unsigned IntNo =
      cast<ConstantSDNode>(
          Op.getOperand(Op.getOperand(0).getValueType() == MVT::Other))
          ->getZExtValue();
  if (IntNo == Intrinsic::syncvm_ptr_add) {
    LLVM_DEBUG(dbgs() << "LowerINTRINSIC_WO_CHAIN matched: ptr.add\n");
    return DAG.getNode(SyncVMISD::PTR_ADD, SDLoc(Op), Op.getValueType(),
                       Op.getOperand(1), Op.getOperand(2));
  } else if (IntNo == Intrinsic::syncvm_ptr_pack) {
    LLVM_DEBUG(dbgs() << "LowerINTRINSIC_WO_CHAIN matched: ptr.pack\n");
    return DAG.getNode(SyncVMISD::PTR_PACK, SDLoc(Op), Op.getValueType(),
                       Op.getOperand(1), Op.getOperand(2));
  } else {
    return SDValue();
  }
}

SDValue SyncVMTargetLowering::LowerINTRINSIC_VOID(SDValue Op,
                                                  SelectionDAG &DAG) const {
  unsigned IntNo =
      cast<ConstantSDNode>(
          Op.getOperand(Op.getOperand(0).getValueType() == MVT::Other))
          ->getZExtValue();

  if (IntNo != Intrinsic::syncvm_throw && IntNo != Intrinsic::syncvm_return &&
      IntNo != Intrinsic::syncvm_revert)
    return {};
  SDLoc DL(Op);
  auto CTR =
      DAG.getCopyToReg(Op.getOperand(0), DL, SyncVM::R1, Op.getOperand(2));
  switch (IntNo) {
  default:
    llvm_unreachable("Unexpected intrinsic");
    return {};
  case Intrinsic::syncvm_throw:
    return DAG.getNode(SyncVMISD::THROW, DL, MVT::Other, CTR,
                       DAG.getRegister(SyncVM::R1, MVT::i256));
  case Intrinsic::syncvm_return:
    return DAG.getNode(SyncVMISD::RETURN, DL, MVT::Other, CTR,
                       DAG.getRegister(SyncVM::R1, MVT::i256));
  case Intrinsic::syncvm_revert:
    return DAG.getNode(SyncVMISD::REVERT, DL, MVT::Other, CTR,
                       DAG.getRegister(SyncVM::R1, MVT::i256));
  }
}

SDValue SyncVMTargetLowering::LowerSTACKSAVE(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDVTList RetTys = DAG.getVTList(MVT::i256, MVT::Other);
  return DAG.getNode(SyncVMISD::GET_SP, SDLoc(Op), RetTys, Op.getOperand(0));
}

SDValue SyncVMTargetLowering::LowerSTACKRESTORE(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDVTList GetSPTys = DAG.getVTList(MVT::i256, MVT::Other);
  SDValue CurrentSP =
      DAG.getNode(SyncVMISD::GET_SP, DL, GetSPTys, Op.getOperand(0));
  SDValue SPDelta =
      DAG.getNode(ISD::SUB, DL, MVT::i256, Op.getOperand(1), CurrentSP);
  return DAG.getNode(SyncVMISD::CHANGE_SP, DL, MVT::Other,
                     CurrentSP.getValue(1), SPDelta);
}

void SyncVMTargetLowering::ReplaceNodeResults(SDNode *N,
                                              SmallVectorImpl<SDValue> &Results,
                                              SelectionDAG &DAG) const {
  LowerOperationWrapper(N, Results, DAG);
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

SDValue SyncVMTargetLowering::PerformDAGCombine(SDNode *N,
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
