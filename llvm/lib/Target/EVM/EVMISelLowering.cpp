//===-------- EVMISelLowering.cpp - EVM DAG Lowering Implementation  ------===//
//
// This file implements the EVMTargetLowering class.
//
//===----------------------------------------------------------------------===//

#include "EVMISelLowering.h"
#include "EVMTargetMachine.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/IR/DiagnosticInfo.h"

using namespace llvm;

#define DEBUG_TYPE "evm-lower"

EVMTargetLowering::EVMTargetLowering(const TargetMachine &TM,
                                     const EVMSubtarget &STI)
    : TargetLowering(TM) {

  // Booleans always contain 0 or 1.
  setBooleanContents(ZeroOrOneBooleanContent);

  // Set up the register classes.
  addRegisterClass(MVT::i256, &EVM::GPRRegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Provide all sorts of operation actions
  setStackPointerRegisterToSaveRestore(EVM::SP);

  // By default, expand all i256bit operations
  for (unsigned Opc = 0; Opc < ISD::BUILTIN_OP_END; ++Opc)
    setOperationAction(Opc, MVT::i256, Expand);

  // Legal operations
  setOperationAction({ISD::ADD, ISD::SUB, ISD::MUL, ISD::AND, ISD::OR, ISD::XOR,
                      ISD::SHL, ISD::SRL, ISD::SRA, ISD::SDIV, ISD::UDIV,
                      ISD::UREM, ISD::SREM, ISD::SETCC, ISD::SELECT,
                      ISD::FrameIndex},
                     MVT::i256, Legal);

  for (auto CC : {ISD::SETULT, ISD::SETUGT, ISD::SETLT, ISD::SETGT, ISD::SETGE,
                  ISD::SETUGE, ISD::SETLE, ISD::SETULE, ISD::SETEQ, ISD::SETNE})
    setCondCodeAction(CC, MVT::i256, Legal);

  // Don't use constant pools.
  // TODO: Probably this needs to be relaxed in the future.
  setOperationAction(ISD::Constant, MVT::i256, Legal);

  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::i128}) {
    setOperationAction(ISD::MERGE_VALUES, VT, Promote);
    setTruncStoreAction(MVT::i256, VT, Custom);
  }

  // Custom lowering of extended loads.
  for (MVT VT : {MVT::i1, MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::i128}) {
    setLoadExtAction(ISD::SEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i256, VT, Custom);
    setLoadExtAction(ISD::EXTLOAD, MVT::i256, VT, Custom);
  }

  // Custom lowering operations.
  setOperationAction({ISD::GlobalAddress, ISD::LOAD, ISD::STORE}, MVT::i256,
                     Custom);

  setJumpIsExpensive(false);
  setMaximumJumpTableSize(0);
}

const char *EVMTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<EVMISD::NodeType>(Opcode)) {
  case EVMISD::FIRST_NUMBER:
    break;
#define HANDLE_NODETYPE(NODE)                                                  \
  case EVMISD::NODE:                                                           \
    return "EVMISD::" #NODE;
#include "EVMISD.def"
#undef HANDLE_NODETYPE
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// EVM Lowering private implementation.
//===----------------------------------------------------------------------===//

static void fail(const SDLoc &DL, SelectionDAG &DAG, const char *msg) {
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(
      DiagnosticInfoUnsupported(MF.getFunction(), msg, DL.getDebugLoc()));
}

SDValue EVMTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  switch (Op.getOpcode()) {
  default:
    llvm_unreachable("Unimplemented operation lowering");
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::LOAD:
    return LowerLOAD(Op, DAG);
  case ISD::STORE:
    return LowerSTORE(Op, DAG);
  }
}

SDValue EVMTargetLowering::LowerGlobalAddress(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const auto *GA = cast<GlobalAddressSDNode>(Op);
  EVT VT = Op.getValueType();
  assert(GA->getTargetFlags() == 0 &&
         "Unexpected target flags on generic GlobalAddressSDNode");
  if (GA->getAddressSpace() != 0)
    fail(DL, DAG, "EVM expects only the 0 address space");

  return DAG.getNode(
      EVMISD::TARGET_ADDR_WRAPPER, DL, VT,
      DAG.getTargetGlobalAddress(GA->getGlobal(), DL, VT, GA->getOffset()));
}

