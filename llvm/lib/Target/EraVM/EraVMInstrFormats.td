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

//===----------------------------------------------------------------------===//
// Instruction mapping tables

// Flag setting instructions
// Used to map: `_s` -> `_v`
// Flag setting instructions include: and, or, xor, add, sub, shl, shr, rol, ror, mul, div
class FlagSetting;

class mod_set_flags<bit silent> {
  bit Value = silent;
}

def PreserveFlags : mod_set_flags<0>;
def SetFlags : mod_set_flags<1>;

def getFlagSettingOpcode : InstrMapping {
  let FilterClass = "FlagSetting";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "ReverseOperands"];
  let ColFields = ["Silent"];
  let KeyCol = ["1"];
  let ValueCols = [["0"]];
}

def getNonFlagSettingOpcode : InstrMapping {
  let FilterClass = "FlagSetting";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "ReverseOperands"];
  let ColFields = ["Silent"];
  let KeyCol = ["0"];
  let ValueCols = [["1"]];
}

// Instructions that support swapping arguments
// This included non-commutative arithmetic ones: sub, shl, shr, rol, ror, div and
// pointer manipulation: ptr.add, ptr.sub, ptr.shrink, ptr.sub
class Swappable;

class mod_swap<bit swap> {
  bit Value = swap;
}

def NoSwap : mod_swap<0>;
def Swap : mod_swap<1>;

def withInsSwapped : InstrMapping {
  let FilterClass = "Swappable";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "Silent"];
  let ColFields = ["ReverseOperands"];
  let KeyCol = ["0"];
  let ValueCols = [["1"]];
}

def withInsNotSwapped : InstrMapping {
  let FilterClass = "Swappable";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "Silent"];
  let ColFields = ["ReverseOperands"];
  let KeyCol = ["1"];
  let ValueCols = [["0"]];
}

class AddrModeRel; // generic map of addressing mode of operands
class RetAddrModeRel; // generic map of addressing mode of results


def mapRRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "Silent"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["0"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def mapIRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "Silent"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["1"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def mapCRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "Silent"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["2"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def mapSRInputTo : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "Silent"];
  let ColFields = ["OperandAM"];
  let KeyCol = ["3"];
  let ValueCols = [["0"], ["1"], ["2"], ["3"]];
}

def withStackResult : InstrMapping {
  let FilterClass = "RetAddrModeRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "Silent", "ReverseOperands"];
  let ColFields = ["ResultAM"];
  let KeyCol = ["0"];
  let ValueCols = [["3"]];
}

def withRegisterResult : InstrMapping {
  let FilterClass = "RetAddrModeRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "Silent", "ReverseOperands"];
  let ColFields = ["ResultAM"];
  let KeyCol = ["3"];
  let ValueCols = [["0"]];
}

include "EraVMOpcodes.td"

//===----------------------------------------------------------------------===//
// EraVM Instructions
//===----------------------------------------------------------------------===//
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
  // 10:0  mach_opcode (11 bits)
  // 12:11 2 reserved bits
  // 15:13 predicate (3 bits)
  // 31:28, 27:24, 23:20, 19:16 reg_names (4 x 4 bits)
  // 63:48, 47:32 immediate u16 values (2 x 16 bits, big endian)

  field bits<64> Inst;
  field bits<64> SoftFail = 0;

  bits<16> Imm1 = 0;
  bits<16> Imm0 = 0;
  bits<4> Dst1 = 0b0000;
  bits<4> Dst0 = 0b0000;
  bits<4> Src1 = 0b0000;
  bits<4> Src0 = 0b0000;
  bits<3> Predicate;
  bits<11> Opcode;

  let Inst{63-48} = Imm1;
  let Inst{47-32} = Imm0;
  let Inst{31-28} = Dst1;
  let Inst{27-24} = Dst0;
  let Inst{23-20} = Src1;
  let Inst{19-16} = Src0;
  let Inst{15-13} = Predicate;
  let Inst{12-11} = 0;
  let Inst{10-0}  = Opcode;
}

let OperandType = "OPERAND_IMMEDIATE" in
def i256imm : Operand<i256>;

// Operand for printing out a condition code.
// TODO: We really do not need here full i256
def pred : PredicateOperand<i256, (ops i256imm), (ops (i256 0))> {
  let PrintMethod = "printCCOperand";
  let EncoderMethod = "getCCOpValue";
}

// Pseudo instructions (do not have encoding information, only for asmparser)
class AsmParserPseudo<dag outs, dag inops, string opc, string asmstr>
  : EraVMInstruction {
  bits<3> cc;

  dag InOperandList  = !con(inops, (ins pred:$cc));
  dag OutOperandList = outs;
  let Size           = 8;
  let AsmString      = !strconcat(opc, "${cc}", "\t", asmstr);

  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
  let isPseudo = 1;
}

// Real instructions (have encoding)
class IForm <EraVMOpcode opcode,
             dag oops, dag inops,
             string opc, string asmstr,
             list<dag> pattern> : EraVMInstruction, Encoding
{
  bits<3> cc;

  dag InOperandList  = !con(inops, (ins pred:$cc));
  dag OutOperandList = oops;
  let AsmString      = !strconcat(opc, "${cc}", "\t", asmstr);
  let Pattern        = pattern;
  let Size           = 8;

  let Predicate = cc;
  // FIXME: This should be overriden in the subclass
  let Opcode = opcode.BaseOpcode;
}

