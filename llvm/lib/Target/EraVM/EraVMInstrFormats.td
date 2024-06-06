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
class SrcOperandMode;
def OpndAddrNotSet : SrcOperandMode;
def OpndRR         : SrcOperandMode;
def OpndIR         : SrcOperandMode;
def OpndSR         : SrcOperandMode;
def OpndCR         : SrcOperandMode;

// DestAddrMode:
class DestOperandMode;
def DestAddrNotSet : DestOperandMode;
def ToReg          : DestOperandMode;
def ToStack        : DestOperandMode;

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

class FlipFlagSetting<string from, string to> : InstrMapping {
  let FilterClass = "FlagSetting";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "ReverseOperands"];
  let ColFields = ["Silent"];
  let KeyCol = [from];
  let ValueCols = [[to]];
}

def getFlagSettingOpcode : FlipFlagSetting<"PreserveFlags", "SetFlags">;
def getNonFlagSettingOpcode : FlipFlagSetting<"SetFlags", "PreserveFlags">;

// Instructions that support swapping arguments
// This included non-commutative arithmetic ones: sub, shl, shr, rol, ror, div and
// pointer manipulation: ptr.add, ptr.sub, ptr.shrink, ptr.sub
class Swappable;

class mod_swap<bit swap> {
  bit Value = swap;
}

def NoSwap : mod_swap<0>;
def Swap : mod_swap<1>;

class FlipInsSwapped<string from, string to> : InstrMapping {
  let FilterClass = "Swappable";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "DestAddrMode", "Silent"];
  let ColFields = ["ReverseOperands"];
  let KeyCol = [from];
  let ValueCols = [[to]];
}

def withInsSwapped : FlipInsSwapped<"NoSwap", "Swap">;
def withInsNotSwapped : FlipInsSwapped<"Swap", "NoSwap">;

class AddrModeRel; // generic map of addressing mode of operands
class RetAddrModeRel; // generic map of addressing mode of results

class MapInputFrom<string from> : InstrMapping {
  let FilterClass = "AddrModeRel";
  let RowFields = ["BaseOpcode", "DestAddrMode", "Silent"];
  let ColFields = ["OperandAddrMode"];
  let KeyCol = [from];
  let ValueCols = [["OpndRR"], ["OpndIR"], ["OpndCR"], ["OpndSR"]];
}

def mapRRInputTo : MapInputFrom<"OpndRR">;
def mapIRInputTo : MapInputFrom<"OpndIR">;
def mapCRInputTo : MapInputFrom<"OpndCR">;
def mapSRInputTo : MapInputFrom<"OpndSR">;

class WithDifferentResult<string from, string to> : InstrMapping {
  let FilterClass = "RetAddrModeRel";
  let RowFields = ["BaseOpcode", "OperandAddrMode", "Silent", "ReverseOperands"];
  let ColFields = ["DestAddrMode"];
  let KeyCol = [from];
  let ValueCols = [[to]];
}

def withStackResult : WithDifferentResult<"ToReg", "ToStack">;
def withRegisterResult : WithDifferentResult<"ToStack", "ToReg">;

include "EraVMOpcodes.td"

//===----------------------------------------------------------------------===//
// EraVM Instructions
//===----------------------------------------------------------------------===//

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
  let DecoderMethod = "DecodeCCOperand";
}

class EraVMCondCode<int val> {
  int Encoding = val;
}
def COND_NONE    : EraVMCondCode<0>;
def COND_GT      : EraVMCondCode<1>;
def COND_LT      : EraVMCondCode<2>;
def COND_E       : EraVMCondCode<3>;
def COND_GE      : EraVMCondCode<4>;
def COND_LE      : EraVMCondCode<5>;
def COND_NE      : EraVMCondCode<6>;
def COND_GTOrLT  : EraVMCondCode<7>;
// This is a pseudo flag, alias of LT and used to distinguish between
// normal LT set by sub instruction which is reversible and LT set to
// indicate overflow caused by add/mul which are not reversible.
def COND_OF   : EraVMCondCode<8>;
def COND_INVALID : EraVMCondCode<-1>;

