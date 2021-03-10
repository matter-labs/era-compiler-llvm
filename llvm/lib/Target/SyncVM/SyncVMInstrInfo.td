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

let isCall = 1,
    Defs = [R1, R2, R3, R4, R5, R6],
    Uses = [SP] in {
  def CALL : Call<0x43, (outs), (ins i16imm:$callee),
                  "call\t$callee", [(SyncVMcall tglobaladdr:$callee)]>;
}