SDValue EVMTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *Load = cast<LoadSDNode>(Op);

  SDValue BasePtr = Load->getBasePtr();
  SDValue Chain = Load->getChain();
  const MachinePointerInfo &PInfo = Load->getPointerInfo();

  EVT MemVT = Load->getMemoryVT();
  unsigned MemVTSize = MemVT.getSizeInBits();
  if (MemVT == MVT::i256)
    return {};

  auto ExtType = Load->getExtensionType();

  assert(Op->getValueType(0) == MVT::i256 && "Unexpected load type");
  assert(ExtType != ISD::NON_EXTLOAD && "Expected extended LOAD");
  assert(MemVT.isScalarInteger() && "Expected scalar load");
  assert(MemVTSize < 256 && "Expected < 256-bits sized loads");

  LLVM_DEBUG(errs() << "Special handling of extended LOAD node:\n";
             Op.dump(&DAG));

  // As the EVM architecture has only 256-bits load, additional handling
  // is required to load smaller types.
  // In the EVM architecture, values located on stack are righ-aligned. Values
  // located in memory are left-aligned.

  // A small load is implemented as follows:
  // 1. L = load 256 bits starting from the pointer
  // 2. Shift the value to the right
  //   V = V >> (256 - MemVTSize)
  // 3. Sign-expand the value for SEXTLOAD

  SDValue LoadValue =
      DAG.getLoad(MVT::i256, DL, Chain, BasePtr, PInfo, Load->getAlign());

  SDValue Res = DAG.getNode(ISD::SRL, DL, MVT::i256, LoadValue,
                            DAG.getConstant(256 - MemVTSize, DL, MVT::i256));

  if (ExtType == ISD::SEXTLOAD)
    Res = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i256, Res,
                      DAG.getValueType(MemVT));

  return DAG.getMergeValues({Res, LoadValue.getValue(1)}, DL);
}

SDValue EVMTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  StoreSDNode *Store = cast<StoreSDNode>(Op);

  SDValue BasePtr = Store->getBasePtr();
  SDValue Chain = Store->getChain();
  const MachinePointerInfo &PInfo = Store->getPointerInfo();

  EVT MemVT = Store->getMemoryVT();
  unsigned MemVTSize = MemVT.getSizeInBits();
  assert(MemVT.isScalarInteger() && "Expected a scalar store");
  if (MemVT == MVT::i256 || MemVT == MVT::i8)
    return {};

  assert(MemVTSize < 256 && "Expected < 256-bits sized stores");

  LLVM_DEBUG(errs() << "Special handling of STORE node:\n"; Op.dump(&DAG));

  // As the EVM architecture has only 256-bits stors, additional handling
  // is required to store smaller types.
  // In the EVM architecture, values located on stack are righ-aligned. Values
  // located in memory are left-aligned.
  // The i8 store is handled a special way, as EVM has MSTORE8 instruction
  // for this case.

  // A small store is implemented as follows:
  // 1. L = load 256 bits starting from the pointer
  // 2. Clear the MSB memory bits that will be overwritten
  //   L = L << MemVTSize
  //   L = L >> MemVTSize
  // 3. Zero-expand the value being stored
  // 4. Shift the value to the left
  //   V = V << (256 - MemVTSize)
  // 5. S = or L, V
  // 6. store i256 S

  // Load 256 bits starting from the pointer.
  SDValue OrigValue = DAG.getExtLoad(
      ISD::NON_EXTLOAD, DL, MVT::i256, Chain, BasePtr, PInfo, MVT::i256,
      Store->getAlign(), MachineMemOperand::MOLoad, Store->getAAInfo());
  Chain = OrigValue.getValue(1);

  // Clear LSB bits of the memory word that will be overwritten.
  OrigValue = DAG.getNode(ISD::SRL, DL, MVT::i256, OrigValue,
                          DAG.getConstant(MemVTSize, DL, MVT::i256));
  OrigValue = DAG.getNode(ISD::SHL, DL, MVT::i256, OrigValue,
                          DAG.getConstant(MemVTSize, DL, MVT::i256));

  SDValue ZextValue = DAG.getZeroExtendInReg(Store->getValue(), DL, MVT::i256);

  SDValue StoreValue =
      DAG.getNode(ISD::SHL, DL, MVT::i256, ZextValue,
                  DAG.getConstant(256 - MemVTSize, DL, MVT::i256));

  SDValue OR = DAG.getNode(ISD::OR, DL, MVT::i256, StoreValue, OrigValue);

  return DAG.getStore(Chain, DL, OR, BasePtr, PInfo);
}

