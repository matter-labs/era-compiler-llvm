//===-------- EVMInstrInfo.td - EVM Instruction defs -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the EVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// EVM-specific DAG Node Types.
//===----------------------------------------------------------------------===//

def SDT_EVMCallSeqStart
  : SDCallSeqStart<[SDTCisVT<0, i256>, SDTCisVT<1, i256>]>;

def SDT_EVMCallSeqEnd
  : SDCallSeqEnd<[SDTCisVT<0, i256>, SDTCisVT<1, i256>]>;

def SDT_EVMArgument
  : SDTypeProfile<1, 1, [SDTCisVT<1, i256>]>;

def SDT_EVMRet
  : SDTypeProfile<0, -1, []>;

def SDT_EVMSignextend
  : SDTypeProfile<1, 2, [SDTCisSameAs<0, 1>, SDTCisVT<1, i256>]>;

def SDT_EVMTargetAddrWrapper
  : SDTypeProfile<1, 1, [SDTCisPtrTy<0>]>;

def SDT_EVMMemcpy
  : SDTypeProfile<0, 3, [SDTCisPtrTy<0>, SDTCisPtrTy<1>, SDTCisInt<2>]>;

//===----------------------------------------------------------------------===//
// EVM-specific DAG Nodes.
//===----------------------------------------------------------------------===//

def EVMargument
  : SDNode<"EVMISD::ARGUMENT", SDT_EVMArgument>;

def EVMcallseq_start
  : SDNode<"ISD::CALLSEQ_START", SDT_EVMCallSeqStart,
            [SDNPHasChain, SDNPOutGlue]>;

def EVMcallseq_end
  : SDNode<"ISD::CALLSEQ_END", SDT_EVMCallSeqEnd,
            [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def EVMret
  : SDNode<"EVMISD::RET", SDT_EVMRet,
           [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def EVMSignextend
  : SDNode<"EVMISD::SIGNEXTEND", SDT_EVMSignextend>;

def EVMTargetAddrWrapper
  : SDNode<"EVMISD::TARGET_ADDR_WRAPPER", SDT_EVMTargetAddrWrapper>;

def EVMMemcpy_call_data
  : SDNode<"EVMISD::MEMCPY_CALL_DATA", SDT_EVMMemcpy,
            [SDNPHasChain, SDNPMayLoad, SDNPMayStore]>;

def EVMMemcpy_return_data
  : SDNode<"EVMISD::MEMCPY_RETURN_DATA", SDT_EVMMemcpy,
            [SDNPHasChain, SDNPMayLoad, SDNPMayStore]>;

def EVMMemcpy_code
  : SDNode<"EVMISD::MEMCPY_CODE", SDT_EVMMemcpy,
            [SDNPHasChain, SDNPMayLoad, SDNPMayStore]>;

def EVMMemcpy_heap
  : SDNode<"EVMISD::MEMCPY_HEAP", SDT_EVMMemcpy,
            [SDNPHasChain, SDNPMayLoad, SDNPMayStore]>;

//===----------------------------------------------------------------------===//
// EVM Operand Definitions.
//===----------------------------------------------------------------------===//

let OperandType = "OPERAND_IMMEDIATE" in
def i256imm : Operand<i256>;

def neg_imm128 : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNegative() && Imm.abs().isIntN(128);
}]>;

def imm128 : Operand<i256>, IntImmLeaf<i256, [{
  return APInt(256, 1, false).shl(128).ugt(Imm);
}]>;

def jmptarget : Operand<OtherVT>;

//===----------------------------------------------------------------------===//
// EVM Register to Stack instruction mapping
//===----------------------------------------------------------------------===//

class StackRel;
def getStackOpcode : InstrMapping {
  let FilterClass = "StackRel";
  let RowFields = ["BaseName"];
  let ColFields = ["StackBased"];
  let KeyCol = ["0"];
  let ValueCols = [["1"]];
}

//===----------------------------------------------------------------------===//
// EVM Stack to Register instruction mapping
//===----------------------------------------------------------------------===//

class RegisterRel;
def getRegisterOpcode : InstrMapping {
  let FilterClass = "RegisterRel";
  let RowFields = ["BaseName"];
  let ColFields = ["StackBased"];
  let KeyCol = ["1"];
  let ValueCols = [["0"]];
}


//===----------------------------------------------------------------------===//
// EVM Instruction Format Definitions.
//===----------------------------------------------------------------------===//

include "EVMInstrFormats.td"


//===----------------------------------------------------------------------===//
// Custom DAG Selection Operations.
//===----------------------------------------------------------------------===//

def negate_imm : SDNodeXForm<imm, [{
  auto& neg = N->getAPIntValue();
  APInt pos = neg;
  pos.negate();
  return CurDAG->getTargetConstant(pos, SDLoc(N), MVT::i256);
}]>;


//===----------------------------------------------------------------------===//
// Pattern fragments for memory instructions.
//===----------------------------------------------------------------------===//

// Load pattern fragments
foreach AS = ["stack", "heap", "call_data", "storage", "tstorage"] in {
  let AddressSpaces = !cast<ASList>("AS_"#AS).List in {
    def load_#AS : PatFrag<(ops node:$ptr), (load node:$ptr)> {
      let IsLoad = 1;
    }
  }
}

// Store patterns for Stack and Memory
foreach AS = ["stack", "heap", "storage", "tstorage"] in {
  let AddressSpaces = !cast<ASList>("AS_"#AS).List in {
    def store_#AS : PatFrag<(ops node:$val, node:$ptr),
                            (store node:$val, node:$ptr)> {
      let IsStore = 1;
    }
  }
}

