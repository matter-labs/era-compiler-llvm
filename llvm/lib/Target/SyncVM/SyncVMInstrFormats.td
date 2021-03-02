//===-- SyncVMInstrFormats.td - SyncVM Instruction Formats -*- tablegen -*-===//
//
// This file describes the SyncVM instruction formats in TableGen format.
//
//===----------------------------------------------------------------------===//



//===----------------------------------------------------------------------===//
// Generic SyncVM instruction
//===----------------------------------------------------------------------===//

class SyncVMInstruction<
  bits<8> opcode,
  dag outs,
  dag ins,
  string asmstr,
  list<dag> pattern
> : Instruction
{
  field bits<256> Inst;
  field bits<256> SoftFail = 0;

  let Namespace         = "SyncVM";
  let AsmString         = asmstr;
  let Size              = 32;
  let OutOperandList    = outs;
  let InOperandList     = ins;
  let Pattern           = pattern;

  let Inst{255-248} = opcode;
}



//===----------------------------------------------------------------------===//
// An empty operation
//===----------------------------------------------------------------------===//

class Empty<
  bits<8> opcode,
  dag outs,
  dag ins,
  string asmstr,
  list<dag> pattern
> : SyncVMInstruction<
  opcode,
  outs,
  ins,
  asmstr,
  pattern
> {}



//===----------------------------------------------------------------------===//
// Binary operation with two source registers and one destination
//===----------------------------------------------------------------------===//

class ArithmeticTwoRegistersOneDestination<
  bits<8> opcode,
  dag outs,
  dag ins,
  string asmstr,
  list<dag> pattern
> : SyncVMInstruction<
  opcode,
  outs,
  ins,
  asmstr,
  pattern
> {
  bits<4> src1;
  bits<4> src2;
  bits<4> dst;

  let Inst{247-244} = src1;
  let Inst{243-240} = src2;
  let Inst{239-236} = dst;
}



//===----------------------------------------------------------------------===//
// Binary operation with two source registers and two destinations
//===----------------------------------------------------------------------===//

class ArithmeticTwoRegistersTwoDestinations<
  bits<8> opcode,
  dag outs,
  dag ins,
  string asmstr,
  list<dag> pattern
> : SyncVMInstruction<
  opcode,
  outs,
  ins,
  asmstr,
  pattern
> {
  bits<4> src1;
  bits<4> src2;
  bits<4> dst1;
  bits<4> dst2;

  let Inst{247-244} = src1;
  let Inst{243-240} = src2;
  let Inst{239-236} = dst1;
  let Inst{235-232} = dst2;
}
