//===-- SyncVMInstrFormats.td - SyncVM Instruction Formats -*- tablegen -*-===//
//
// This file describes the SyncVM instruction formats in TableGen format.
//
//===----------------------------------------------------------------------===//

class SourceMode<bits<4> val> {
  bits<4> Value = val;
}

def SrcNone     : SourceMode<0b0000>; // r0
def SrcReg      : SourceMode<0b0001>; // r
def SrcCode     : SourceMode<0b0010>; // m
def SrcStack    : SourceMode<0b0100>; // m
def SrcImm      : SourceMode<0b1000>; // i

class DestMode<bits<4> val> {
  bits<4> Value = val;
}

def DstNone     : DestMode<0b0000>;   // r0
def DstReg      : DestMode<0b0001>;   // r
def DstStack    : DestMode<0b0100>;   // m

class RetFlags<bits<4> val> {
  bits<4> Value = val;
}

def RFNone           : RetFlags<0b0000>;
def RFErr            : RetFlags<0b0001>;
def RFPanic          : RetFlags<0b0010>;
def RFTransitRetData : RetFlags<0b0100>;

// Generic SyncVM instruction
class SyncVMInstruction<
  dag outs,
  dag ins,
  int size,
  string asmstr,
  list<dag> pattern
> : Instruction
{
  field bits<72>        Inst;
  field bits<72>        SoftFail = 0;

  let Namespace         = "SyncVM";
  let DecoderNamespace  = "SyncVM";
  let AsmString         = asmstr;
  let Size              = size;
  let OutOperandList    = outs;
  let InOperandList     = ins;
  let Pattern           = pattern;
}

class IForm <
  bits<8>  opcode,
/*
  bits<4>  reg_source1,
  bits<4>  reg_source2,
  bits<4>  reg_destination1,
  bits<4>  reg_destination2,
  bits<4>  mem_imm_handler_flags_src1,
  bits<4>  mem_imm_handler_flags_dst1,
  bits<4>  non_selector_flags,
  bits<4>  table_based_non_selector_flags,
  bits<16> const_src,
  bits<16> const_dst,
*/
  SourceMode as,
  DestMode ad,
  bit swap_operands,
  dag outs,
  dag ins,
  string asmstr,
  list<dag> pattern
> : SyncVMInstruction<outs, ins, 9, asmstr, pattern>
{
  let Inst{64-71}       = opcode;
  let Inst{44-47}       = as.Value;
  let Inst{40-43}       = ad.Value;
  let Inst{32}          = swap_operands;
  /*
  let Inst{60-63}       = reg_source1;
  let Inst{56-59}       = reg_source2;
  let Inst{52-55}       = reg_destination1;
  let Inst{48-51}       = reg_destination2;
  let Inst{44-47}       = mem_imm_handler_flags_src1;
  let Inst{40-43}       = mem_imm_handler_flags_dst1;
  let Inst{36-39}       = non_selector_flags;
  let Inst{32-35}       = table_based_non_selector_flags;
  let Inst{16-31}       = const_src;
  let Inst{0-15}        = const_dst;
  */
}

// Nop instruction
class INop<bits<8> opcode, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, 0, (outs), (ins), asmstr, pattern > { }

class IConditional<bits<8> opcode,
                   dag outs,
                   dag ins,
                   string asmstr,
                   list<dag> pattern>
  : IForm <opcode, SrcReg, DstNone, 0, outs, ins, asmstr, pattern > {
  bits<4> rs1;
  bits<4> flags;
  let Inst{60-63} = rs1;
  let Inst{36-39} = flags;
}

class IJump<bits<8> opcode, dag ins, string asmstr, list<dag> pattern>
  : IConditional <opcode, (outs), ins, asmstr, pattern> {
  bits<16> addrtrue;
  bits<16> addrfalse;
  let Inst{16-31} = addrtrue;
  let Inst{0-15}  = addrfalse;
}