let AddressSpaces = AS_heap.List in
def truncstorei8_heap : PatFrag<(ops node:$val, node:$ptr),
                                (truncstore node:$val, node:$ptr)> {
  let IsStore = 1;
  let MemoryVT = i8;
}


//===----------------------------------------------------------------------===//
// EVM Instructions list.
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Pseudo instructions.
//===----------------------------------------------------------------------===//

let usesCustomInserter = 1 in
def SELECT
  : EVMPseudo<(outs GPR:$dst), (ins GPR:$cond, GPR:$lhs, GPR:$rhs),
              [(set GPR:$dst, (select GPR:$cond, GPR:$lhs, GPR:$rhs))]>;


//===----------------------------------------------------------------------===//
// Additional instructions.
//===----------------------------------------------------------------------===//

// Call sequence markers.
let isCodeGenOnly = 1, Defs = [SP], Uses = [SP] in {
def ADJCALLSTACKDOWN
  : EVMPseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
              [(EVMcallseq_start timm:$amt1, timm:$amt2)]>;

def ADJCALLSTACKUP
  : EVMPseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
              [(EVMcallseq_end timm:$amt1, timm:$amt2)]>;
}

let isCodeGenOnly = 1 in {
let hasSideEffects = 1 in
def ARGUMENT
  : NRI<(outs GPR:$res), (ins i256imm:$argno),
       [(set GPR:$res, (EVMargument timm:$argno))],
       "ARGUMENT $res, $argno">;

// This is not real EVM instruction. It should be eliminted while
// stackification.
let isMoveImm = 1, isAsCheapAsAMove = 1, isReMaterializable = 1 in
def CONST_I256
  : NRI<(outs GPR:$res), (ins i256imm:$imm), [(set GPR:$res, imm:$imm)],
      "CONST_I256 $res, $imm">;

// This is not real EVM instruction. It should be eliminted while
// stackification.
let isAsCheapAsAMove = 1 in
def COPY_I256
  : NRI<(outs GPR:$res), (ins GPR:$src), [], "COPY_I256 $res, $src">;
}

def : Pat<(i256 (EVMTargetAddrWrapper tglobaladdr:$addr)),
          (CONST_I256 tglobaladdr:$addr)>;
def : Pat<(i256 (EVMTargetAddrWrapper texternalsym:$addr)),
          (CONST_I256 texternalsym:$addr)>;

let Uses = [SP], isCall = 1 in {

// CALL should take both variadic arguments and produce variadic results, but
// this is not possible to model directly. Instead, we select calls to a
// CALL_PARAMS taking variadic arguments linked with a CALL_RESULTS that handles
// producing the call's variadic results. We recombine the two in a custom
// inserter hook after DAG ISel, so passes over MachineInstrs will only ever
// observe CALL nodes with all of the expected variadic uses and defs.
let isPseudo = 1 in
def CALL_PARAMS
  : NRI<(outs), (ins jmptarget:$callee, variable_ops), [],
        "CALL_PARAMS\t$callee">;

let variadicOpsAreDefs = 1, usesCustomInserter = 1, isPseudo = 1 in
def CALL_RESULTS
  :  NRI<(outs), (ins variable_ops), [], "CALL_RESULTS">;

// Note that instructions with variable_ops have custom printers in
// EVMInstPrinter.cpp.

let variadicOpsAreDefs = 1 in
def FCALL
  : NRI<(outs), (ins jmptarget:$callee, variable_ops), [],
        "FCALL\t$callee">;
} // Uses = [SP], isCall = 1


//===----------------------------------------------------------------------===//
// EVM arithmetic instructions.
//===----------------------------------------------------------------------===//

multiclass BinaryInst<SDNode node, string opcode_str, bits<8> inst, int cost>
  : I<(outs GPR:$dst), (ins GPR:$lhs, GPR:$rhs),
      [(set GPR:$dst, (node GPR:$lhs, GPR:$rhs))],
      opcode_str, " $dst, $lhs, $rhs", inst, cost>;

let isCommutable = 1 in {
  defm ADD : BinaryInst<add, "ADD", 0x01, 3>;
  defm MUL : BinaryInst<mul, "MUL", 0x02, 5>;
}
defm SUB : BinaryInst<sub, "SUB", 0x03, 3>;
defm DIV : BinaryInst<udiv, "DIV", 0x04, 5>;
defm SDIV : BinaryInst<sdiv, "SDIV", 0x05, 5>;
defm MOD : BinaryInst<urem, "MOD", 0x06, 5>;
defm SMOD : BinaryInst<srem, "SMOD", 0x07, 5>;

defm ADDMOD
  : I<(outs GPR:$dst), (ins GPR:$add_op1, GPR:$add_op2, GPR:$denom),
      [(set GPR:$dst,
       (int_evm_addmod GPR:$add_op1, GPR:$add_op2, GPR:$denom))],
      "ADDMOD", " $dst, $add_op1, $add_op2, $denom", 0x08, 8>;

defm MULMOD
  : I<(outs GPR:$dst), (ins GPR:$mul_op1, GPR:$mul_op2, GPR:$denom),
      [(set GPR:$dst,
       (int_evm_mulmod GPR:$mul_op1, GPR:$mul_op2, GPR:$denom))],
      "MULMOD", " $dst, $mul_op1, $mul_op2, $denom", 0x09, 8>;

