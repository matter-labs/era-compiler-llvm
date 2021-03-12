//===-- SyncVMInstrInfo.td - SyncVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the SyncVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "SyncVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// Type Profiles.
//===----------------------------------------------------------------------===//
def SDT_SyncVMCall         : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;

//===----------------------------------------------------------------------===//
// SyncVM Specific Node Definitions.
//===----------------------------------------------------------------------===//

def SyncVMret   : SDNode<"SyncVMISD::RET", SDTNone,
                   [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def SyncVMcall  : SDNode<"SyncVMISD::CALL", SDT_SyncVMCall,
                   [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;


//===----------------------------------------------------------------------===//
// SyncVM data classes.
//===----------------------------------------------------------------------===//

def ImmediateSrc : Operand<i256>, IntImmLeaf<i256, [{
  int64_t Val = Imm.getZExtValue();
  return ((unsigned long) Val) <= ((1ul << 128) - 1);
}]> {
//  let ParserMatchClass = MemAsmOperand;
//  let PrintMethod = "printSrcMemOperand";
//  let DecoderMethod = "DecodeMemOperand";
//  let EncoderMethod = "getMemOpValue";

//  let MIOperandInfo = (ops GR256, i128imm);
}



//===----------------------------------------------------------------------===//
// The NoOperation instruction
//===----------------------------------------------------------------------===//

def NOOP : Empty< 0x00, "noop", [] >;



//===----------------------------------------------------------------------===//
// Arithmetic
//===----------------------------------------------------------------------===//

def ADDrrr     : ArithmeticRRR< 0x01, "add", add >;
def SUBrrr     : ArithmeticRRR< 0x02, "sub", sub >;
def MULrrrz    : ArithmeticRRRZ< 0x03, "mul", mul >;
def DIVrrrz    : ArithmeticRRRZ< 0x04, "div", udiv >;
def REMrrzr    : ArithmeticRRZR< 0x04, "div", urem >;
// TODO: needs logic in isel lowering to be generated
def DIVREMrrrr : ArithmeticRRRR< 0x04, "div", udivrem >;


//===----------------------------------------------------------------------===//
// Operations with memory and registers
//===----------------------------------------------------------------------===//

// MOV is ADD rSrc, r0, rDst
def MOV        : ArithmeticRZR< 0x01, "add", add >;

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
// TODO: opcode is not in the spec
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : SyncVMInstruction<0x42, (outs), (ins), "ret", [(SyncVMret)]>;

let isCall = 1,
    Defs = [R1, R2, R3, R4, R5, R6],
    Uses = [SP] in {
  def CALL : Call<0x43, (outs), (ins i16imm:$callee),
                  "call\t$callee", [(SyncVMcall tglobaladdr:$callee)]>;
}


//===----------------------------------------------------------------------===//
// The LOAD constant immediate instruction
//===----------------------------------------------------------------------===//

def LOAD : Immediate< 0x05, "cnst" >;



////===----------------------------------------------------------------------===//
//// The JUMP instruction
////===----------------------------------------------------------------------===//
//
//def JUMP : Jump<
//  0x06,
//  (outs),
//  (ins GR256:$src),
//  "jump",
//  []
//>;
//
//
//
////===----------------------------------------------------------------------===//
//// The CALL instruction
////===----------------------------------------------------------------------===//
//
//def CALL : Call<
//  0x07,
//  (outs),
//  (ins GR256:$src),
//  "call",
//  []
//>;
//
//
//
////===----------------------------------------------------------------------===//
//// The SWITCH context instruction
////===----------------------------------------------------------------------===//
//
//def SWITCH : Empty<
//  0x08,
//  (outs),
//  (ins),
//  "swch",
//  []
//>;
//
//
//
////===----------------------------------------------------------------------===//
//// The MEMORY load or store instruction
////===----------------------------------------------------------------------===//
//
//def MEMORY : Memory<
//  0x09,
//  (outs),
//  (ins GR256:$src1, GR256:$src2),
//  "mmio",
//  []
//>;
//
//
//
////===----------------------------------------------------------------------===//
//// The STORAGE load or store instruction
////===----------------------------------------------------------------------===//
//
//def STORAGE : Storage<
//  0x0A,
//  (outs GR256:$dst),
//  (ins GR256:$src1, GR256:$src2),
//  "strg",
//  []
//>;
//
//
//
////===----------------------------------------------------------------------===//
//// The HABS hash absorb instruction
////===----------------------------------------------------------------------===//
//
//def HABS : Storage<
//  0x0B,
//  (outs),
//  (ins GR256:$src),
//  "habs",
//  []
//>;
//
//
//
////===----------------------------------------------------------------------===//
//// The HOUT hash output instruction
////===----------------------------------------------------------------------===//
//
//def HOUT : Storage<
//  0x0C,
//  (outs GR256:$dst),
//  (ins),
//  "hout",
//  []
//>;
