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

/// Helper function: wrap symbols according to their address space.
/// Precondition:
/// \p ValueToWrap is SDValue containing a symbol value (global address,
/// external symbol or block address)
SDValue SyncVMTargetLowering::wrapSymbol(const SDValue &ValueToWrap,
                                         SelectionDAG &DAG, const SDLoc &DL,
                                         unsigned addrspace) const {
  auto VT = getPointerTy(DAG.getDataLayout());
  switch (addrspace) {
  case SyncVMAS::AS_STACK:
    return DAG.getNode(SyncVMISD::GAStack, DL, VT, ValueToWrap);
  case SyncVMAS::AS_CODE:
    return DAG.getNode(SyncVMISD::GACode, DL, VT, ValueToWrap);
  default:
    llvm_unreachable("Global symbol in unexpected addr space");
  }
  return {};
}

/// Wrap a global address and lower to TargetGlobalAddress.
/// The \p ValueToWrap must be a GlobalAddressSDNode.
SDValue SyncVMTargetLowering::wrapGlobalAddress(const SDValue &ValueToWrap,
                                                SelectionDAG &DAG,
                                                const SDLoc &DL) const {
  // convert to TargetGlobalAddress
  auto *GANode = dyn_cast<GlobalAddressSDNode>(ValueToWrap.getNode());
  auto TGA = DAG.getTargetGlobalAddress(
      GANode->getGlobal(), DL, ValueToWrap.getValueType(), GANode->getOffset());
  return wrapSymbol(TGA, DAG, DL, GANode->getAddressSpace());
}

/// Wrap a external symbol and lower to TargetExternalSymbol.
/// The \p ValueToWrap must be a ExternalSymbolSDNode.
SDValue SyncVMTargetLowering::wrapExternalSymbol(const SDValue &ValueToWrap,
                                                 SelectionDAG &DAG,
                                                 const SDLoc &DL) const {
  // convert to TargetExternalSymbol
  auto *ESNode = dyn_cast<ExternalSymbolSDNode>(ValueToWrap.getNode());
  auto TES = DAG.getTargetExternalSymbol(ESNode->getSymbol(),
                                         ValueToWrap.getValueType());
  return wrapSymbol(TES, DAG, DL, SyncVMAS::AS_CODE);
}

SyncVMTargetLowering::SyncVMTargetLowering(const TargetMachine &TM,
                                           const SyncVMSubtarget &STI)
    : TargetLowering(TM) {
  // Set up the register classes.
  addRegisterClass(MVT::i256, &SyncVM::GR256RegClass);
  addRegisterClass(MVT::fatptr, &SyncVM::GRPTRRegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Provide all sorts of operation actions
  setStackPointerRegisterToSaveRestore(SyncVM::SP);

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
                      ISD::STACKSAVE, ISD::STACKRESTORE, ISD::TRAP},
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

  // special DAG combining handling for SyncVM
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

  if (Outs.size() >= SyncVM::GR256RegClass.getNumRegs() - 1)
    return false;

  SmallVector<CCValAssign, 1> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  // cannot support return convention or is variadic?
  if (!CCInfo.CheckReturn(Outs, RetCC_SYNCVM))
    return false;

  return true;
}

SDValue
SyncVMTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                  bool IsVarArg,
                                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                                  const SmallVectorImpl<SDValue> &OutVals,
                                  const SDLoc &DL, SelectionDAG &DAG) const {
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
      auto *RC = VA.getValVT() == MVT::fatptr ? &SyncVM::GRPTRRegClass
                                              : &SyncVM::GR256RegClass;
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
  auto GA = dyn_cast<GlobalAddressSDNode>(Callee.getNode());
  if (!GA)
    return 0;
  return StringSwitch<uint64_t>(GA->getGlobal()->getName())
      .Case("__farcall_int", SyncVMISD::FARCALL)
      .Case("__staticcall_int", SyncVMISD::STATICCALL)
      .Case("__delegatecall_int", SyncVMISD::DELEGATECALL)
      .Case("__mimiccall_int", SyncVMISD::MIMICCALL)
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
  // TODO: SyncVM target does not yet support tail call optimization.
  CLI.IsTailCall = false;
  CallingConv::ID CallConv = CLI.CallConv;
  // We meddle with number of parameters, set vararg to true to prevent
  // assertion that the number of parameters before lowering is equal to the
  // number of parameters after lowering.
  bool IsVarArg = CLI.IsVarArg = true;

  uint64_t FarcallOpcode = farcallOpcode(Callee);
  if (!FarcallOpcode)
    return {};
  bool IsMimicCall = FarcallOpcode == SyncVMISD::MIMICCALL;

  SmallVector<CCValAssign, 13> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_SYNCVM);
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
    Chain = DAG.getCopyToReg(Chain, DL, SyncVM::R15, OutVals.back(), InFlag);
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
    Ops.push_back(DAG.getRegister(SyncVM::R15, MVT::i256));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain = DAG.getNode(FarcallOpcode, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain =
      DAG.getCALLSEQ_END(Chain, DAG.getConstant(0, DL, MVT::i256, true),
                         DAG.getConstant(0, DL, MVT::i256, true), InFlag, DL);
  InFlag = Chain.getValue(1);

  InVals.push_back(DAG.getCopyFromReg(Chain, DL, SyncVM::R1, MVT::fatptr, {}));
  InVals.push_back(DAG.getCopyFromReg(InVals[0].getValue(1), DL, SyncVM::R1,
                                      MVT::fatptr, InVals[0].getValue(2)));
  return InVals[1].getValue(1);
}