defm EXP
  : I<(outs GPR:$dst), (ins GPR:$base, GPR:$exp),
      [(set GPR:$dst, (int_evm_exp GPR:$base, GPR:$exp))],
      "EXP", " $dst, $base, $exp", 0x0A, 10>;

defm SIGNEXTEND
  : I<(outs GPR:$dst), (ins GPR:$size, GPR:$src),
      [(set GPR:$dst, (int_evm_signextend GPR:$size, GPR:$src))],
      "SIGNEXTEND", " $dst, $size, $src", 0x0B, 5>;

// The first operand of SIGNEXTEND is the type size in bytes of
// the value being extendent minus one.
def : Pat<(sext_inreg GPR:$src, i8), (SIGNEXTEND (CONST_I256 0), GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i16), (SIGNEXTEND (CONST_I256 1), GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i32), (SIGNEXTEND (CONST_I256 3), GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i64), (SIGNEXTEND (CONST_I256 7), GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i128), (SIGNEXTEND (CONST_I256 15), GPR:$src)>;

// Reverse some operations with negative immediate operands to reduce bit-size
// of the immediate encoding.
// TODO: It needs to be discussed starting which bit-size this transformation
// is deems profitable.
def : Pat<(neg_imm128:$imm),
          (SUB (CONST_I256 0), (CONST_I256 (negate_imm imm:$imm)))>;
def : Pat<(add GPR:$res, neg_imm128:$imm),
          (SUB GPR:$res, (CONST_I256 (negate_imm imm:$imm)))>;
def : Pat<(sub GPR:$res, neg_imm128:$imm),
          (ADD GPR:$res, (CONST_I256 (negate_imm imm:$imm)))>;

def : Pat<(int_evm_div GPR:$op1, GPR:$op2), (DIV GPR:$op1, GPR:$op2)>;
def : Pat<(int_evm_sdiv GPR:$op1, GPR:$op2), (SDIV GPR:$op1, GPR:$op2)>;
def : Pat<(int_evm_mod GPR:$op1, GPR:$op2), (MOD GPR:$op1, GPR:$op2)>;
def : Pat<(int_evm_smod GPR:$op1, GPR:$op2), (SMOD GPR:$op1, GPR:$op2)>;


//===----------------------------------------------------------------------===//
// EVM comparison instructions.
//===----------------------------------------------------------------------===//

multiclass ComparisonInst<CondCode cond, string opcode_str,
                          bits<8> inst, int cost>
  : I<(outs GPR:$dst), (ins GPR:$lhs, GPR:$rhs),
      [(set GPR:$dst, (setcc GPR:$lhs, GPR:$rhs, cond))],
      opcode_str, " $dst, $lhs, $rhs", inst, cost>;

let isCompare = 1 in {
defm ULT : ComparisonInst<SETULT, "LT", 0x10, 3>;
defm UGT : ComparisonInst<SETUGT, "GT", 0x11, 3>;
defm LT : ComparisonInst<SETLT, "SLT", 0x12, 3>;
defm GT : ComparisonInst<SETGT, "SGT", 0x13, 3>;
let isCommutable = 1 in
defm EQ : ComparisonInst<SETEQ, "EQ", 0x14, 3>;
defm ISZERO
  : I<(outs GPR:$dst), (ins GPR:$src),
      [(set GPR:$dst, (setcc GPR:$src, 0, SETEQ))],
      "ISZERO", " $dst, $src", 0x15, 3>;
} // isCompare = 1

// Patterns for comparison operations that have no
// corresponding machine instructions.
def : Pat<(setcc GPR:$rs0, GPR:$rs1, SETNE), (ISZERO (EQ GPR:$rs0, GPR:$rs1))>;
def : Pat<(setcc GPR:$rs0, GPR:$rs1, SETGE), (ISZERO (LT GPR:$rs0, GPR:$rs1))>;
def : Pat<(setcc GPR:$rs0, GPR:$rs1, SETLE), (ISZERO (GT GPR:$rs0, GPR:$rs1))>;
def : Pat<(setcc GPR:$rs0, GPR:$rs1, SETULE),
          (ISZERO (UGT GPR:$rs0, GPR:$rs1))>;
def : Pat<(setcc GPR:$rs0, GPR:$rs1, SETUGE),
          (ISZERO (ULT GPR:$rs0, GPR:$rs1))>;


//===----------------------------------------------------------------------===//
// EVM bitwise instructions.
//===----------------------------------------------------------------------===//

let isCommutable = 1 in {
  defm AND : BinaryInst<and, "AND", 0x16, 3>;
  defm OR : BinaryInst<or, "OR", 0x17, 3>;
  defm XOR : BinaryInst<xor, "XOR", 0x18, 3>;
}
defm NOT
  : I<(outs GPR:$dst), (ins GPR:$src), [(set GPR:$dst, (not GPR:$src))],
      "NOT", " $dst, $src", 0x19, 3>;

let mayLoad = 1 in
defm SHA3
  : I<(outs GPR:$dst), (ins GPR:$offset, GPR:$size),
      [(set GPR:$dst, (int_evm_sha3 GPR:$offset, GPR:$size))],
      "SHA3", " $dst, $offset, $size", 0x20, 30>;

defm BYTE
  : I<(outs GPR:$dst), (ins GPR:$idx, GPR:$val),
      [(set GPR:$dst, (int_evm_byte GPR:$idx, GPR:$val))],
      "BYTE", " $dst, $idx, $val", 0x1a, 3>;