// TODO: CPR-1360 SP change is currently only allowed in NOPs.
// It should be fixed in accordance with the spec.
class INop<dag ins, string opc, string asmstr, list<dag> pattern>
  : IForm <OpNoOp, (outs), ins, opc, asmstr, pattern > {
}

class IJump<EraVMOpcode opcode,
            SrcMode src,
            dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, (outs), ins, "<invalid>", asmstr, pattern> {

  // FIXME: Should should be default
  let AsmString = !strconcat(opcode.Name, "${cc}", "\t", asmstr);
  let mayLoad = !or(isStackIn<src>.Value, !eq(src, SrcCodeAddr));
  let Opcode = JumpOpcEncoder<opcode.BaseOpcode, src>.Opcode;
}

class IContext<EraVMOpcode opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, "<invalid>", asmstr, pattern> {

  // FIXME: Should should be default
  let AsmString = !strconcat(opcode.Name, "${cc}", "\t", asmstr);
}

// FIXME: This needs to be split into ILogRrr, ILogRrr_r and ILogRr_r versions
class ILogR<EraVMOpcode opcode, dag outs, dag ins,
            string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, "<invalid>", asmstr, pattern> {

  // FIXME: Should should be default
  let AsmString = !strconcat(opcode.Name, "${cc}", "\t", asmstr);
}

class ILogRFirst<EraVMOpcode opcode, bit is_first,
                 dag outs, dag ins,
                 string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, "<invalid>", asmstr, pattern> {

  // FIXME: Should should be default
  let AsmString = !strconcat(opcode.Name,
                             !if(is_first, ".first", ""),
                             "${cc}", "\t", asmstr);
  let Opcode = LogROpcEncoder<opcode.Encoding, opcode.BaseOpcode,
                              is_first>.Opcode;
}

class IBinary<EraVMOpcode opcode,
              SrcMode src, DstMode dst,
              mod_swap swap, mod_set_flags set_flags,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
  : IForm <opcode, outs, ins, "<invalid>", asmstr, pattern > {
  string BaseOpcode;
  bit Silent = !eq(set_flags, PreserveFlags);
  bit ReverseOperands = !eq(swap, Swap);
  bits<3> OperandAddrMode = OpndAddrNotSet.Value;
  bits<3> DestAddrMode = DestAddrNotSet.Value;
  int OperandAM = 0;
  int ResultAM = 0;

  let AsmString = !strconcat(opcode.Name,
                             !if(!eq(swap, Swap), ".s", ""),
                             !if(!eq(set_flags, SetFlags), "!", ""),
                             "${cc}", "\t", asmstr);
  let Defs = !if(Silent, [], [Flags]);
  let mayLoad = !or(isStackIn<src>.Value, !eq(src, SrcCodeAddr));
  let mayStore = isStackOut<dst>.Value;

  let Opcode = ArithOpcEncoder<opcode.Encoding, opcode.BaseOpcode,
                               src, dst,
                               swap, set_flags>.Opcode;
}

class IBinaryR<EraVMOpcode opcode,
               SrcMode src,
               mod_swap swap_operands, mod_set_flags silent,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
  : IBinary <opcode, src, DstReg, swap_operands, silent, outs, ins, asmstr, pattern > {
  bits<4> rd0;

  let DestAddrMode = ToReg.Value;
  let ResultAM = 0;

  let Dst0 = rd0;
}

class IBinaryS<EraVMOpcode opcode,
               SrcMode src,
               mod_swap swap, mod_set_flags set_flags,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
  // FIXME: DstStackAbsolute is incorrect here, we need to be able to properly encode relative and post-increment
  // FIXME: Does encoding via post-fixup, does disassembly manually
  : IBinary <opcode, src, DstStackAbsolute, swap, set_flags, outs, ins, asmstr, pattern > {
  // Encode stack operand into 20 bits as reg + imm offset
  bits<20> dst0;

  let DestAddrMode = ToStack.Value;
  let ResultAM = 3;

  let Dst0 = dst0{3-0};
  let Imm1 = dst0{19-4};
}

class Irr_r<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryR<opcode, SrcReg, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;

  let OperandAddrMode = OpndRR.Value;
  let OperandAM = 0;

  let Src0 = rs0;
  let Src1 = rs1;
}

class Irr_rr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Irr_r<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToRegReg.Value;
  let OperandAM = 0;

  let Dst1 = rd1;
}

class Iir_r<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryR<opcode, SrcImm, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rs1;
  bits<16> imm;

  let OperandAddrMode = OpndIR.Value;
  let OperandAM = 1;

  let Src1 = rs1;
  let Imm0 = imm;
}

class Iir_rr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Iir_r<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToRegReg.Value;
  let OperandAM = 1;

  let Dst1 = rd1;
}

