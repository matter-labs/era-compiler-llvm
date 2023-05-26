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
                      ISD::UREM, ISD::SREM, ISD::SETCC, ISD::SELECT},
                     MVT::i256, Legal);

  for (auto CC : {ISD::SETULT, ISD::SETUGT, ISD::SETLT, ISD::SETGT, ISD::SETGE,
                  ISD::SETUGE, ISD::SETLE, ISD::SETULE, ISD::SETEQ, ISD::SETNE})
    setCondCodeAction(CC, MVT::i256, Legal);

  // Don't use constant pools.
  // TODO: Probably this needs to be relaxed in the future.
  setOperationAction(ISD::Constant, MVT::i256, Legal);

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

//===----------------------------------------------------------------------===//
// Calling Convention cmplementation.
//===----------------------------------------------------------------------===//

static void fail(const SDLoc &DL, SelectionDAG &DAG, const char *msg) {
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(
      DiagnosticInfoUnsupported(MF.getFunction(), msg, DL.getDebugLoc()));
}

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
  Chain = DAG.getNode(EVMISD::RETURN, DL, MVT::Other, RetOps);

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