//===----------------------------------------------------------------------===//
// EVM shift instructions.
//===----------------------------------------------------------------------===//

multiclass ShiftInst<SDNode node, string opcode_str, bits<8> inst, int cost>
  : I<(outs GPR:$dst), (ins GPR:$val, GPR:$shift),
      [(set GPR:$dst, (node GPR:$shift, GPR:$val))],
      opcode_str, " $dst, $shift, $val", inst, cost>;

defm SHL : ShiftInst<shl, "SHL", 0x1b, 3>;
defm SHR : ShiftInst<srl, "SHR", 0x1c, 3>;
defm SAR : ShiftInst<sra, "SAR", 0x1d, 3>;

def : Pat<(int_evm_shl GPR:$op1, GPR:$op2), (SHL GPR:$op1, GPR:$op2)>;
def : Pat<(int_evm_shr GPR:$op1, GPR:$op2), (SHR GPR:$op1, GPR:$op2)>;
def : Pat<(int_evm_sar GPR:$op1, GPR:$op2), (SAR GPR:$op1, GPR:$op2)>;


//===----------------------------------------------------------------------===//
// EVM control instructions.
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1 in {
// The condition operand is a boolean value which EVM represents as i256.
defm JUMPI
  : I<(outs), (ins jmptarget:$dst, GPR:$cond), [(brcond GPR:$cond, bb:$dst)],
      "JUMPI", " $dst, $cond", 0x57, 10>;

let isBarrier = 1 in
defm JUMP
  : I<(outs), (ins jmptarget:$dst), [(br bb:$dst)], "JUMP", " $dst", 0x56, 8>;
} // isBranch = 1, isTerminator = 1

// This isn't really a control flow instruction, but it should be used to mark
// destination of jump instructions.
defm JUMPDEST : I<(outs), (ins), [], "JUMPDEST", "", 0x5B, 1>;

let isBarrier = 1, isTerminator = 1, isReturn = 1 in
def RET : NRI<(outs), (ins variable_ops), [(EVMret)], "RET">;


//===----------------------------------------------------------------------===//
// EVM memory/storage instructions.
//===----------------------------------------------------------------------===//

let mayLoad = 1 in {
  defm MLOAD
    : I<(outs GPR:$dst), (ins GPR:$offset),
        [(set GPR:$dst, (load_heap GPR:$offset))],
        "MLOAD", " $dst, $offset", 0x51, 3>;

  defm SLOAD
    : I<(outs GPR:$dst), (ins GPR:$key),
        [(set GPR:$dst, (load_storage GPR:$key))],
        "SLOAD", " $dst, $key", 0x54, 100>;

  defm TLOAD
    : I<(outs GPR:$dst), (ins GPR:$key),
        [(set GPR:$dst, (load_tstorage GPR:$key))],
        "TLOAD", " $dst, $key", 0x5c, 100>;
}

let mayStore = 1 in {
  defm MSTORE
    : I<(outs), (ins GPR:$offset, GPR:$val),
        [(store_heap GPR:$val, GPR:$offset)],
        "MSTORE", " $offset, $val", 0x52, 3>;

  defm MSTORE8
    : I<(outs), (ins GPR:$offset, GPR:$val),
        [(int_evm_mstore8 GPR:$offset, GPR:$val)],
        "MSTORE8", " $offset, $val", 0x53, 3>;

  defm SSTORE
    : I<(outs), (ins GPR:$key, GPR:$val), [(store_storage GPR:$val, GPR:$key)],
        "SSTORE", " $key, $val", 0x55, 100>;

  defm TSTORE
    : I<(outs), (ins GPR:$key, GPR:$val), [(store_tstorage GPR:$val, GPR:$key)],
        "TSTORE", " $key, $val", 0x5d, 100>;
}

let mayStore = 1, mayLoad = 1 in
defm MCOPY
  : I<(outs), (ins GPR:$dst, GPR:$src, GPR:$size), [],
      "MCOPY", " $dst, $src, $size", 0x5E, 3>;

def : Pat<(EVMMemcpy_heap GPR:$dst, GPR:$src, GPR:$size),
          (MCOPY GPR:$dst, GPR:$src, GPR:$size)>;

let hasSideEffects = 1 in {
  defm PC : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_pc))],
               "PC", " $dst", 0x58, 2>;

  defm GAS : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_gas))],
                "GAS", " $dst", 0x5A, 2>;
}

defm MSIZE : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_msize))],
                "MSIZE", " $dst", 0x59, 2>;

// The i8 store is handled a speciall way, as EVM has a dedicated instruction
// for this.
def : Pat<(truncstorei8_heap GPR:$val, GPR:$off), (MSTORE8 GPR:$off, GPR:$val)>;

// Instructions for the stack manipulation. These are not real EVM instructions.
// They are used to model 'alloca' before stackification and should be
// eliminated at that stage.

def to_tframeindex : SDNodeXForm<frameindex, [{
  return CurDAG->getTargetFrameIndex(N->getIndex(), MVT::i256);
}]>;

def TargetFI: OutPatFrag<(ops node:$fi), (i256 (to_tframeindex $fi))>;

def add_like: PatFrags<(ops node:$lhs, node:$rhs),
                       [(add $lhs, $rhs), (or $lhs, $rhs)], [{
  return N->getOpcode() == ISD::ADD || isOrEquivalentToAdd(N);
}]>;