def CondCodes : GenericEnum {
  let FilterClass = "EraVMCondCode";
  let ValueField = "Encoding";
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
             string asmstr,
             list<dag> pattern> : EraVMInstruction, Encoding {
  bits<3> cc;

  dag InOperandList  = !con(inops, (ins pred:$cc));
  dag OutOperandList = oops;
  let Pattern        = pattern;
  let Size           = 8;

  let Predicate = cc;
  let Opcode = opcode.BaseOpcode;
  let AsmString = !strconcat(opcode.Name, "${cc}", "\t", asmstr);

  let UseLogicalOperandMappings = true;
}

class IJump<EraVMOpcode opcode,
            SrcMode src,
            dag outs, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, asmstr, pattern> {

  let isIndirectBranch = !ne(src, SrcImm);
  let mayLoad = !or(isStackIn<src>.Value, !eq(src, SrcCodeAddr));
  let Opcode = JumpOpcEncoder<opcode.BaseOpcode, src>.Opcode;
}

class IContextBase<EraVMOpcode opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, asmstr, pattern> { }

class IContext_<EraVMOpcode opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IContextBase<opcode, outs, ins, asmstr, pattern>;
class IContextr_<EraVMOpcode opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IContextBase<opcode, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  let Src0 = rs0;
}
class IContext_r<EraVMOpcode opcode, dag outs, dag ins, string asmstr, list<dag> pattern>
  : IContextBase<opcode, outs, ins, asmstr, pattern> {
  bits<4> rd0;
  let Dst0 = rd0;
}

class ILogRBase<EraVMOpcode opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, asmstr, pattern> { }

class ILogRr_r<EraVMOpcode opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
  : ILogRBase<opcode, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rd0;

  let Src0 = rs0;
  let Dst0 = rd0;
}

class ILogRrr_<EraVMOpcode opcode, dag outs, dag ins,
               string asmstr, list<dag> pattern>
  : ILogRBase<opcode, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;

  let Src0 = rs0;
  let Src1 = rs1;
}

class ILogRrr_r<EraVMOpcode opcode, dag outs, dag ins,
                string asmstr, list<dag> pattern>
  : ILogRBase<opcode, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;
  bits<4> rd0;

  let Src0 = rs0;
  let Src1 = rs1;
  let Dst0 = rd0;
}

class IBinary<EraVMOpcode opcode,
              SrcMode src, DstMode dst,
              mod_swap swap, mod_set_flags set_flags,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
  : IForm <opcode, outs, ins, asmstr, pattern > {
  string BaseOpcode;
  mod_set_flags Silent = set_flags;
  mod_swap ReverseOperands = swap;
  SrcOperandMode OperandAddrMode = OpndAddrNotSet;
  DestOperandMode DestAddrMode = DestAddrNotSet;

  let AsmString = !strconcat(opcode.Name,
                             !if(!eq(swap, Swap), ".s", ""),
                             !if(!eq(set_flags, SetFlags), "!", ""),
                             "${cc}", "\t", asmstr);
  let Defs = !if(!eq(set_flags, PreserveFlags), [], [Flags]);
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

  let DestAddrMode = ToReg;

  let Dst0 = rd0;
}

class IBinaryS<EraVMOpcode opcode,
               SrcMode src,
               mod_swap swap, mod_set_flags set_flags,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
  : IBinary <opcode, src, DstStackAbsolute, swap, set_flags, outs, ins, asmstr, pattern > {
  // Encode stack operand into 20 bits as reg + imm offset
  bits<20> dst0;

  let DestAddrMode = ToStack;

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

  let OperandAddrMode = OpndRR;

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

  let OperandAddrMode = OpndIR;

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

  let Dst1 = rd1;
}

class Imr_r<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryR<opcode, SrcCodeAddr, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0;
  bits<4> rs1;

  let OperandAddrMode = OpndCR;

  let Src0 = src0{3-0};
  let Imm0 = src0{19-4};
  let Src1 = rs1;
}

class Isr_r<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryR<opcode, SrcStackAbsolute, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0;
  bits<4> rs1;

  let OperandAddrMode = OpndSR;

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

  let Dst1 = rd1;
}

class Isr_rr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Isr_r<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

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

  let OperandAddrMode = OpndRR;

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

  let OperandAddrMode = OpndIR;

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

  let Dst1 = rd1;
}

class Imr_s<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryS<opcode, SrcCodeAddr, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0;
  bits<4> rs1;

  let OperandAddrMode = OpndCR;

  let Src0 = src0{3-0};
  let Imm0 = src0{19-4};
  let Src1 = rs1;
}

class Isr_s<EraVMOpcode opcode,
            mod_swap swap_operands, mod_set_flags silent,
            dag outs, dag ins,
            string asmstr,
            list<dag> pattern>
  : IBinaryS<opcode, SrcStackAbsolute, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<20> src0;
  bits<4> rs1;

  let OperandAddrMode = OpndSR;

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

  let Dst1 = rd1;
}