SDValue
SyncVMTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
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
  // TODO: SyncVM target does not yet support tail call optimization.
  CLI.IsTailCall = false;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_SYNCVM);

  SDValue InFlag;

  if (!CallingConvSupported(CallConv))
    fail(DL, DAG, "SyncVM doesn't support non-C calling conventions");

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getNextStackOffset();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;
  SDValue Zero = DAG.getTargetConstant(0, DL, MVT::i256);
  SDValue SPCtx = DAG.getTargetConstant(SyncVMCTX::SP, DL, MVT::i256);

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

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = wrapGlobalAddress(Callee, DAG, DL);
  }
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
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
  case ISD::CopyToReg:          return LowerCopyToReg(Op, DAG);
  case ISD::SRA:                return LowerSRA(Op, DAG);
  case ISD::SDIV:               return LowerSDIV(Op, DAG);
  case ISD::SREM:               return LowerSREM(Op, DAG);
  case ISD::INTRINSIC_VOID:     return LowerINTRINSIC_VOID(Op, DAG);
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

  // for now only handle cases where alignment == 1
  // indexed loads and stores are illegal before ISEL,
  // the SyncVMCombineToIndexedMemops pass is used to do the trick.
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

SDValue SyncVMTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *Load = cast<LoadSDNode>(Op);

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
SDValue SyncVMTargetLowering::LowerSDIV(SDValue Op, SelectionDAG &DAG) const {
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
SDValue SyncVMTargetLowering::LowerSREM(SDValue Op, SelectionDAG &DAG) const {
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

SDValue SyncVMTargetLowering::LowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  return wrapGlobalAddress(Op, DAG, SDLoc(Op));
}

SDValue SyncVMTargetLowering::LowerExternalSymbol(SDValue Op,
                                                  SelectionDAG &DAG) const {
  SDLoc dl(Op);
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetExternalSymbol(Sym, PtrVT);

  // SyncVM doesn't support external symbols, but it make sense to enable
  // generic codegen tests to pass. In case an external symbol persist linker
  // will emit a diagnostic.
  return DAG.getNode(SyncVMISD::GACode, dl, PtrVT, Result);
}

SDValue SyncVMTargetLowering::LowerBlockAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc dl(Op);
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  EVT PtrVT = Op.getValueType();
  SDValue Result = DAG.getTargetBlockAddress(BA, PtrVT);

  return DAG.getNode(SyncVMISD::GACode, dl, PtrVT, Result);
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

// TODO: CPR-885 Get rid of LowerCopyToReg in ISelLowering
// It seems an outdated design solution from SyncVM v1.0. We should check if it
// still needed, and if it is, use BytesToCells pass instead.
SDValue SyncVMTargetLowering::LowerCopyToReg(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Src = Op.getOperand(2);
  if (Src.getOpcode() == ISD::FrameIndex ||
      (Src.getOpcode() == ISD::ADD &&
       Src.getOperand(0).getOpcode() == ISD::FrameIndex)) {
    Register Reg = cast<RegisterSDNode>(Op.getOperand(1))->getReg();
    // TODO: It's really a hack:
    // If we put an expression involving stack frame, we replase the address in
    // bytes with the address in cells. Probably we need to reconsider that
    // desing.
    SDValue Div = DAG.getNode(ISD::UDIV, DL, Op.getValueType(), Src,
                              DAG.getConstant(32, DL, Op.getValueType()));
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
  unsigned IntrinsicID = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  EVT VT = Op.getValueType();
  SDLoc DL(Op);

  switch (IntrinsicID) {
  default:
    break;
  case Intrinsic::syncvm_ptrtoint: {
    return DAG.getNode(SyncVMISD::PTR_TO_INT, DL, VT, Op.getOperand(1));
  }
  case Intrinsic::syncvm_ptr_add: {
    return DAG.getNode(SyncVMISD::PTR_ADD, DL, VT, Op.getOperand(1),
                       Op.getOperand(2));
  }
  case Intrinsic::syncvm_ptr_shrink: {
    return DAG.getNode(SyncVMISD::PTR_SHRINK, DL, VT, Op.getOperand(1),
                       Op.getOperand(2));
  }
  case Intrinsic::syncvm_ptr_pack: {
    return DAG.getNode(SyncVMISD::PTR_PACK, DL, VT, Op.getOperand(1),
                       Op.getOperand(2));
  }
  }
  return SDValue();
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

SDValue SyncVMTargetLowering::LowerBSWAP(SDValue BSWAP,
                                         SelectionDAG &DAG) const {
  SDNode *N = BSWAP.getNode();
  SDLoc dl(N);
  EVT VT = N->getValueType(0);
  SDValue Op = N->getOperand(0);
  EVT SHVT = getShiftAmountTy(VT, DAG.getDataLayout());

  assert(VT == MVT::i256 && "Unexpected type for bswap");

  SDValue Tmp[33];

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

SDValue SyncVMTargetLowering::LowerCTPOP(SDValue CTPOP,
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

SDValue SyncVMTargetLowering::LowerTRAP(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);
  SDValue Chain = Op.getOperand(0);
  return DAG.getNode(SyncVMISD::TRAP, dl, MVT::Other, Chain);
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

Register
SyncVMTargetLowering::getRegisterByName(const char *RegName, LLT VT,
                                        const MachineFunction &MF) const {
  Register Reg = StringSwitch<unsigned>(RegName)
                     .Case("r0", SyncVM::R0)
                     .Case("r1", SyncVM::R1)
                     .Default(0);
  if (Reg)
    return Reg;

  report_fatal_error(
      Twine("Invalid register name \"" + StringRef(RegName) + "\"."));
}