let mayLoad = 1 in
def STACK_LOAD
  : NRI<(outs GPR:$dst), (ins GPR:$fi, i256imm:$off), [],
        "STACK_LOAD $dst, $fi, $off">;

let mayStore = 1 in
  def STACK_STORE
    : NRI<(outs), (ins GPR:$fi, i256imm:$off, GPR:$val), [],
          "STACK_STORE $fi, $off, $val">;

def : Pat<(i256 frameindex:$fi), (TargetFI $fi)>;
def : Pat<(load_stack frameindex:$fi), (STACK_LOAD (TargetFI $fi), 0)>;
def : Pat<(load_stack (add_like frameindex:$fi, imm:$off)),
          (STACK_LOAD (TargetFI $fi), imm:$off)>;
def : Pat<(store_stack GPR:$val, frameindex:$fi),
          (STACK_STORE (TargetFI $fi), 0, GPR:$val)>;
def : Pat<(store_stack GPR:$val, (add_like frameindex:$fi, imm:$off)),
          (STACK_STORE (TargetFI $fi), imm:$off, GPR:$val)>;

// The following patterns shouldn't be used for lowering a real code
// generated by FE. We add them only to be able to compile target independent
// LIT tests.
def : Pat<(load_stack GPR:$off), (STACK_LOAD GPR:$off, 0)>;
def : Pat<(store_stack GPR:$val, GPR:$off), (STACK_STORE GPR:$off, 0, GPR:$val)>;
def : Pat<(truncstorei8 GPR:$val, GPR:$off), (STACK_STORE GPR:$off, 0, GPR:$val)>;


//===----------------------------------------------------------------------===//
// EVM instructions for retrieval values from context.
//===----------------------------------------------------------------------===//

defm ADDRESS
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_address))],
      "ADDRESS", " $dst", 0x30, 2>;

defm BALANCE
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_balance GPR:$addr))],
      "BALANCE", " $dst, $addr", 0x31, 100>;

defm ORIGIN
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_origin))],
      "ORIGIN", " $dst", 0x32, 2>;

defm CALLER
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_caller))],
      "CALLER", " $dst", 0x33, 2>;

defm CALLVALUE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_callvalue))],
      "CALLVALUE", " $dst", 0x34, 2>;

let mayLoad = 1 in
defm CALLDATALOAD
  : I<(outs GPR:$dst), (ins GPR:$off),
      [(set GPR:$dst, (int_evm_calldataload GPR:$off))],
      "CALLDATALOAD", " $dst, $off", 0x35, 2>;

def : Pat<(load_call_data GPR:$off), (CALLDATALOAD GPR:$off)>;

defm CALLDATASIZE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_calldatasize))],
      "CALLDATASIZE", " $dst", 0x36, 2>;

let mayStore = 1 in
defm CALLDATACOPY
  : I<(outs), (ins GPR:$dst_off, GPR:$src_off, GPR:$size), [],
      "CALLDATACOPY", " $dst_off, $src_off, $size", 0x37, 3>;

def : Pat<(EVMMemcpy_call_data GPR:$dst, GPR:$src, GPR:$size),
          (CALLDATACOPY GPR:$dst, GPR:$src, GPR:$size)>;

defm CODESIZE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_codesize))],
      "CODESIZE", " $dst", 0x38, 2>;

let mayStore = 1 in
defm CODECOPY
  : I<(outs), (ins GPR:$dst_off, GPR:$src_off, GPR:$size), [],
      "CODECOPY", " $dst_off, $src_off, $size", 0x39, 3>;

def : Pat<(EVMMemcpy_code GPR:$dst, GPR:$src, GPR:$size),
          (CODECOPY GPR:$dst, GPR:$src, GPR:$size)>;

defm GASPRICE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_gasprice))],
      "GASPRICE", " $dst", 0x3A, 2>;

defm EXTCODESIZE
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_extcodesize GPR:$addr))],
      "EXTCODESIZE", " $dst, $addr", 0x3B, 100>;

let mayStore = 1 in
defm EXTCODECOPY
  : I<(outs), (ins GPR:$addr, GPR:$dst_off, GPR:$src_off, GPR:$size),
      [(int_evm_extcodecopy GPR:$addr, GPR:$dst_off, GPR:$src_off, GPR:$size)],
      "EXTCODECOPY", " $addr, $dst_off, $src_off, $size", 0x3C, 100>;

defm RETURNDATASIZE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_returndatasize))],
      "RETURNDATASIZE", " $dst", 0x3D, 2>;

let mayStore = 1 in
defm RETURNDATACOPY
  : I<(outs), (ins GPR:$dst_off, GPR:$src_off, GPR:$size), [],
      "RETURNDATACOPY", " $dst_off, $src_off, $size", 0x3E, 3>;

def : Pat<(EVMMemcpy_return_data GPR:$dst, GPR:$src, GPR:$size),
          (RETURNDATACOPY GPR:$dst, GPR:$src, GPR:$size)>;

defm EXTCODEHASH
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_extcodehash GPR:$addr))],
      "EXTCODEHASH", " $dst, $addr", 0x3F, 100>;

defm BLOCKHASH
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_blockhash GPR:$addr))],
      "BLOCKHASH", " $dst, $addr", 0x40, 20>;

defm COINBASE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_coinbase))],
      "COINBASE", " $dst", 0x41, 2>;

defm TIMESTAMP
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_timestamp))],
      "TIMESTAMP", " $dst", 0x42, 2>;

