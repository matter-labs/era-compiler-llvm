//===-- SyncVMInstrInfo.td - SyncVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the SyncVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "SyncVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// SyncVM Specific Node Definitions.
//===----------------------------------------------------------------------===//
def SyncVMret : SDNode<"SyncVMISD::RET", SDTNone,
                       [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;


//===----------------------------------------------------------------------===//
// The NoOperation instruction
//===----------------------------------------------------------------------===//

def NOOP : Empty< 0x00, "noop", [] >;

//===----------------------------------------------------------------------===//
// Arithmetic
//===----------------------------------------------------------------------===//

def ADDrrr     : ArithmeticTwoSourcesOneDestination< 0x01, "add", add >;
def SUBrrr     : ArithmeticTwoSourcesOneDestination< 0x02, "sub", sub >;
def MULrzrr    : ArithmeticRZRR< 0x03, "mul", mul >;
// div/rem instruction
// TODO: results in decoding conflict, consider other approach to generate div
def DIVrrr     : ArithmeticRZRR< 0x04, "div", udiv >;
def REMrrr     : ArithmeticZRRR< 0x04, "div", urem >;
// TODO: needs logic in isel lowering to be generated
def DIVREMrrrr : ArithmeticTwoSourcesTwoDestinations< 0x04, "div", udivrem >;

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
// TODO: opcode is not in the spec
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : SyncVMInstruction<0x42, (outs), (ins), "ret", [(SyncVMret)]>;

////===----------------------------------------------------------------------===//
//// The LOAD constant immediate instruction
////===----------------------------------------------------------------------===//
//
//def LOAD : Immediate<
//  0x05,
//  (outs GR256:$dst),
//  (ins),
//  "load",
//  []
//>;
//
//
//
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
