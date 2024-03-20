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
                    SrcSpecialMode src, bit is_inc> {
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

def OpDecommit       : EraVMOpcode<"log.decommit", 1093, DirectEncoding>;
def OpTransientLoad  : EraVMOpcode<"tload",    1094, DirectEncoding>;
def OpTransientStore : EraVMOpcode<"tstore",   1095, DirectEncoding>;

// FIXME: Add those
// def OpStaticRead src inc ⇒ 1096 + 2 × src + inc
// def OpStaticWrite src inc ⇒ 1100 + 2 × src + inc
