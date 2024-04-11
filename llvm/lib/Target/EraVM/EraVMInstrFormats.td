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

include "llvm/TableGen/SearchableTable.td"

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

//===----------------------------------------------------------------------===//
// EraVM Opcodes
//===----------------------------------------------------------------------===//
class SrcMode<bits<3> val> {
  bits<3> Value = val;
}
def SrcReg : SrcMode<0>;
def SrcSpRelativePop : SrcMode<1>;
def SrcSpRelative : SrcMode<2>;
def SrcStackAbsolute : SrcMode<3>;
def SrcImm : SrcMode<4>;
def SrcCodeAddr : SrcMode<5>;

class isStackIn<SrcMode src> {
  bit Value = !or(
    !eq(src, SrcSpRelativePop),
    !eq(src, SrcSpRelative),
    !eq(src, SrcStackAbsolute));
}

class DstMode<bits<2> val> {
  bits<2> Value = val;
}

def DstReg : DstMode<0>;
def DstSpRelativePush : DstMode<1>;
def DstSpRelative : DstMode<2>;
def DstStackAbsolute : DstMode<3>;

class isStackOut<DstMode dst> {
  bit Value = !or(
    !eq(dst, DstSpRelativePush),
    !eq(dst, DstSpRelative),
    !eq(dst, DstStackAbsolute));
}

class OpcodeEncoding<bits<4> val> {
  bits<4> Value = val;
}

def DirectEncoding     : OpcodeEncoding<0>; // opcode ⇒ opcode
def NopEncoding        : OpcodeEncoding<1>; // opcode src dst ⇒ opcode + 4 × src + 1 × dst
def ArithCommEncoding  : OpcodeEncoding<2>; // opcode src dst set_flags ⇒ opcode + 8 × src + 2 × dst + set_flags
def ArithNCommEncoding : OpcodeEncoding<3>; // opcode src dst swap set_flags ⇒ opcode + 16 × src + 4 × dst + 2 × set_flags + swap
def ArithPtrEncoding   : OpcodeEncoding<4>; // opcode src dst swap ⇒ opcode + 8 × src + 2 × dst + swap
def FarCallEncoding    : OpcodeEncoding<5>; // opcode is_shard is_static ⇒ opcode + 2 × is_static + is_shard
def RetEncoding        : OpcodeEncoding<6>; // opcode to_label ⇒ opcode + to_label
def JumpEncoding       : OpcodeEncoding<7>; // opcode src ⇒ opcode + 1 × src
def LoadPtrEncoding    : OpcodeEncoding<8>; // opcode inc ⇒ opcode + inc
def HeapOpEncoding     : OpcodeEncoding<9>; // opcode src inc ⇒ opcode + 10 × src + inc
def LogEncoding        : OpcodeEncoding<10>;// opcode is_first ⇒ opcode + is_first

class ArithOpcEncoder<OpcodeEncoding encoding, bits<11> BaseOpcode,
                      SrcMode src, DstMode dst,
                      mod_swap swap, mod_set_flags set_flags> {
  bits<11> Opcode =
    !cond(!eq(encoding, NopEncoding)        : !add(BaseOpcode, !mul(4,  src.Value),         dst.Value),
          !eq(encoding, ArithCommEncoding)  : !add(BaseOpcode, !mul(8,  src.Value), !mul(2, dst.Value), set_flags.Value),
          !eq(encoding, ArithNCommEncoding) : !add(BaseOpcode, !mul(16, src.Value), !mul(4, dst.Value), !mul(set_flags.Value, 2), swap.Value),
          !eq(encoding, ArithPtrEncoding)   : !add(BaseOpcode, !mul(8,  src.Value), !mul(2, dst.Value), swap.Value),
          true: -1);
}

class FarCallOpcEncoder<bits<11> BaseOpcode,
                        bit is_shard, bit is_static> {
  bits<11> Opcode = !add(BaseOpcode, !mul(2, is_static), is_shard);
}

class RetOpcEncoder<bits<11> BaseOpcode,
                    bit to_label> {
  bits<11> Opcode = !add(BaseOpcode, to_label);
}

class JumpOpcEncoder<bits<11> BaseOpcode,
                     SrcMode src> {
  bits<11> Opcode = !add(BaseOpcode, src.Value);
}

class UMAOpcEncoder<OpcodeEncoding encoding, bits<11> BaseOpcode,
                    SrcMode src, bit is_inc> {
  bits<11> Opcode =
    !cond(!eq(encoding, LoadPtrEncoding) : !add(BaseOpcode, is_inc),
          !eq(encoding, HeapOpEncoding)  : !add(BaseOpcode, !mul(10, src.Value), is_inc),
          true : -1);
}

class LogROpcEncoder<OpcodeEncoding encoding, bits<11> BaseOpcode,
                     bit is_first> {
  bits<11> Opcode =
    !cond(!eq(encoding, LogEncoding) : !add(BaseOpcode, is_first),
          !eq(encoding, DirectEncoding) : BaseOpcode,
          true : -1);
}

class EraVMOpcode<string name, bits<11> opc, OpcodeEncoding encoding> {
  string Name = name;
  bits<11> BaseOpcode = opc;
  OpcodeEncoding Encoding = encoding;
}
def EraVMOpcodesList : GenericTable {
  let FilterClass = "EraVMOpcode";
  let Fields = [
    "Name", "BaseOpcode",
  ];
  let PrimaryKey = [ "BaseOpcode" ];
  let PrimaryKeyName = "lookupEraVMOpcodeByValue";
}
def lookupEraVMOpcodeByName : SearchIndex {
  let Table = EraVMOpcodesList;
  let Key = [ "Name" ];
}