defm NUMBER
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_number))],
      "NUMBER", " $dst", 0x43, 2>;

defm DIFFICULTY
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_difficulty))],
      "DIFFICULTY", " $dst", 0x44, 2>;

defm GASLIMIT
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_gaslimit))],
      "GASLIMIT", " $dst", 0x45, 2>;

defm CHAINID
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_chainid))],
      "CHAINID", " $dst", 0x46, 2>;

let hasSideEffects = 1 in
defm SELFBALANCE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_selfbalance))],
      "SELFBALANCE", " $dst", 0x47, 5>;

defm BASEFEE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_basefee))],
      "BASEFEE", " $dst", 0x48, 2>;

defm BLOBHASH
  : I<(outs GPR:$dst), (ins GPR:$index),
      [(set GPR:$dst, (int_evm_blobhash GPR:$index))],
      "BLOBHASH", " $dst, $index", 0x49, 3>;

defm BLOBBASEFEE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_blobbasefee))],
      "BLOBBASEFEE", " $dst", 0x4A, 2>;


//===----------------------------------------------------------------------===//
// EVM instructions for logging.
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 1 in {
  defm LOG0
    : I<(outs), (ins GPR:$offset, GPR:$size),
        [(int_evm_log0 GPR:$offset, GPR:$size)],
        "LOG0", " $offset, $size", 0xA0, 375>;

  defm LOG1
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1),
        [(int_evm_log1 GPR:$offset, GPR:$size, GPR:$t1)],
        "LOG1", " $offset, $size, $t1", 0xA1, 750>;

  defm LOG2
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2),
        [(int_evm_log2 GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2)],
        "LOG2", " $offset, $size, $t1, $t2", 0xA2, 1125>;

  defm LOG3
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3),
        [(int_evm_log3 GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3)],
        "LOG3", " $offset, $size, $t1, $t2, $t3", 0xA3, 1500>;

  defm LOG4
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3, GPR:$t4),
        [(int_evm_log4 GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3,
          GPR:$t4)],
        "LOG4", " $offset, $size, $t1, $t2, $t3, $t4", 0xA4, 1875>;
}

//===----------------------------------------------------------------------===//
// EVM system instructions.
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 1 in
defm CREATE
  : I<(outs GPR:$dst), (ins GPR:$value, GPR:$offset, GPR:$size),
      [(set GPR:$dst, (int_evm_create GPR:$value, GPR:$offset, GPR:$size))],
      "CREATE", " $dst, $value, $offset, $size", 0xF0, 32000>;

let isCall = 1, hasSideEffects = 1, mayLoad = 1, mayStore = 1 in {
  defm CALL
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$value, GPR:$arg_off,
                          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_call GPR:$gas, GPR:$addr, GPR:$value,
          GPR:$arg_off, GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "CALL", " $dst, $gas, $addr, $value, $arg_off, $arg_size, $ret_off, $ret_size",
        0xF1, 100>;

  defm CALLCODE
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$value, GPR:$arg_off,
                          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_callcode GPR:$gas, GPR:$addr, GPR:$value,
          GPR:$arg_off, GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "CALLCODE", " $dst, $gas, $addr, $value, $arg_off, $arg_size, $ret_off, $ret_size",
        0xF2, 100>;

  defm DELEGATECALL
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$arg_off, GPR:$arg_size,
                          GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_delegatecall GPR:$gas, GPR:$addr, GPR:$arg_off,
          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "DELEGATECALL", " $dst, $gas, $addr, $arg_off, $arg_size, $ret_off, $ret_size",
        0xF4, 100>;

  defm STATICCALL
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$arg_off, GPR:$arg_size,
                          GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_staticcall GPR:$gas, GPR:$addr, GPR:$arg_off,
          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "STATICCALL", " $dst, $gas, $addr, $arg_off, $arg_size, $ret_off, $ret_size",
        0xFA, 100>;
}

let mayLoad = 1, hasSideEffects = 1 in
defm CREATE2
  : I<(outs GPR:$dst), (ins GPR:$value, GPR:$offset, GPR:$size, GPR:$salt),
      [(set GPR:$dst, (int_evm_create2 GPR:$value, GPR:$offset, GPR:$size,
        GPR:$salt))],
      "CREATE2", " $dst, $value, $offset, $size, $salt", 0xF5, 32000>;


//===----------------------------------------------------------------------===//
// EVM instructions to return with error.
//===----------------------------------------------------------------------===//

let isTerminator = 1, isBarrier = 1, isReturn = 1 in {
  defm REVERT
    : I<(outs), (ins GPR:$offset, GPR:$size),
        [(int_evm_revert GPR:$offset, GPR:$size)],
        "REVERT", " $offset, $size", 0xFD, 0>;

  defm RETURN
    : I<(outs), (ins GPR:$offset, GPR:$size),
        [(int_evm_return GPR:$offset, GPR:$size)],
        "RETURN", " $offset, $size", 0xF3, 0>;
}

let hasSideEffects = 1 in {
  defm SELFDESTRUCT
      : I<(outs), (ins GPR:$addr), [(int_evm_selfdestruct GPR:$addr)],
          "SELFDESTRUCT", " $addr", 0xFF, 5000>;

  let isTerminator = 1, isBarrier = 1, isReturn = 1 in {
    defm STOP : I<(outs), (ins), [(int_evm_stop)], "STOP", "", 0x00, 0>;
    defm INVALID : I<(outs), (ins), [(int_evm_invalid)], "INVALID", "", 0xFE, 0>;
  }
}