class ICmov<bits<8> opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IConditional< opcode, outs, ins, asmstr, pattern> {
  bits<4> rs2;
  bits<4> rd;
  let Inst{56-59} = rs2;
  let Inst{52-55} = rd;
  let Inst{40-43} = DstReg.Value;
}

class IRet<bits<8> opcode, RetFlags flags, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, 0, (outs), (ins), asmstr, pattern > {
  let Inst{36-39} = flags.Value;
}

class Irrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcReg, DstReg, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rs1;
  bits<4> rs2;
  bits<4> rd;
  let Inst{60-63} = rs1;
  let Inst{56-59} = rs2;
  let Inst{52-55} = rd;
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
}

class Irrrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Irrr<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Iirr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcImm, DstReg, swap_operands, outs, ins, asmstr, pattern> {
  bits<16> imm;
  bits<4>  rs2;
  bits<4>  rd;
  let Inst{56-59} = rs2;
  let Inst{52-55} = rd;
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{16-31} = imm;
}

class Iirrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Iirr<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Icrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcCode, DstReg, swap_operands, outs, ins, asmstr, pattern> {
  bits<20> src1; // reg + imm
  bits<4>  rs2;
  bits<4>  rd;
  let Inst{60-63} = src1{0-3};
  let Inst{56-59} = rs2;
  let Inst{52-55} = rd;
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{16-31} = src1{4-19};
}

class Icrrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Icrr<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Isrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcStack, DstReg, swap_operands, outs, ins, asmstr, pattern> {
  bits<20> src1; // reg [+ sp] + imm
  bits<4>  rs2;
  bits<4>  rd;
  let Inst{60-63} = src1{0-3};
  let Inst{56-59} = rs2;
  let Inst{52-55} = rd;
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{16-31} = src1{4-19};
}

class Isrrr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Isrr<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Irrs<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcReg, DstStack, swap_operands, outs, ins, asmstr, pattern> {
  bits<4>  rs1;
  bits<4>  rs2;
  bits<24> dst;
  let Inst{60-63} = rs1;
  let Inst{56-59} = rs2;
  let Inst{52-55} = dst{0-3};
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{0-15}  = dst{4-19};
}

class Irrsr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Irrs<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Iirs<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcImm, DstStack, swap_operands, outs, ins, asmstr, pattern> {
  bits<16> imm;
  bits<4>  rs2;
  bits<20> dst;
  let Inst{56-59} = rs2;
  let Inst{52-55} = dst{0-3};
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{16-31} = imm;
  let Inst{0-15}  = dst{4-19};
}

class Iirsr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Iirs<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Icrs<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcCode, DstStack, swap_operands, outs, ins, asmstr, pattern> {
  bits<20> src1; // reg + imm
  bits<4>  rs2;
  bits<20> dst;
  let Inst{60-63} = src1{0-3};
  let Inst{56-59} = rs2;
  let Inst{52-55} = dst{0-3};
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{16-31} = src1{4-19};
  let Inst{0-15}  = dst{4-19};
}

class Icrsr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Icrs<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

class Isrs<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm<opcode, SrcStack, DstStack, swap_operands, outs, ins, asmstr, pattern> {
  bits<20> src1; // reg [+ sp] + imm
  bits<4>  rs2;
  bits<20> dst;
  let Inst{60-63} = src1{0-3};
  let Inst{56-59} = rs2;
  let Inst{52-55} = dst{0-3};
  let Inst{48-51} = 0;
  let Inst{32}    = swap_operands;
  let Inst{16-31} = src1{4-19};
  let Inst{0-15}  = dst{4-19};
}

class Isrsr<bits<8> opcode,
           bit swap_operands,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : Isrs<opcode, swap_operands, outs, ins, asmstr, pattern> {
  bits<4> rd2;
  let Inst{48-51} = rd2;
}

// Pseudo instructions
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
  : SyncVMInstruction <outs, ins, 0, asmstr, pattern> {}