class Imr_r<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryR<opcode, SrcCodeAddr, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rs1;
  // FIXME: ensure we encode address mode properly
  bits<16> src0;

  let OperandAddrMode = OpndCR.Value;
  let OperandAM = 2;

  let Src1 = rs1;
  let Imm0 = src0;
}

class Isr_r<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  // FIXME: SrcStackAbsolute is incorrect here, we need to be able to properly encode relative and post-increment
  // FIXME: Does encoding via post-fixup, does disassembly manually
  : IBinaryR<opcode, SrcStackAbsolute, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0;
  bits<4> rs1;

  let OperandAddrMode = OpndSR.Value;
  let OperandAM = 3;

  let Src0 = src0{3-0};
  let Src1 = rs1;
  let Imm0 = src0{19-4};
}

class Imr_rr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Imr_r<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToRegReg.Value;
  let OperandAddrMode = OpndCR.Value;
  let OperandAM = 2;

  let Dst1 = rd1;
}

class Isr_rr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Isr_r<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToRegReg.Value;
  let OperandAM = 3;

  let Dst1 = rd1;
}

class Irr_s<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryS<opcode, SrcReg, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;

  let OperandAddrMode = OpndRR.Value;
  let OperandAM = 0;

  let Src0 = rs0;
  let Src1 = rs1;
}

class Irr_sr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Irr_s<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 0;

  let Dst1 = rd1;
}

class Iir_s<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryS<opcode, SrcImm, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<16> imm;
  bits<4> rs1;

  let OperandAddrMode = OpndIR.Value;
  let OperandAM = 1;

  let Imm0 = imm;
  let Src1 = rs1;
}

class Iir_sr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Iir_s<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 1;

  let Dst1 = rd1;
}

class Imr_s<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryS<opcode, SrcCodeAddr, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rs1;
  // FIXME: ensure we encode address mode properly
  bits<16> src0;

  let OperandAddrMode = OpndCR.Value;
  let OperandAM = 2;

  let Src1 = rs1;
  let Imm0 = src0;
}

class Isr_s<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  // FIXME: SrcStackAbsolute is incorrect here, we need to be able to properly encode relative and post-increment
  // FIXME: Does encoding via post-fixup, does disassembly manually
  : IBinaryS<opcode, SrcStackAbsolute, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0;
  bits<4> rs1;

  let OperandAddrMode = OpndSR.Value;
  let OperandAM = 3;

  let Src0 = src0{3-0};
  let Src1 = rs1;
  let Imm0 = src0{19-4};
}

class Imr_sr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Imr_s<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 2;

  let Dst1 = rd1;
}

class Isr_sr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Isr_s<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let DestAddrMode = ToStackReg.Value;
  let OperandAM = 3;

  let Dst1 = rd1;
}

class IRet<EraVMOpcode opcode,
           bit to_label,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm <opcode, (outs), ins, "<invalid>", asmstr, pattern > {

  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;

  let AsmString = !strconcat(opcode.Name,
                             !if(to_label, ".to_label", ""),
                             "${cc}", "\t", asmstr);
  let Opcode = RetOpcEncoder<opcode.BaseOpcode, to_label>.Opcode;

}

class ICall<EraVMOpcode opcode, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, (outs), ins, "<invalid>", asmstr, pattern > {
  // FIXME: Should should be default
  let AsmString = !strconcat(opcode.Name, "${cc}", "\t", asmstr);
}

class IFarCall<EraVMOpcode opcode,
               bit is_shard, bit is_static,
               dag ins,
               string asmstr, list<dag> pattern>
  : IForm <opcode, (outs), ins, "<invalid>", asmstr, pattern > {

  let AsmString = !strconcat(opcode.Name,
                             !if(is_shard, ".shard", ""),
                             !if(is_static, ".static", ""),
                             "${cc}", "\t", asmstr);
  let Opcode = FarCallOpcEncoder<opcode.BaseOpcode, is_shard, is_static>.Opcode;
}

class IUMA<EraVMOpcode opcode,
           SrcMode src, bit is_inc,
           dag outs, dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm <opcode, outs, ins, "<invalid>", asmstr, pattern> {

  let AsmString = !strconcat(opcode.Name,
                             !if(is_inc, ".inc", ""),
                             "${cc}", "\t", asmstr);
  let Opcode = UMAOpcEncoder<opcode.Encoding, opcode.BaseOpcode,
                             src, is_inc>.Opcode;
}

class IUMAr_r<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcReg, 0, outs, ins, asmstr, pattern> {
}

class IUMAr_rr<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcReg, 1, outs, ins, asmstr, pattern> {
}

class IUMAi_r<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcImm, 0, outs, ins, asmstr, pattern> {
}

class IUMAi_rr<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcImm, 1, outs, ins, asmstr, pattern> {
}

class IUMArr_<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcReg, 0, outs, ins, asmstr, pattern> {
}

class IUMArr_r<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcReg, 1, outs, ins, asmstr, pattern> {
}

class IUMAir_<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcImm, 0, outs, ins, asmstr, pattern> {
}

class IUMAir_r<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcImm, 1, outs, ins, asmstr, pattern> {
}

// FIXME: There should be IUMAs / IUMAc