//===----------------------------------------------------------------------===//
// EVM instructions for stack manipulation.
//===----------------------------------------------------------------------===//

def POP_KILL : EVMPseudo<(outs), (ins GPR:$val), [(int_evm_pop GPR:$val)]>;

defm POP : I<(outs), (ins), [], "POP", "", 0x50, 2>;

foreach I = {1-16} in {
  defm DUP#I : I<(outs), (ins), [],
                  "DUP"#I, "", !add(I, 0x7F), 3>;
}

foreach I = {1-16} in {
  defm SWAP#I : I<(outs), (ins), [],
                   "SWAP"#I, "", !add(I, 0x8F), 3>;
}

defm PUSH0 : I<(outs), (ins), [], "PUSH0", "", 0x5F, 2>;

def PUSH8_LABEL : NI<(outs), (ins jmptarget:$dst), [], false, "", 0, 0> {
    let isCodeGenOnly = 1;
}

// Define register PUSH* instructions
foreach I = {1 - 32} in {
  def PUSH#I : NI<(outs), (ins i256imm:$imm), [], false, "PUSH"#I#" $imm",
                  !add(I, 0x5F), 3> {
    let BaseName = "PUSH"#I;
    let Size = !add(I, 1);
    let isCodeGenOnly = 1;
  }
}

// Define stack PUSH* instructions
def PUSH1_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH1 $imm",
                  0x60, 3> {
  bits<8> imm;
  let Inst{15-8} = Opc;
  let Inst{7-0} = imm{7-0};
  let Size = 2;
  let BaseName = "PUSH1";
}

def PUSH2_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH2 $imm",
                  0x61, 3> {
  bits<16> imm;
  let Inst{23-16} = Opc;
  let Inst{15-0} = imm{15-0};
  let Size = 3;
  let BaseName = "PUSH2";
}

def PUSH3_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH3 $imm",
                  0x62, 3> {
  bits<24> imm;
  let Inst{31-24} = Opc;
  let Inst{23-0} = imm{23-0};
  let Size = 4;
  let BaseName = "PUSH3";
}

def PUSH4_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH4 $imm",
                  0x63, 3> {
  bits<32> imm;
  let Inst{39-32} = Opc;
  let Inst{31-0} = imm{31-0};
  let Size = 5;
  let BaseName = "PUSH4";
}

def PUSH5_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH5 $imm",
                  0x64, 3> {
  bits<40> imm;
  let Inst{47-40} = Opc;
  let Inst{39-0} = imm{39-0};
  let Size = 6;
  let BaseName = "PUSH5";
}

def PUSH6_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH6 $imm",
                  0x65, 3> {
  bits<48> imm;
  let Inst{55-48} = Opc;
  let Inst{47-0} = imm{47-0};
  let Size = 7;
  let BaseName = "PUSH6";
}

def PUSH7_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH7 $imm",
                  0x66, 3> {
  bits<56> imm;
  let Inst{63-56} = Opc;
  let Inst{55-0} = imm{55-0};
  let Size = 8;
  let BaseName = "PUSH7";
}

def PUSH8_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH8 $imm",
                  0x67, 3> {
  bits<64> imm;
  let Inst{71-64} = Opc;
  let Inst{63-0} = imm{63-0};
  let Size = 9;
  let BaseName = "PUSH8";
}

def PUSH9_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH9 $imm",
                  0x68, 3> {
  bits<72> imm;
  let Inst{79-72} = Opc;
  let Inst{71-0} = imm{71-0};
  let Size = 10;
  let BaseName = "PUSH9";
  let DecoderMethod = "decodePUSH<9>";
}

def PUSH10_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH10 $imm",
                  0x69, 3> {
  bits<80> imm;
  let Inst{87-80} = Opc;
  let Inst{79-0} = imm{79-0};
  let Size = 11;
  let BaseName = "PUSH10";
  let DecoderMethod = "decodePUSH<10>";
}

def PUSH11_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH11 $imm",
                  0x6a, 3> {
  bits<88> imm;
  let Inst{95-88} = Opc;
  let Inst{87-0} = imm{87-0};
  let Size = 12;
  let BaseName = "PUSH11";
  let DecoderMethod = "decodePUSH<11>";
}

def PUSH12_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH12 $imm",
                  0x6b, 3> {
  bits<96> imm;
  let Inst{103-96} = Opc;
  let Inst{95-0} = imm{95-0};
  let Size = 13;
  let BaseName = "PUSH12";
  let DecoderMethod = "decodePUSH<12>";
}

def PUSH13_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH13 $imm",
                  0x6c, 3> {
  bits<104> imm;
  let Inst{111-104} = Opc;
  let Inst{103-0} = imm{103-0};
  let Size = 14;
  let BaseName = "PUSH13";
  let DecoderMethod = "decodePUSH<13>";
}

def PUSH14_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH14 $imm",
                  0x6d, 3> {
  bits<112> imm;
  let Inst{119-112} = Opc;
  let Inst{111-0} = imm{111-0};
  let Size = 15;
  let BaseName = "PUSH14";
  let DecoderMethod = "decodePUSH<14>";
}

def PUSH15_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH15 $imm",
                  0x6e, 3> {
  bits<120> imm;
  let Inst{127-120} = Opc;
  let Inst{119-0} = imm{119-0};
  let Size = 16;
  let BaseName = "PUSH15";
  let DecoderMethod = "decodePUSH<15>";
}

