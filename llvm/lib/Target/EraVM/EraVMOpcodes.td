//===-- EraVMOpcodes.td - EraVM Instruction Opcodes --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the EraVM opcodes in TableGen format.
//
//===----------------------------------------------------------------------===//

include "llvm/TableGen/SearchableTable.td"

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

class SrcSpecialMode<bits<3> val> {
  bits<3> Value = val;
}
def SrcSpecialReg : SrcSpecialMode<0>;
def SrcSpecialImm : SrcSpecialMode<1>;

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

class OpcodeEncoding {
  bits<11> SrcMultiplier = 0; // No in_any operand by default.
  bits<11> DstMultiplier = 0; // No out_any operand by default.

  // Index of the stackout operand in InOperandList of the instruction, if any.
  bits<4> IndexOfStackDstUse = 0;
  // Note that if an instruction has a stackin operand, it is always at
  // the beginning of InOperandList.
}

def DirectEncoding     : OpcodeEncoding; // opcode ⇒ opcode
let SrcMultiplier = 4, DstMultiplier = 1, IndexOfStackDstUse = 2 in
def NopEncoding        : OpcodeEncoding; // opcode src dst ⇒ opcode + 4 × src + 1 × dst
let SrcMultiplier = 8, DstMultiplier = 2, IndexOfStackDstUse = 2 in
def ArithCommEncoding  : OpcodeEncoding; // opcode src dst set_flags ⇒ opcode + 8 × src + 2 × dst + set_flags
let SrcMultiplier = 16, DstMultiplier = 4, IndexOfStackDstUse = 2 in
def ArithNCommEncoding : OpcodeEncoding; // opcode src dst swap set_flags ⇒ opcode + 16 × src + 4 × dst + 2 × set_flags + swap
let SrcMultiplier = 8, DstMultiplier = 2, IndexOfStackDstUse = 2 in
def ArithPtrEncoding   : OpcodeEncoding; // opcode src dst swap ⇒ opcode + 8 × src + 2 × dst + swap
def FarCallEncoding    : OpcodeEncoding; // opcode is_shard is_static ⇒ opcode + 2 × is_static + is_shard
let SrcMultiplier = 1 in
def JumpEncoding       : OpcodeEncoding; // opcode src ⇒ opcode + 1 × src
def HeapOpEncoding     : OpcodeEncoding; // opcode src_special inc ⇒ opcode + 10 × src_special + inc
def StaticOpEncoding   : OpcodeEncoding; // opcode src_special inc ⇒ opcode + 2 × src_special + inc

class ArithOpcEncoder<OpcodeEncoding encoding, bits<11> BaseOpcode,
                      SrcMode src, DstMode dst,
                      mod_swap swap, mod_set_flags set_flags> {
  bits<11> Opcode = !add(
      BaseOpcode,
      !mul(encoding.SrcMultiplier, src.Value),
      !mul(encoding.DstMultiplier, dst.Value),
      !cond(!eq(encoding, NopEncoding)        : 0,
            !eq(encoding, ArithCommEncoding)  : set_flags.Value,
            !eq(encoding, ArithNCommEncoding) : !add(!mul(set_flags.Value, 2), swap.Value),
            !eq(encoding, ArithPtrEncoding)   : swap.Value,
            true: -1000000));
}

class FarCallOpcEncoder<bits<11> BaseOpcode,
                        bit is_shard, bit is_static> {
  bits<11> Opcode = !add(BaseOpcode, !mul(2, is_static), is_shard);
}

class JumpOpcEncoder<bits<11> BaseOpcode,
                     SrcMode src> {
  bits<11> Opcode = !add(BaseOpcode, src.Value);
}

class UMAOpcEncoder<OpcodeEncoding encoding, bits<11> BaseOpcode,
                    SrcSpecialMode src> {
  bits<11> Opcode =
    !cond(!eq(encoding, DirectEncoding)  : BaseOpcode,
          !eq(encoding, HeapOpEncoding)  : !add(BaseOpcode, !mul(10, src.Value)),
          !eq(encoding, StaticOpEncoding): !add(BaseOpcode, !mul(2, src.Value)),
          true : -1);
}

