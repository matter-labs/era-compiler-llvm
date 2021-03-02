//===-- SyncVMInstrInfo.td - SyncVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the SyncVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "SyncVMInstrFormats.td"



//===----------------------------------------------------------------------===//
// The NoOperation instruction
//===----------------------------------------------------------------------===//

def NOOP : Empty<
  0x00,
  (outs),
  (ins),
  "noop",
  []
>;



//===----------------------------------------------------------------------===//
// The ADD arithmetic instruction
//===----------------------------------------------------------------------===//

def ADD : ArithmeticTwoRegistersOneDestination<
  0x01,
  (outs GR256:$dst),
  (ins GR256:$src1, GR256:$src2),
  "add",
  []
>;



//===----------------------------------------------------------------------===//
// The SUB arithmetic instruction
//===----------------------------------------------------------------------===//

def SUB : ArithmeticTwoRegistersOneDestination<
  0x02,
  (outs GR256:$dst),
  (ins GR256:$src1, GR256:$src2),
  "sub",
  []
>;



//===----------------------------------------------------------------------===//
// The MUL arithmetic instruction
//===----------------------------------------------------------------------===//

def MUL : ArithmeticTwoRegistersTwoDestinations<
  0x03,
  (outs GR256:$dst1, GR256:$dst2),
  (ins GR256:$src1, GR256:$src2),
  "mul",
  []
>;



//===----------------------------------------------------------------------===//
// The DIV arithmetic instruction
//===----------------------------------------------------------------------===//

def DIV : ArithmeticTwoRegistersTwoDestinations<
  0x04,
  (outs GR256:$dst1, GR256:$dst2),
  (ins GR256:$src1, GR256:$src2),
  "div",
  []
>;