void EVMTargetLowering::ReplaceNodeResults(SDNode *N,
                                           SmallVectorImpl<SDValue> &Results,
                                           SelectionDAG &DAG) const {
  LowerOperationWrapper(N, Results, DAG);
}

//===----------------------------------------------------------------------===//
// Calling Convention cmplementation.
//===----------------------------------------------------------------------===//

// Test whether the given calling convention is supported.
static bool callingConvSupported(CallingConv::ID CallConv) {
  // TODO: EVM currently doesn't distinguish between different calling
  // convensions.
  return CallConv == CallingConv::C || CallConv == CallingConv::Fast ||
         CallConv == CallingConv::Cold;
}

SDValue EVMTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  if (!callingConvSupported(CallConv))
    fail(DL, DAG, "EVM doesn't support non-C calling conventions");
  if (IsVarArg)
    fail(DL, DAG, "VarArg is not supported yet");

  MachineFunction &MF = DAG.getMachineFunction();

  // Set up the incoming ARGUMENTS value, which serves to represent the liveness
  // of the incoming values before they're represented by virtual registers.
  MF.getRegInfo().addLiveIn(EVM::ARGUMENTS);

  for (const ISD::InputArg &In : Ins) {
    if (In.Flags.isInAlloca())
      fail(DL, DAG, "EVM hasn't implemented inalloca arguments");
    if (In.Flags.isNest())
      fail(DL, DAG, "EVM hasn't implemented nest arguments");
    if (In.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "EVM hasn't implemented cons regs arguments");
    if (In.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "EVM hasn't implemented cons regs last arguments");
    if (In.Flags.isByVal())
      fail(DL, DAG, "EVM hasn't implemented by val arguments");

    // As EVM has no physical registers, we use ARGUMENT instruction to emulate
    // live-in registers.
    InVals.push_back(In.Used ? DAG.getNode(EVMISD::ARGUMENT, DL, In.VT,
                                           DAG.getTargetConstant(InVals.size(),
                                                                 DL, MVT::i32))
                             : DAG.getUNDEF(In.VT));
  }

  return Chain;
}

SDValue EVMTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                     SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc DL = CLI.DL;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  MachineFunction &MF = DAG.getMachineFunction();
  auto Layout = MF.getDataLayout();

  CallingConv::ID CallConv = CLI.CallConv;
  if (!callingConvSupported(CallConv))
    fail(DL, DAG,
         "EVM doesn't support language-specific or target-specific "
         "calling conventions yet");
  if (CLI.IsPatchPoint)
    fail(DL, DAG, "EVM doesn't support patch point yet");

  // TODO: add suport of tail call optimization
  CLI.IsTailCall = false;

  if (CLI.IsVarArg)
    fail(DL, DAG, "EVM hasn't implemented variable arguments");

  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;

  for (const auto &Out : Outs) {
    if (Out.Flags.isNest())
      fail(DL, DAG, "EVM hasn't implemented nest arguments");
    if (Out.Flags.isInAlloca())
      fail(DL, DAG, "EVM hasn't implemented inalloca arguments");
    if (Out.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "EVM hasn't implemented cons regs arguments");
    if (Out.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "EVM hasn't implemented cons regs last arguments");
    if (Out.Flags.isByVal())
      fail(DL, DAG, "EVM hasn't implemented byval arguments");
  }

  if (Callee->getOpcode() == ISD::GlobalAddress) {
    GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Callee);
    Callee = DAG.getTargetGlobalAddress(GA->getGlobal(), DL,
                                        getPointerTy(DAG.getDataLayout()),
                                        GA->getOffset());
    Callee = DAG.getNode(EVMISD::TARGET_ADDR_WRAPPER, DL,
                         getPointerTy(DAG.getDataLayout()), Callee);
  }

  // Compute the operands for the CALLn node.
  SmallVector<SDValue, 16> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add all fixed arguments.
  Ops.append(OutVals.begin(), OutVals.end());

  assert(Ins.size() <= 1 &&
         "EVM doesn't support functions returning more that one value");

  SmallVector<EVT, 8> InTys;
  for (const auto &In : Ins) {
    assert(!In.Flags.isByVal() && "byval is not valid for return values");
    assert(!In.Flags.isNest() && "nest is not valid for return values");
    if (In.Flags.isInAlloca())
      fail(DL, DAG, "EVM hasn't implemented inalloca return values");
    if (In.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "EVM hasn't implemented cons regs return values");
    if (In.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "EVM hasn't implemented cons regs last return values");
    // Ignore In.getNonZeroOrigAlign() because all our arguments are passed in
    // registers.
    InTys.push_back(In.VT);
  }

  InTys.push_back(MVT::Other);
  SDVTList InTyList = DAG.getVTList(InTys);
  SDValue Res = DAG.getNode(Ins.size() == 0 ? EVMISD::CALL0 : EVMISD::CALL1, DL,
                            InTyList, Ops);

  if(Ins.size() > 0)
    InVals.push_back(Res.getValue(0));

  // Return the chain
  return Res.getValue(Ins.size());
}