class EraVMOpcode<string name, bits<11> opc, OpcodeEncoding encoding> {
  string Name = name;
  bits<11> BaseOpcode = opc;
  OpcodeEncoding Encoding = encoding;
  bits<11> SrcMultiplier = Encoding.SrcMultiplier;
  bits<11> DstMultiplier = Encoding.DstMultiplier;
  bits<4> IndexOfStackDstUse = Encoding.IndexOfStackDstUse;
}
def EraVMOpcodesList : GenericTable {
  let FilterClass = "EraVMOpcode";
  let CppTypeName = "EraVMOpcodeInfo";
  let Fields = [
    "Name", "BaseOpcode", "SrcMultiplier", "DstMultiplier", "IndexOfStackDstUse",
  ];
  let PrimaryKey = [ "BaseOpcode" ];
  let PrimaryKeyName = "lookupEraVMOpcodeByValue";
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

def OpPtrAdd    : EraVMOpcode<"addp",  847, ArithPtrEncoding>;
def OpPtrSub    : EraVMOpcode<"subp",  895, ArithPtrEncoding>;
def OpPtrPack   : EraVMOpcode<"pack",  943, ArithPtrEncoding>;
def OpPtrShrink : EraVMOpcode<"shrnk", 991, ArithPtrEncoding>;
def OpCall      : EraVMOpcode<"call",  1039, DirectEncoding>;

def OpContextThis              : EraVMOpcode<"this", 1040, DirectEncoding>;
def OpContextCaller            : EraVMOpcode<"par",  1041, DirectEncoding>;
def OpContextCodeAddress       : EraVMOpcode<"code", 1042, DirectEncoding>;
def OpContextMeta              : EraVMOpcode<"meta", 1043, DirectEncoding>;
def OpContextErgsLeft          : EraVMOpcode<"ergs", 1044, DirectEncoding>;
def OpContextSp                : EraVMOpcode<"sp",   1045, DirectEncoding>;
def OpContextGetContextU128    : EraVMOpcode<"ldvl", 1046, DirectEncoding>;
def OpContextSetContextU128    : EraVMOpcode<"stvl", 1047, DirectEncoding>;
def OpContextIncrementTxNumber : EraVMOpcode<"inctx", 1049, DirectEncoding>;

// Removed in VM 1.5.0
def OpContextSetErgsPerPubdataByte : EraVMOpcode<"stpub", 1048, DirectEncoding>;

def OpSload     : EraVMOpcode<"lds", 1050, DirectEncoding>;
def OpSstore    : EraVMOpcode<"sts", 1051, DirectEncoding>;

def OpLogToL1         : EraVMOpcode<"logL1",   1052, DirectEncoding>; // is_initializer ⇒ 1052 + is_initializer
def OpLogToL1Initial  : EraVMOpcode<"logL1.i", 1053, DirectEncoding>;
def OpLogEvent        : EraVMOpcode<"log",     1054, DirectEncoding>; // is_initializer ⇒ 1054 + is_initializer
def OpLogEventInitial : EraVMOpcode<"log.i",   1055, DirectEncoding>;
def OpLogPrecompile   : EraVMOpcode<"callp",   1056, DirectEncoding>;

def OpFarcall  : EraVMOpcode<"callf", 1057, FarCallEncoding>; // is_shard is_static ⇒ 1057 + 2 × is_static + is_shard
def OpDelegate : EraVMOpcode<"calld", 1061, FarCallEncoding>; // is_shard is_static ⇒ 1061 + 2 × is_static + is_shard
def OpMimic    : EraVMOpcode<"callm", 1065, FarCallEncoding>; // is_shard is_static ⇒ 1065 + 2 × is_static + is_shard

def OpRet           : EraVMOpcode<"ret",  1069, DirectEncoding>; // to_label ⇒ 1069 + to_label
def OpRetToLabel    : EraVMOpcode<"retl", 1070, DirectEncoding>;
def OpRevert        : EraVMOpcode<"rev",  1071, DirectEncoding>; // to_label ⇒ 1071 + to_label
def OpRevertToLabel : EraVMOpcode<"revl", 1072, DirectEncoding>;
def OpPanic         : EraVMOpcode<"pnc",  1073, DirectEncoding>; // to_label ⇒ 1073 + to_label
def OpPanicToLabel  : EraVMOpcode<"pncl", 1074, DirectEncoding>;

def OpLoadHeap        : EraVMOpcode<"ldm.h",   1075, HeapOpEncoding>; // src inc ⇒ 1075 + 10 × src + inc
def OpLoadHeapInc     : EraVMOpcode<"ldmi.h",  1076, HeapOpEncoding>;
def OpStoreHeap       : EraVMOpcode<"stm.h",   1077, HeapOpEncoding>; // src inc ⇒ 1077 + 10 × src + inc
def OpStoreHeapInc    : EraVMOpcode<"stmi.h",  1078, HeapOpEncoding>;

def OpLoadAuxHeap     : EraVMOpcode<"ldm.ah",  1079, HeapOpEncoding>; // src inc ⇒ 1079 + 10 × src + inc
def OpLoadAuxHeapInc  : EraVMOpcode<"ldmi.ah", 1080, HeapOpEncoding>;
def OpStoreAuxHeap    : EraVMOpcode<"stm.ah",  1081, HeapOpEncoding>; // src inc ⇒ 1081 + 10 × src + inc
def OpStoreAuxHeapInc : EraVMOpcode<"stmi.ah", 1082, HeapOpEncoding>;

def OpStaticRead        : EraVMOpcode<"ldm.st",  1096, StaticOpEncoding>; // src inc ⇒ 1096 + 2 × src + inc
def OpStaticReadInc     : EraVMOpcode<"ldmi.st", 1097, StaticOpEncoding>;
def OpStaticWrite       : EraVMOpcode<"stm.st",  1100, StaticOpEncoding>; // src inc ⇒ 1100 + 2 × src + inc
def OpStaticWriteInc    : EraVMOpcode<"stmi.st", 1101, StaticOpEncoding>;

def OpLoadPtr      : EraVMOpcode<"ldp",  1083, DirectEncoding>; // inc ⇒ 1083 + inc
def OpLoadPtrInc   : EraVMOpcode<"ldpi", 1084, DirectEncoding>;

def OpDecommit       : EraVMOpcode<"dcmt", 1093, DirectEncoding>;
def OpTransientLoad  : EraVMOpcode<"ldt",  1094, DirectEncoding>;
def OpTransientStore : EraVMOpcode<"stt",  1095, DirectEncoding>;
