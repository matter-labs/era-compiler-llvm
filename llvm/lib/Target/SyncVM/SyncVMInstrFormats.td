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

  let Inst{255-248}     = opcode;
}



//===----------------------------------------------------------------------===//
// An empty operation
//===----------------------------------------------------------------------===//

class Empty<
  bits<8> opcode,
  string asmstr,
  list<dag> pattern
> : SyncVMInstruction<
  opcode,
  (outs),
  (ins),
  asmstr,
  pattern
> {}



//===----------------------------------------------------------------------===//
// Binary operation with two source registers and one destination
//===----------------------------------------------------------------------===//

class ArithmeticTwoSourcesOneDestination<
  bits<8> opcode,
  string name,
  SDNode node
> : SyncVMInstruction<
  opcode,
  (outs GR256:$dst),
  (ins GR256:$src1, GR256:$src2),
  !strconcat(name, "\t$dst, $src1, $src2"),
  [(set i256:$dst, (node i256:$src1, i256:$src2))]
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

class ArithmeticTwoSourcesTwoDestinations<
  bits<8> opcode,
  string name,
  SDNode node
> : SyncVMInstruction<
  opcode,
  (outs GR256:$dst1, GR256:$dst2),
  (ins GR256:$src1, GR256:$src2),
  !strconcat(name, "\t$dst1, $dst2, $src1, $src2"),
  [(set i256:$dst1, i256:$dst2, (node i256:$src1, i256:$src2))]
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

// Two destination instructions that dispose on of its results
class ArithmeticRZRR <
  bits<8> opcode,
  string name,
  SDNode node
> : SyncVMInstruction <
  opcode,
  (outs GR256:$dst),
  (ins GR256:$src1, GR256:$src2),
  !strconcat(name, "\t$dst, r0, $src1, $src2"),
  [(set i256:$dst, (node i256:$src1, i256:$src2))]
> {
  bits<4> src1;
  bits<4> src2;
  bits<4> dst;

  let Inst{247-244} = src1;
  let Inst{243-240} = src2;
  let Inst{239-236} = dst;
  let Inst{235-232} = 0;
}

class ArithmeticZRRR <
  bits<8> opcode,
  string name,
  SDNode node
> : SyncVMInstruction <
  opcode,
  (outs GR256:$dst),
  (ins GR256:$src1, GR256:$src2),
  !strconcat(name, "\tr0, $dst, $src1, $src2"),
  [(set i256:$dst, (node i256:$src1, i256:$src2))]
> {
  bits<4> src1;
  bits<4> src2;
  bits<4> dst;

  let Inst{247-244} = src1;
  let Inst{243-240} = src2;
  let Inst{239-236} = 0;
  let Inst{235-232} = dst;
}


//===----------------------------------------------------------------------===//
// Immediate loading operation with one destination
//===----------------------------------------------------------------------===//

class Immediate<
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
  bits<4>   dst;
  bits<128> val;
  bit       is_high;

  let Inst{247-244} = dst;
  let Inst{243-116} = val;
  let Inst{115}     = is_high;
}



//===----------------------------------------------------------------------===//
// Jump operation with one source
//===----------------------------------------------------------------------===//

class Jump<
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
  bits<4>   src;
  bit       unconditional;
  bit       less;
  bit       equals;
  bit       greater;
  bit       src_zero;
  bits<3>   flags_space;
  bits<16>  if_dst;
  bits<16>  else_dst;

  let Inst{247-244} = src;
  let Inst{243}     = unconditional;
  let Inst{242}     = less;
  let Inst{241}     = equals;
  let Inst{240}     = greater;
  let Inst{239}     = src_zero;
  let Inst{238-236} = flags_space;
  let Inst{235-220} = if_dst;
  let Inst{219-204} = else_dst;
}



//===----------------------------------------------------------------------===//
// Call/return operation with one source
//===----------------------------------------------------------------------===//

class Call<
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
  bits<4>   src;
  bit       call_or_return;
  bit       external_or_local;
  bit       with_error;
  bits<1>   flags_space;

  let Inst{247-244} = src;
  let Inst{243}     = call_or_return;
  let Inst{242}     = external_or_local;
  let Inst{241}     = with_error;
  let Inst{240-240} = flags_space;
}



//===----------------------------------------------------------------------===//
// Memory operation with two sources
//===----------------------------------------------------------------------===//

class Memory<
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
  bits<4>   src1;
  bits<4>   src2;
  bits<4>   dst;
  bit       read_or_write;
  bit       force_stack;
  bits<2>   memory_type;
  bits<16>  offset;

  let Inst{247-244} = src1;
  let Inst{243-240} = src2;
  let Inst{239}     = read_or_write;
  let Inst{238}     = force_stack;
  let Inst{237-236} = memory_type;
  let Inst{235-220} = offset;
}



//===----------------------------------------------------------------------===//
// Storage operation with two sources and one destination
//===----------------------------------------------------------------------===//

class Storage<
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
  bits<4>   src1;
  bits<4>   src2;
  bits<4>   dst;
  bit       read_or_write;
  bits<3>   flags_space;

  let Inst{247-244} = src1;
  let Inst{243-240} = src2;
  let Inst{239-236} = dst;
  let Inst{235}     = read_or_write;
  let Inst{234-232} = flags_space;
}



//===----------------------------------------------------------------------===//
// Hash absorb operation with one source
//===----------------------------------------------------------------------===//

class HashAbsorb<
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
  bits<4>   src;
  bit       reset;
  bits<3>   flags_space;

  let Inst{247-244} = src;
  let Inst{243}     = reset;
  let Inst{242-240} = flags_space;
}



//===----------------------------------------------------------------------===//
// Hash output operation with one destination
//===----------------------------------------------------------------------===//

class HashOutput<
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
  bits<4>   dst;

  let Inst{247-244} = dst;
}