def OpInvalid : EraVMOpcode<"<invalid>", 0, DirectEncoding>;
def OpNoOp : EraVMOpcode<"nop",  1,   NopEncoding>;
def OpAdd  : EraVMOpcode<"add",  25,  ArithCommEncoding>;
def OpSub  : EraVMOpcode<"sub",  73,  ArithNCommEncoding>;
def OpMul  : EraVMOpcode<"mul",  169, ArithCommEncoding>;
def OpDiv  : EraVMOpcode<"div",  217, ArithNCommEncoding>;
def OpJump : EraVMOpcode<"jump", 313, JumpEncoding>; // src ⇒ 313 + 1 × src
def OpXor  : EraVMOpcode<"xor",  319, ArithCommEncoding>;
def OpAnd  : EraVMOpcode<"and",  367, ArithCommEncoding>;
def OpOr   : EraVMOpcode<"or",   415, ArithCommEncoding>;
def OpShl  : EraVMOpcode<"shl",  463, ArithNCommEncoding>;
def OpShr  : EraVMOpcode<"shr",  559, ArithNCommEncoding>;
def OpRol  : EraVMOpcode<"rol",  655, ArithNCommEncoding>;
def OpRor  : EraVMOpcode<"ror",  751, ArithNCommEncoding>;
def OpPtrAdd    : EraVMOpcode<"ptr.add",    847, ArithPtrEncoding>;
def OpPtrSub    : EraVMOpcode<"ptr.sub",    895, ArithPtrEncoding>;
def OpPtrPack   : EraVMOpcode<"ptr.pack",   943, ArithPtrEncoding>;
def OpPtrShrink : EraVMOpcode<"ptr.shrink", 991, ArithPtrEncoding>;
def OpCall      : EraVMOpcode<"near_call",  1039, DirectEncoding>;

def OpContextThis              : EraVMOpcode<"context.this",             1040, DirectEncoding>;
def OpContextCaller            : EraVMOpcode<"context.caller",           1041, DirectEncoding>;
def OpContextCodeAddress       : EraVMOpcode<"context.code_source",      1042, DirectEncoding>;
def OpContextMeta              : EraVMOpcode<"context.meta",             1043, DirectEncoding>;
def OpContextErgsLeft          : EraVMOpcode<"context.gas_left",         1044, DirectEncoding>;
def OpContextSp                : EraVMOpcode<"context.sp",               1045, DirectEncoding>;
def OpContextGetContextU128    : EraVMOpcode<"context.get_context_u128", 1046, DirectEncoding>;
def OpContextSetContextU128    : EraVMOpcode<"context.set_context_u128", 1047, DirectEncoding>;
def OpContextSetErgsPerPubdataByte   : EraVMOpcode<"context.set_gas_per_pubdata",    1048, DirectEncoding>;
def OpContextIncrementTxNumber : EraVMOpcode<"context.inc_tx_num",       1049, DirectEncoding>;

// aliased as log.sread
def OpSload     : EraVMOpcode<"sload",      1050, DirectEncoding>;
// aliased as log.swrite
def OpSstore    : EraVMOpcode<"sstore",     1051, DirectEncoding>;

// aliased as log.to_l1
def OpLogToL1   : EraVMOpcode<"to_l1",      1052, LogEncoding>; // is_first ⇒ 1052 + is_first
// aliased as log.event
def OpLogEvent  : EraVMOpcode<"event",      1054, LogEncoding>; // is_first ⇒ 1054 + is_first
// aliased as log.precompile
def OpLogPrecompile : EraVMOpcode<"precompile", 1056, DirectEncoding>;

def OpFarcall  : EraVMOpcode<"far_call",     1057, FarCallEncoding>; // is_shard is_static ⇒ 1057 + 2 × is_static + is_shard
def OpDelegate : EraVMOpcode<"far_call.delegate", 1061, FarCallEncoding>; // is_shard is_static ⇒ 1061 + 2 × is_static + is_shard
def OpMimic    : EraVMOpcode<"far_call.mimic",    1065, FarCallEncoding>; // is_shard is_static ⇒ 1065 + 2 × is_static + is_shard

def OpRet      : EraVMOpcode<"ret.ok",     1069, RetEncoding>; // to_label ⇒ 1069 + to_label
def OpRevert   : EraVMOpcode<"ret.revert", 1071, RetEncoding>; // to_label ⇒ 1071 + to_label
def OpPanic    : EraVMOpcode<"ret.panic",  1073, RetEncoding>; // to_label ⇒ 1073 + to_label

// aliased as uma.heap_read
def OpLoadHeap     : EraVMOpcode<"ld.1", 1075, HeapOpEncoding>; // src inc ⇒ 1075 + 10 × src + inc
// aliased as uma.heap_write
def OpStoreHeap    : EraVMOpcode<"st.1", 1077, HeapOpEncoding>; // src inc ⇒ 1077 + 10 × src + inc
// aliased as uma.aux_heap_read
def OpLoadAuxHeap  : EraVMOpcode<"ld.2", 1079, HeapOpEncoding>; // src inc ⇒ 1079 + 10 × src + inc
// aliased as uma.aux_heap_write
def OpStoreAuxHeap : EraVMOpcode<"st.2", 1081, HeapOpEncoding>; // src inc ⇒ 1081 + 10 × src + inc

// aliased as uma.fat_ptr_read
def OpLoadPtr      : EraVMOpcode<"ld",  1083, LoadPtrEncoding>; // inc ⇒ 1083 + inc

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

class ILogR<EraVMOpcode opcode, dag outs, dag ins,
            string opc, string asmstr, list<dag> pattern>
  : IForm <opcode, outs, ins, opc, asmstr, pattern> {
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