def PUSH16_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH16 $imm",
                  0x6f, 3> {
  bits<128> imm;
  let Inst{135-128} = Opc;
  let Inst{127-0} = imm{127-0};
  let Size = 17;
  let BaseName = "PUSH16";
  let DecoderMethod = "decodePUSH<16>";
}

def PUSH17_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH17 $imm",
                  0x70, 3> {
  bits<136> imm;
  let Inst{143-136} = Opc;
  let Inst{135-0} = imm{135-0};
  let Size = 18;
  let BaseName = "PUSH17";
  let DecoderMethod = "decodePUSH<17>";
}

def PUSH18_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH18 $imm",
                  0x71, 3> {
  bits<144> imm;
  let Inst{151-144} = Opc;
  let Inst{143-0} = imm{143-0};
  let Size = 19;
  let BaseName = "PUSH18";
  let DecoderMethod = "decodePUSH<18>";
}

def PUSH19_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH19 $imm",
                  0x72, 3> {
  bits<152> imm;
  let Inst{159-152} = Opc;
  let Inst{151-0} = imm{151-0};
  let Size = 20;
  let BaseName = "PUSH19";
  let DecoderMethod = "decodePUSH<19>";
}

def PUSH20_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH20 $imm",
                  0x73, 3> {
  bits<160> imm;
  let Inst{167-160} = Opc;
  let Inst{159-0} = imm{159-0};
  let Size = 21;
  let BaseName = "PUSH20";
  let DecoderMethod = "decodePUSH<20>";
}

def PUSH21_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH21 $imm",
                  0x74, 3> {
  bits<168> imm;
  let Inst{175-168} = Opc;
  let Inst{167-0} = imm{167-0};
  let Size = 22;
  let BaseName = "PUSH21";
  let DecoderMethod = "decodePUSH<21>";
}

def PUSH22_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH22 $imm",
                  0x75, 3> {
  bits<176> imm;
  let Inst{183-176} = Opc;
  let Inst{175-0} = imm{175-0};
  let Size = 23;
  let BaseName = "PUSH22";
  let DecoderMethod = "decodePUSH<22>";
}

def PUSH23_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH23 $imm",
                  0x76, 3> {
  bits<184> imm;
  let Inst{191-184} = Opc;
  let Inst{183-0} = imm{183-0};
  let Size = 24;
  let BaseName = "PUSH23";
  let DecoderMethod = "decodePUSH<23>";
}

def PUSH24_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH24 $imm",
                  0x77, 3> {
  bits<192> imm;
  let Inst{199-192} = Opc;
  let Inst{191-0} = imm{191-0};
  let Size = 25;
  let BaseName = "PUSH24";
  let DecoderMethod = "decodePUSH<24>";
}

def PUSH25_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH25 $imm",
                  0x78, 3> {
  bits<200> imm;
  let Inst{207-200} = Opc;
  let Inst{199-0} = imm{199-0};
  let Size = 26;
  let BaseName = "PUSH25";
  let DecoderMethod = "decodePUSH<25>";
}

def PUSH26_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH26 $imm",
                  0x79, 3> {
  bits<208> imm;
  let Inst{215-208} = Opc;
  let Inst{207-0} = imm{207-0};
  let Size = 27;
  let BaseName = "PUSH26";
  let DecoderMethod = "decodePUSH<26>";
}

def PUSH27_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH27 $imm",
                  0x7a, 3> {
  bits<216> imm;
  let Inst{223-216} = Opc;
  let Inst{215-0} = imm{215-0};
  let Size = 28;
  let BaseName = "PUSH27";
  let DecoderMethod = "decodePUSH<27>";
}

def PUSH28_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH28 $imm",
                  0x7b, 3> {
  bits<224> imm;
  let Inst{231-224} = Opc;
  let Inst{223-0} = imm{223-0};
  let Size = 29;
  let BaseName = "PUSH28";
  let DecoderMethod = "decodePUSH<28>";
}

def PUSH29_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH29 $imm",
                  0x7c, 3> {
  bits<232> imm;
  let Inst{239-232} = Opc;
  let Inst{231-0} = imm{231-0};
  let Size = 30;
  let BaseName = "PUSH29";
  let DecoderMethod = "decodePUSH<29>";
}

def PUSH30_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH30 $imm",
                  0x7d, 3> {
  bits<240> imm;
  let Inst{247-240} = Opc;
  let Inst{239-0} = imm{239-0};
  let Size = 31;
  let BaseName = "PUSH30";
  let DecoderMethod = "decodePUSH<30>";
}

def PUSH31_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH31 $imm",
                  0x7e, 3> {
  bits<248> imm;
  let Inst{255-248} = Opc;
  let Inst{247-0} = imm{247-0};
  let Size = 32;
  let BaseName = "PUSH31";
  let DecoderMethod = "decodePUSH<31>";
}

def PUSH32_S : NI<(outs), (ins i256imm:$imm), [], true, "PUSH32 $imm",
                  0x7f, 3> {
  bits<256> imm;
  let Inst{263-256} = Opc;
  let Inst{255-0} = imm{255-0};
  let Size = 33;
  let BaseName = "PUSH32";
  let DecoderMethod = "decodePUSH<32>";
}

// Pseudo instructions for linkage
let isCodeGenOnly = 1 in
defm DATA
  : I<(outs GPR:$dst), (ins jmptarget:$reloc), [], "",
       "", 0, 0>;