class Isr_sr<EraVMOpcode opcode,
             mod_swap swap_operands, mod_set_flags silent,
             dag outs, dag ins,
             string asmstr,
             list<dag> pattern>
  : Isr_s<opcode, swap_operands, silent, outs, ins, asmstr, pattern> {
  bits<4> rd1;

  let Dst1 = rd1;
}

class IRetBase<EraVMOpcode opcode,
               dag ins,
               string asmstr,
               list<dag> pattern>
  : IForm <opcode, (outs), ins, asmstr, pattern > {

  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

class IRet<EraVMOpcode opcode,
           dag ins,
           string asmstr,
           list<dag> pattern>
  : IRetBase <opcode, ins, asmstr, pattern > {
  bits<4> rs0;

  let Src0 = rs0;
}

class IRetToLabel<EraVMOpcode opcode,
                  dag ins,
                  string asmstr,
                  list<dag> pattern>
  : IRetBase <opcode, ins, asmstr, pattern > {
  bits<4> rs0;
  bits<16> dest;

  let Src0 = rs0;
  let Imm0 = dest;
}

class ICall<EraVMOpcode opcode, dag ins, string asmstr, list<dag> pattern>
  : IForm <opcode, (outs), ins, asmstr, pattern > {
  bits<4> in1;
  bits<16> callee;
  bits<16> unwind;

  let Src0 = in1;
  let Imm0 = callee;
  let Imm1 = unwind;
}

class IFarCall<EraVMOpcode opcode,
               bit is_shard, bit is_static,
               dag ins,
               string asmstr, list<dag> pattern>
  : IForm <opcode, (outs), ins, asmstr, pattern > {
  bits<4> rs0;
  bits<4> rs1;
  bits<16> unwind;

  string Modifiers =
      !cond(!eq({is_static, is_shard}, {0, 0}): "",
            !eq({is_static, is_shard}, {1, 0}): ".static",
            !eq({is_static, is_shard}, {0, 1}): ".shard",
            !eq({is_static, is_shard}, {1, 1}): ".st.sh",
            true: ".INVALID");
  let AsmString = !strconcat(opcode.Name, Modifiers, "${cc}", "\t", asmstr);
  let Opcode = FarCallOpcEncoder<opcode.BaseOpcode, is_shard, is_static>.Opcode;

  let Src0 = rs0;
  let Src1 = rs1;
  let Imm0 = unwind;
}

class IUMA<EraVMOpcode opcode,
           SrcSpecialMode src,
           dag outs, dag ins,
           string asmstr,
           list<dag> pattern>
  : IForm <opcode, outs, ins, asmstr, pattern> {

  let AsmString = !strconcat(opcode.Name,
                             "${cc}", "\t", asmstr);
  let Opcode = UMAOpcEncoder<opcode.Encoding, opcode.BaseOpcode, src>.Opcode;
}

class IUMAr_r<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcSpecialReg, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rd0;

  let Src0 = rs0;
  let Dst0 = rd0;
}

class IUMAr_rr<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcSpecialReg, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rd0;
  bits<4> rd1;

  let Src0 = rs0;
  let Dst0 = rd0;
  let Dst1 = rd1;
}

class IUMAi_r<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcSpecialImm, outs, ins, asmstr, pattern> {
  bits<16> addr;
  bits<4> rd0;

  let Imm0 = addr;
  let Dst0 = rd0;
}

class IUMAi_rr<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcSpecialImm, outs, ins, asmstr, pattern> {
  bits<16> addr;
  bits<4> rd0;
  bits<4> rd1;

  let Imm0 = addr;
  let Dst0 = rd0;
  let Dst1 = rd1;
}

class IUMArr_<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcSpecialReg, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;

  let Src0 = rs0;
  let Src1 = rs1;
}

class IUMArr_r<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcSpecialReg, outs, ins, asmstr, pattern> {
  bits<4> rs0;
  bits<4> rs1;
  bits<4> rd0;

  let Src0 = rs0;
  let Src1 = rs1;
  let Dst0 = rd0;
}

class IUMAir_<EraVMOpcode opcode,
              dag outs, dag ins,
              string asmstr,
              list<dag> pattern>
 : IUMA<opcode, SrcSpecialImm, outs, ins, asmstr, pattern> {
  bits<16> addr;
  bits<4> rs1;

  let Imm0 = addr;
  let Src1 = rs1;
}

class IUMAir_r<EraVMOpcode opcode,
               dag outs, dag ins,
               string asmstr,
               list<dag> pattern>
 : IUMA<opcode, SrcSpecialImm, outs, ins, asmstr, pattern> {
  bits<16> addr;
  bits<4> rs1;
  bits<4> rd0;

  let Imm0 = addr;
  let Src1 = rs1;
  let Dst0 = rd0;
}
