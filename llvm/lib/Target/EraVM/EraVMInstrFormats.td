//===-- EraVMInstrFormats.td - EraVM Instruction Formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the EraVM instruction formats in TableGen format.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Classes and defs used for relation maps.
//===----------------------------------------------------------------------===//

class PredRel;
class FlagRel; // map: `_s` -> `_v`
class AddrModeRel; // generic map of addressing mode of operands
class RetAddrModeRel; // generic map of addressing mode of results
class SwapRel; // map in0, in1 swapping operand instruction <-> non swapping one

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
// Instruction mapping tables

// Generate mapping table to relate non-flag setting instruction (`_s`) to flag-setting
// instruction (`_v`)
//
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

def mapRRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "FlagSetting"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["0"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def mapIRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "FlagSetting"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["1"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def mapCRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "FlagSetting"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["2"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def mapSRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "FlagSetting"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["3"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def withStackResult : InstrMapping {
  let FilterClass = "RetAddrModeRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "FlagSetting", "ReverseOperand"];
  let ColFields = ["ResultAM"];
  let KeyCol = ["0"];
  let ValueCols = [["3"]];
}

def withRegisterResult : InstrMapping {
  let FilterClass = "RetAddrModeRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "FlagSetting", "ReverseOperand"];
  let ColFields = ["ResultAM"];
  let KeyCol = ["3"];
  let ValueCols = [["0"]];
}

def withInsSwapped : InstrMapping {
  let FilterClass = "PredRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "FlagSetting"];
  let ColFields = ["ReverseOperand"];
  let KeyCol = ["0"];
  let ValueCols = [["1"]];
}

def withInsNotSwapped : InstrMapping {
  let FilterClass = "PredRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "FlagSetting"];
  let ColFields = ["ReverseOperand"];
  let KeyCol = ["1"];
  let ValueCols = [["0"]];
}

//===----------------------------------------------------------------------===//
// EraVM Instructions
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

// Generic EraVM instruction
class EraVMInstruction : Instruction {
  let Namespace         = "EraVM";
  let DecoderNamespace  = "EraVM";
  let Pattern   = [];
}

// Pseudo instructions (do not have encoding information)
class Pseudo<dag outs, dag ins, list<dag> pattern>
  : EraVMInstruction {
  dag InOperandList  = ins;
  dag OutOperandList = outs;
  let Pattern        = pattern;
  let Size           = 8;

  let isCodeGenOnly = 1;
  let isPseudo = 1;
}

class Encoding {
  field bits<64> Inst;
  field bits<64> SoftFail = 0;
}

let OperandType = "OPERAND_IMMEDIATE" in
def i256imm : Operand<i256>;

// Operand for printing out a condition code.
// TODO: We really do not need here full i256
def pred : PredicateOperand<i256, (ops i256imm), (ops (i256 0))> {
  let PrintMethod = "printCCOperand";
}

// Real instructions (have encoding)
class IForm <bits<8> opcode,
             SourceMode as, DestMode ad,
             dag oops, dag inops,
             string opc, string asmstr,
             list<dag> pattern> : EraVMInstruction, Encoding
{
  dag InOperandList  = !con(inops, (ins pred:$cc));
  dag OutOperandList = oops;
  let AsmString      = !strconcat(opc, "${cc}", "\t", asmstr);
  let Pattern        = pattern;
  let Size           = 8;
}

class IRet<bits<8> opcode, RetFlags flags, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, opc, asmstr, pattern > {
}

// TODO: CPR-1360 SP change is currently only allowed in NOPs.
// It should be fixed in accordance with the spec.
class INop<bits<8> opcode, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, opc, asmstr, pattern > {
}

class ICondJump<bits<8> opcode, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, opc, asmstr, pattern> {
}

class IUMA<bits<8> opcode, dag outs, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, outs, ins, opc, asmstr, pattern> {
}

class IContextR<bits<8> opcode, dag outs, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstReg, outs, ins, opc, asmstr, pattern> {
}

class IContextS<bits<8> opcode, dag outs, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstStack, outs, ins, opc, asmstr, pattern> {
}

class ILogR<bits<8> opcode, DestMode ad, dag outs, dag ins,
            string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcReg, ad, outs, ins, opc, asmstr, pattern> {
}

class IBinary<bits<8> opcode,
              SourceMode as, DestMode ad,
              bit swap_operands, bit silent,
              dag outs, dag ins,
              string opc, string asmstr,
              list<dag> pattern>
  : IForm <opcode, as, ad, outs, ins, opc, asmstr, pattern >, PredRel {
  let AsmString = !strconcat(opc,
                             !if(swap_operands, ".s", ""),
                             !if(silent, "", "!"),
                             "${cc}", "\t", asmstr);

  string BaseOpcode;
  bit FlagSetting = silent;
  bit ReverseOperand = swap_operands;
  bits<3> OperandAddrMode = OpndAddrNotSet.Value;
  bits<3> DestAddrMode = DestAddrNotSet.Value;
  int OperandAM = 0;
  int ResultAM = 0;
}

class IBinaryR<bits<8> opcode,
               SourceMode as,
               bit swap_operands, bit silent,
               dag outs, dag ins,
               string opc, string asmstr,
               list<dag> pattern>
  : IBinary <opcode, as, DstReg, swap_operands, silent, outs, ins, opc, asmstr, pattern > {
  let DestAddrMode = ToReg.Value;
  let ResultAM = 0;
}

class IBinaryS<bits<8> opcode,
               SourceMode as,
               bit swap_operands, bit silent,
               dag outs, dag ins,
               string opc, string asmstr,
               list<dag> pattern>
  : IBinary <opcode, as, DstStack, swap_operands, silent, outs, ins, opc, asmstr, pattern > {
  let DestAddrMode = ToStack.Value;
  let ResultAM = 3;
}

class Irrr<bits<8> opcode,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, SrcReg, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndRR.Value;
  let OperandAM = 0;
}

class Irrrr<bits<8> opcode,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Irrr<opcode, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToRegReg.Value;
  let OperandAM = 0;
}

class Iirr<bits<8> opcode,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, SrcImm, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndIR.Value;
  let OperandAM = 1;
}

class Iirrr<bits<8> opcode,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Iirr<opcode, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToRegReg.Value;
  let OperandAM = 1;
}

