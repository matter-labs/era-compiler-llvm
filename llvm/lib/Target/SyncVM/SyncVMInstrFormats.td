//===-- SyncVMInstrFormats.td - SyncVM Instruction Formats -*- tablegen -*-===//
//
// This file describes the SyncVM instruction formats in TableGen format.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Classes and defs used for relation maps.
//===----------------------------------------------------------------------===//

class PredRel;
class PseudoRel; // map: `_p` -> `_s`
class FlagRel; // map: `_s` -> `_v`
class AddrModeRel; // generic map of addressing mode

// The addressing mode can be categorized into operand addressing and destination addressing
// OperandAddrMode:
// RR : Reg Reg; SR : Stack Reg; CR : Code Reg; IR : Imm Reg;
class OperandAddrModeValue<bits<3> val> {
  bits<3> Value = val;
}
def OpndAddrNotSet : OperandAddrModeValue<0>;
def OpndRR         : OperandAddrModeValue<1>;
def OpndIR         : OperandAddrModeValue<2>;
def OpndSR         : OperandAddrModeValue<3>;
def OpndCR         : OperandAddrModeValue<4>;

// DestAddrMode:
class DestAddrModeValue<bits<3> val> {
  bits<3> Value = val;
}
def DestAddrNotSet : DestAddrModeValue<0>;
def ToReg          : DestAddrModeValue<1>;
def ToStack        : DestAddrModeValue<2>;
def ToRegReg       : DestAddrModeValue<3>;
def ToStackReg     : DestAddrModeValue<4>;

// FlagSetting:
// true (1) : `_v` instructions ; false (0) : `_s` instructions

//===----------------------------------------------------------------------===//
// Generate mapping table to relate non-flag setting instruction (`_s`) to flag-setting
// instruction (`_v`)
//

def getPseudoMapOpcode : InstrMapping {
  let FilterClass = "PseudoRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "ReverseOperand", "isCommutable"];
  let ColFields = ["IsPseudoArith"];
  let KeyCol = ["1"];
  let ValueCols = [["0"]];
}

def getFlagSettingOpcode : InstrMapping {
  let FilterClass = "FlagRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "ReverseOperand"];
  let ColFields = ["FlagSetting"];
  let KeyCol = ["1"];
  let ValueCols = [["0"]];
}

def getNonFlagSettingOpcode : InstrMapping {
  let FilterClass = "PredRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "ReverseOperand"];
  let ColFields = ["FlagSetting"];
  let KeyCol = ["0"];
  let ValueCols = [["1"]];
}

//===----------------------------------------------------------------------===//
// SyncVM Instructions
//===----------------------------------------------------------------------===//


class SourceMode<bits<2> val> {
  bits<2> Value = val;
}

def SrcNone     : SourceMode<0>; // r0
def SrcReg      : SourceMode<0>; // r
def SrcCode     : SourceMode<1>; // m
def SrcStack    : SourceMode<2>; // m
def SrcImm      : SourceMode<3>; // i

class DestMode<bit val> {
  bit Value = val;
}

def DstNone     : DestMode<0>;   // r0
def DstReg      : DestMode<0>;   // r
def DstStack    : DestMode<1>;   // m

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
  SourceMode as,
  DestMode ad,
  dag outs,
  dag ins,
  string asmstr,
  list<dag> pattern
> : SyncVMInstruction<outs, ins, 9, asmstr, pattern>
{
  let Inst{37}    = ad.Value;
  let Inst{38-39} = as.Value;
  let Inst{40-43} = 0; /* dst1 */
  let Inst{48-51} = 0; /* src1 */
  let Inst{56-63} = opcode;
}

// FIXME: SP change is currently only allowed in NOPs.
// It should be fixed in accordance with the spec.
class INop<bits<8> opcode, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, asmstr, pattern > {
  bits<16> sp_change;
  let Inst{44-47} = 0;
  let Inst{52-55} = 0;
  let Inst{ 0-15} = sp_change;
}