SDValue
EVMTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                               bool /*IsVarArg*/,
                               const SmallVectorImpl<ISD::OutputArg> &Outs,
                               const SmallVectorImpl<SDValue> &OutVals,
                               const SDLoc &DL, SelectionDAG &DAG) const {
  assert((Outs.size() <= 1) && "EVM can only return up to one value");
  if (!callingConvSupported(CallConv))
    fail(DL, DAG, "EVM doesn't support non-C calling conventions");

  SmallVector<SDValue, 4> RetOps(1, Chain);
  RetOps.append(OutVals.begin(), OutVals.end());
  Chain = DAG.getNode(EVMISD::RET, DL, MVT::Other, RetOps);

  // Record the number and types of the return values.
  for (const ISD::OutputArg &Out : Outs) {
    assert(!Out.Flags.isByVal() && "byval is not valid for return values");
    assert(!Out.Flags.isNest() && "nest is not valid for return values");
    assert(Out.IsFixed && "non-fixed return value is not valid");
    if (Out.Flags.isInAlloca())
      fail(DL, DAG, "EVM hasn't implemented inalloca results");
    if (Out.Flags.isInConsecutiveRegs())
      fail(DL, DAG, "EVM hasn't implemented cons regs results");
    if (Out.Flags.isInConsecutiveRegsLast())
      fail(DL, DAG, "EVM hasn't implemented cons regs last results");
  }

  return Chain;
}

bool EVMTargetLowering::CanLowerReturn(
    CallingConv::ID /*CallConv*/, MachineFunction & /*MF*/, bool /*IsVarArg*/,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    LLVMContext & /*Context*/) const {
  return true;
}

MachineBasicBlock *
EVMTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("Unexpected instr type to insert");
  case EVM::SELECT:
    return emitSelect(MI, BB);
  }
}

MachineBasicBlock *EVMTargetLowering::emitSelect(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  const TargetInstrInfo *TII = BB->getParent()->getSubtarget().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();
  // To "insert" a SELECT instruction, we actually have to insert the
  // diamond control-flow pattern.  The incoming instruction knows the
  // destination vreg to set, the condition code register to branch on and the
  // true/false values to select between.
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();

  //  ThisMBB:
  //  ...
  //   TrueVal = ...
  //   setcc $cond, $2, $1
  //   JUMPI SinkMBB, $cond
  //   fallthrough --> FHMBB
  MachineBasicBlock *ThisMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *FHMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkMBB = F->CreateMachineBasicBlock(LLVM_BB);
  F->insert(It, FHMBB);
  F->insert(It, SinkMBB);

  // Transfer the remainder of BB and its successor edges to SinkMBB.
  SinkMBB->splice(SinkMBB->begin(), BB,
                  std::next(MachineBasicBlock::iterator(MI)), BB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(BB);

  // Next, add the true and fallthrough blocks as its successors.
  BB->addSuccessor(FHMBB);
  BB->addSuccessor(SinkMBB);

  BuildMI(BB, DL, TII->get(EVM::JUMPI))
      .addMBB(SinkMBB)
      .addReg(MI.getOperand(1).getReg());

  //  FHMBB:
  //   %FalseValue = ...
  //   # fallthrough to SinkMBB
  BB = FHMBB;

  // Update machine-CFG edges
  BB->addSuccessor(SinkMBB);

  //  SinkMBB:
  //   %Result = phi [ %TrueValue, ThisMBB ], [ %FalseValue, FHMBB ]
  //  ...
  BB = SinkMBB;

  BuildMI(*BB, BB->begin(), DL, TII->get(EVM::PHI), MI.getOperand(0).getReg())
      .addReg(MI.getOperand(2).getReg())
      .addMBB(ThisMBB)
      .addReg(MI.getOperand(3).getReg())
      .addMBB(FHMBB);

  MI.eraseFromParent(); // The pseudo instruction is gone now.
  return BB;
}