class Imrr<bits<8> opcode,
           SourceMode as,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndCR.Value;
  let OperandAM = 2;
}

class Isrr<bits<8> opcode,
           SourceMode as,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryR<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndSR.Value;
  let OperandAM = 3;
}

class Imrrr<bits<8> opcode,
            SourceMode as,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Imrr<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToRegReg.Value;
  let OperandAddrMode = OpndCR.Value;
  let OperandAM = 2;
}

class Isrrr<bits<8> opcode,
            SourceMode as,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Isrr<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToRegReg.Value;
  let OperandAM = 3;
}

class Irrs<bits<8> opcode,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, SrcReg, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndRR.Value;
  let OperandAM = 0;
}

class Irrsr<bits<8> opcode,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Irrs<opcode, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 0;
}

class Iirs<bits<8> opcode,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, SrcImm, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndIR.Value;
  let OperandAM = 1;
}

class Iirsr<bits<8> opcode,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Iirs<opcode, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 1;
}

class Imrs<bits<8> opcode,
           SourceMode as,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndCR.Value;
  let OperandAM = 2;
}

class Isrs<bits<8> opcode,
           SourceMode as,
           bit swap_operands, bit silent,
           dag outs, dag ins,
           string opc, string asmstr,
           list<dag> pattern>
  : IBinaryS<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let OperandAddrMode = OpndSR.Value;
  let OperandAM = 3;
}

class Imrsr<bits<8> opcode,
            SourceMode as,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Imrs<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 2;
}

class Isrsr<bits<8> opcode,
            SourceMode as,
            bit swap_operands, bit silent,
            dag outs, dag ins,
            string opc, string asmstr,
            list<dag> pattern>
  : Isrs<opcode, as, swap_operands, silent, outs, ins, opc, asmstr, pattern> {
  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 3;
}

class CallFlags<bits<4> val> {
  bits<4> Value = val;
}
def CFNormal         : CallFlags<0b0000>;
def CFDelegate       : CallFlags<0b0001>;
def CFMimic          : CallFlags<0b0010>;

class ICall<bits<8> opcode, CallFlags flags, dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, SrcNone, DstNone, (outs), ins, opc, asmstr, pattern > {
}