class IRet<bits<8> opcode, RetFlags flags, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, asmstr, pattern > {
  let Inst{32-35} = flags.Value;
  let Inst{44-47} = 0;
  let Inst{52-55} = 0;
}

class IConditional<bits<8> opcode,
                   SourceMode as,
                   DestMode ad,
                   dag outs,
                   dag ins,
                   string asmstr,
                   list<dag> pattern>
  : IForm <opcode, as, ad, outs, ins, asmstr, pattern > {
  bits<3> cc;
  let Inst{33-35} = cc;
}

class ICondJump<bits<8> opcode, dag ins, string asmstr, list<dag> pattern>
  : IConditional <opcode, SrcNone, DstNone, (outs), ins, asmstr, pattern> {
  bits<16> dest;
  let Inst{16-31} = dest;
}

class IJump<bits<8> opcode, dag ins, string asmstr, list<dag> pattern>
  : IConditional <opcode, SrcNone, DstNone, (outs), ins, asmstr, pattern> {
  bits<16> dest;
  let Inst{16-31} = dest;
  let cc = 0;
}

class IUMA<bits<8> opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IConditional <opcode, SrcNone, DstNone, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;
  bits<4> rd0;

  let Inst{40-43} = rd0;
  let Inst{48-51} = rs1;
  let Inst{52-55} = rs0;
}

class IContextR<bits<8> opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IConditional <opcode, SrcNone, DstReg, outs, ins, asmstr, pattern> {
  bits<4> cop;
  bits<4> rd0;
  let Inst{40-43} = rd0;
  let Inst{52-55} = cop;
}

class IContextS<bits<8> opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IConditional <opcode, SrcNone, DstStack, outs, ins, asmstr, pattern> {
  bits<4> cop;
  bits<20> dst0;
  let Inst{ 0-15} = dst0{4-19};
  let Inst{44-47} = dst0{0-3};
  let Inst{52-55} = cop;
}

class ILogR<bits<8> opcode, DestMode ad, dag outs, dag ins,
            string asmstr, list<dag> pattern>
  : IConditional <opcode, SrcReg, ad, outs, ins, asmstr, pattern> {
  bit first;
  bits<4> rs0;
  bits<4> rs1;
  let Inst{36} = first;
  let Inst{48-51} = rs1;
  let Inst{52-55} = rs0;
}

class IBinary<bits<8> opcode,
              SourceMode as,
              DestMode ad,
              bit swap_operands,
              bit silent,
              dag outs,
              dag ins,
              string asmstr,
              list<dag> pattern>
  : IConditional <opcode, as, ad, outs, ins, asmstr, pattern >, PredRel {
  bits<4> rs1;
  string BaseOpcode;
  bit FlagSetting = silent;
  bit ReverseOperand = swap_operands;
  bit IsPseudoArith = 0;
  bits<3> OperandAddrMode = OpndAddrNotSet.Value;
  bits<3> DestAddrMode = DestAddrNotSet.Value;
  let Inst{32} = swap_operands;
  let Inst{36} = silent;
  let Inst{48-51} = rs1;
}

class IBinaryR<bits<8> opcode,
               SourceMode as,
               bit swap_operands,
               bit silent,
               dag outs,
               dag ins,
               string asmstr,
               list<dag> pattern>
  : IBinary <opcode, as, DstReg, swap_operands, silent, outs, ins, asmstr, pattern > {
  bits<4> rd0;
  let DestAddrMode = ToReg.Value;
  let Inst{40-43} = rd0;
}

class IBinaryS<bits<8> opcode,
               SourceMode as,
               bit swap_operands,
               bit silent,
               dag outs,
               dag ins,
               string asmstr,
               list<dag> pattern>
  : IBinary <opcode, as, DstStack, swap_operands, silent, outs, ins, asmstr, pattern > {
  bits<20> dst0;
  let DestAddrMode = ToStack.Value;
  let Inst{44-47} = dst0{0-3};
  let Inst{ 0-15} = dst0{4-19};
}

class Irrr<bits<8> opcode,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, SrcReg, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  let OperandAddrMode = OpndRR.Value;
  let Inst{52-55} = rs0;
}

class Irrrr<bits<8> opcode,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Irrr<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToRegReg.Value;
  let Inst{40-43} = rd1;
}

class Iirr<bits<8> opcode,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, SrcImm, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<16> imm;
  let OperandAddrMode = OpndIR.Value;
  let Inst{52-55} = 0;
  let Inst{16-31} = imm;
}

class Iirrr<bits<8> opcode,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Iirr<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToRegReg.Value;
  let Inst{40-43} = rd1;
}

class Imrr<bits<8> opcode,
           SourceMode as,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0; // reg + imm
  let OperandAddrMode = OpndCR.Value;
  let Inst{52-55} = src0{0-3};
  let Inst{16-31} = src0{4-19};
}

class Isrr<bits<8> opcode,
           SourceMode as,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0; // reg + imm
  let OperandAddrMode = OpndSR.Value;
  let Inst{52-55} = src0{0-3};
  let Inst{16-31} = src0{4-19};
}

class Imrrr<bits<8> opcode,
            SourceMode as,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Imrr<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToRegReg.Value;
  let OperandAddrMode = OpndCR.Value;
  let Inst{40-43} = rd1;
}

class Isrrr<bits<8> opcode,
            SourceMode as,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Isrr<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToRegReg.Value;
  let Inst{40-43} = rd1;
}

class Irrs<bits<8> opcode,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, SrcReg, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4>  rs0;
  let OperandAddrMode = OpndRR.Value;
  let Inst{52-55} = rs0;
}

class Irrsr<bits<8> opcode,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Irrs<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToStackReg.Value;
  let Inst{40-43} = rd1;
}

class Iirs<bits<8> opcode,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, SrcImm, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<16> imm;
  let OperandAddrMode = OpndIR.Value;
  let Inst{52-55} = 0;
  let Inst{16-31} = imm;
}

class Iirsr<bits<8> opcode,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Iirs<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToStackReg.Value;
  let Inst{40-43} = rd1;
}

class Imrs<bits<8> opcode,
           SourceMode as,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0; // reg + imm
  let OperandAddrMode = OpndCR.Value;
  let Inst{52-55} = src0{0-3};
  let Inst{16-31} = src0{4-19};
}

class Isrs<bits<8> opcode,
           SourceMode as,
           bit swap_operands,
           bit silent,
           dag outs,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0; // reg + imm
  let OperandAddrMode = OpndSR.Value;
  let Inst{52-55} = src0{0-3};
  let Inst{16-31} = src0{4-19};
}

class Imrsr<bits<8> opcode,
            SourceMode as,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Imrs<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToStackReg.Value;
  let Inst{40-43} = rd1;
}

class Isrsr<bits<8> opcode,
            SourceMode as,
            bit swap_operands,
            bit silent,
            dag outs,
            dag ins,
            string asmstr,
            list<dag> pattern>
  : Isrs<opcode, as, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;
  let DestAddrMode = ToStackReg.Value;
  let Inst{40-43} = rd1;
}

// Pseudo instructions
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
  : SyncVMInstruction <outs, ins, 0, asmstr, pattern>, PseudoRel {
  string BaseOpcode;
  bit FlagSetting = 0;
  bit IsPseudoArith = 1;
  bit ReverseOperand = 0;
  bits<3> OperandAddrMode = OpndAddrNotSet.Value;
  bits<3> DestAddrMode = DestAddrNotSet.Value;
}

class CallFlags<bits<4> val> {
  bits<4> Value = val;
}
def CFNormal         : CallFlags<0b0000>;
def CFDelegate       : CallFlags<0b0001>;
def CFMimic          : CallFlags<0b0010>;

class ICall<bits<8> opcode, CallFlags flags, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, asmstr, pattern > {
  let Inst{32-35} = flags.Value;
  let Inst{44-47} = 0;
  let Inst{52-55} = 0;
}
