//===-------- EVMInstrInfo.td - EVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the EVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "EVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// EVM-specific DAG Node Types.
//===----------------------------------------------------------------------===//

def SDT_EVMCall0
  : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;

def SDT_EVMCall1
  : SDTypeProfile<1, -1, [SDTCisPtrTy<1>]>;

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

def EVMcall0
  : SDNode<"EVMISD::CALL0", SDT_EVMCall0,
            [SDNPHasChain, SDNPVariadic]>;

def EVMcall1
  : SDNode<"EVMISD::CALL1", SDT_EVMCall1,
            [SDNPHasChain, SDNPVariadic]>;

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
foreach AS = ["stack", "heap", "call_data"] in {
  let AddressSpaces = !cast<ASList>("AS_"#AS).List in {
    def load_#AS : PatFrag<(ops node:$ptr), (load node:$ptr)> {
      let IsLoad = 1;
    }
  }
}

// Store patterns for Stack and Memory
foreach AS = ["stack", "heap"] in {
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
  : NI<(outs), (ins i256imm:$amt1, i256imm:$amt2),
       [(EVMcallseq_start timm:$amt1, timm:$amt2)],
       "#ADJCALLSTACKDOWN $amt1 $amt2">;

def ADJCALLSTACKUP
  : NI<(outs), (ins i256imm:$amt1, i256imm:$amt2),
       [(EVMcallseq_end timm:$amt1, timm:$amt2)],
       "#ADJCALLSTACKUP $amt1 $amt2">;
}

let hasSideEffects = 1, Uses = [ARGUMENTS] in
def ARGUMENT
  : NI<(outs GPR:$res), (ins i256imm:$argno),
       [(set GPR:$res, (EVMargument timm:$argno))],
       "ARGUMENT $res, $argno">;

// This is not real EVM instruction. It should be eliminted while
// stackification.
let isMoveImm = 1, isAsCheapAsAMove = 1, isReMaterializable = 1 in
defm CONST_I256
  : I<(outs GPR:$res), (ins i256imm:$imm), [(set GPR:$res, imm:$imm)],
      "CONST_I256 $res, $imm">;

// This is not real EVM instruction. It should be eliminted while
// stackification.
let isAsCheapAsAMove = 1 in
defm COPY_I256
  : I<(outs GPR:$res), (ins GPR:$src), [], "COPY_I256 $res, $src">;

// There is no 'call' instruction in EVM; the 'jump' is used instead.
// The CALL* are not real EVM instructions. They should be replaced
// with the jump while stackification.

let isCall = 1, hasCtrlDep = 1 in {
def CALL0
  : NI<(outs), (ins GPR:$callee, variable_ops),
       [(EVMcall0 tglobaladdr:$callee)],
       "CALL0 $callee">;

def CALL1
  : NI<(outs GPR:$res), (ins GPR:$callee, variable_ops),
       [(set GPR:$res, (EVMcall1 tglobaladdr:$callee))],
       "CALL1 $res, $callee">;
}

def : Pat<(EVMcall0 GPR:$addr), (CALL0 GPR:$addr)>;
def : Pat<(EVMcall1 GPR:$addr), (CALL1 GPR:$addr)>;
def : Pat<(i256 (EVMTargetAddrWrapper tglobaladdr:$addr)),
          (CONST_I256 tglobaladdr:$addr)>;
def : Pat<(i256 (EVMTargetAddrWrapper texternalsym:$addr)),
          (CONST_I256 texternalsym:$addr)>;


//===----------------------------------------------------------------------===//
// EVM arithmetic instructions.
//===----------------------------------------------------------------------===//

multiclass BinaryInst<SDNode node, string opcodeStr, bits<8> inst, int cost>
  : I<(outs GPR:$dst), (ins GPR:$lhs, GPR:$rhs),
      [(set GPR:$dst, (node GPR:$lhs, GPR:$rhs))],
      opcodeStr#" $dst, $lhs, $rhs", inst, cost>;

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
      "ADDMOD $dst, $add_op1, $add_op2, $denom", 0x08, 8>;

defm MULMOD
  : I<(outs GPR:$dst), (ins GPR:$mul_op1, GPR:$mul_op2, GPR:$denom),
      [(set GPR:$dst,
       (int_evm_mulmod GPR:$mul_op1, GPR:$mul_op2, GPR:$denom))],
      "MULMOD $dst, $mul_op1, $mul_op2, $denom", 0x09, 8>;

defm EXP
  : I<(outs GPR:$dst), (ins GPR:$base, GPR:$exp),
      [(set GPR:$dst, (int_evm_exp GPR:$base, GPR:$exp))],
      "EXP $dst, $base, $exp", 0x0A, 10>;

defm SIGNEXTEND
  : I<(outs GPR:$dst), (ins GPR:$size, GPR:$src), [],
      "SIGNEXTEND $dst, $size, $src", 0x0B, 5>;

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


//===----------------------------------------------------------------------===//
// EVM comparison instructions.
//===----------------------------------------------------------------------===//

multiclass ComparisonInst<CondCode cond, string opcodeStr,
                          bits<8> inst, int cost>
  : I<(outs GPR:$dst), (ins GPR:$lhs, GPR:$rhs),
      [(set GPR:$dst, (setcc GPR:$lhs, GPR:$rhs, cond))],
      opcodeStr#" $dst, $lhs, $rhs", inst, cost>;

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
      "ISZERO $dst, $src", 0x15, 3>;
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
      "NOT $dst, $src", 0x19, 3>;

let mayLoad = 1 in
defm SHA3
  : I<(outs GPR:$dst), (ins GPR:$offset, GPR:$size),
      [(set GPR:$dst, (int_evm_sha3 GPR:$offset, GPR:$size))],
      "SHA3 $dst, $offset, $size", 0x20, 30>;


//===----------------------------------------------------------------------===//
// EVM shift instructions.
//===----------------------------------------------------------------------===//

defm SHL : BinaryInst<shl, "SHL", 0x1b, 3>;
defm SHR : BinaryInst<srl, "SHR", 0x1c, 3>;
defm SAR : BinaryInst<sra, "SAR", 0x1d, 3>;


//===----------------------------------------------------------------------===//
// EVM control instructions.
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1 in {
// The condition operand is a boolean value which EVM represents as i256.
defm JUMPI
  : I<(outs), (ins jmptarget:$dst, GPR:$cond), [(brcond GPR:$cond, bb:$dst)],
      "JUMPI $dst, $cond", 0x56, 10>;

let isBarrier = 1 in
defm JUMP
  : I<(outs), (ins jmptarget:$dst), [(br bb:$dst)], "JUMP $dst", 0x57, 10>;
} // isBranch = 1, isTerminator = 1

// This isn't really a control flow instruction, but it should be used to mark
// destination of jump instructions.
defm : I<(outs), (ins), [], "JUMPDEST", 0x57, 10>;

let isBarrier = 1, isTerminator = 1, isReturn = 1 in
def RET : NI<(outs), (ins variable_ops), [(EVMret)], "RET">;


//===----------------------------------------------------------------------===//
// EVM memory/storage instructions.
//===----------------------------------------------------------------------===//

let mayLoad = 1 in
defm MLOAD
  : I<(outs GPR:$dst), (ins GPR:$offset),
      [(set GPR:$dst, (load_heap GPR:$offset))],
      "MLOAD $dst, $offset", 0x51, 3>;

let mayStore = 1, hasSideEffects = 1 in {
  defm MSTORE
    : I<(outs), (ins GPR:$offset, GPR:$val),
        [(store_heap GPR:$val, GPR:$offset)],
        "MSTORE $offset, $val", 0x52, 3>;

  defm MSTORE8
    : I<(outs), (ins GPR:$offset, GPR:$val),
        [(int_evm_mstore8 GPR:$offset, GPR:$val)],
        "MSTORE8 $offset, $val", 0x53, 3>;
}

defm SLOAD
  : I<(outs GPR:$dst), (ins GPR:$key),
      [(set GPR:$dst, (int_evm_sload GPR:$key))],
      "SLOAD $dst, $key", 0x54, 100>;

let hasSideEffects = 1 in {
  defm SSTORE
    : I<(outs), (ins GPR:$key, GPR:$val), [(int_evm_sstore GPR:$key, GPR:$val)],
        "SSTORE $key, $val", 0x55, 100>;

  defm PC : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_pc))],
               "PC $dst", 0x58, 2>;

  defm GAS : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_gas))],
                "GAS $dst", 0x5A, 2>;
}

defm MSIZE : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_msize))],
                "MSIZE $dst", 0x59, 2>;

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
  : NI<(outs GPR:$dst), (ins GPR:$fi), [], "STACK_LOAD $dst, $fi">;

let mayStore = 1, hasSideEffects = 1 in
  def STACK_STORE
    : NI<(outs), (ins GPR:$fi, GPR:$val), [], "STACK_STORE $fi, $val">;

def : Pat<(i256 frameindex:$fi), (TargetFI $fi)>;
def : Pat<(load_stack frameindex:$fi), (STACK_LOAD (TargetFI $fi))>;
def : Pat<(load_stack (add_like frameindex:$fi, GPR:$off)),
          (STACK_LOAD (ADD (TargetFI $fi), GPR:$off))>;
def : Pat<(store_stack GPR:$val, frameindex:$fi),
          (STACK_STORE (TargetFI $fi), GPR:$val)>;
def : Pat<(store_stack GPR:$val, (add_like frameindex:$fi, GPR:$off)),
          (STACK_STORE (ADD (TargetFI $fi), GPR:$off), GPR:$val)>;

// The following patterns shouldn't be used for lowering a real code
// generated by FE. We add them only to be able to compile target independent
// LIT tests.
def : Pat<(load_stack GPR:$off), (STACK_LOAD GPR:$off)>;
def : Pat<(store_stack GPR:$val, GPR:$off), (STACK_STORE GPR:$off, GPR:$val)>;
def : Pat<(truncstorei8 GPR:$val, GPR:$off), (STACK_STORE GPR:$off, GPR:$val)>;


//===----------------------------------------------------------------------===//
// EVM instructions for retrieval values from context.
//===----------------------------------------------------------------------===//

defm ADDRESS
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_address))],
      "ADDRESS $dst", 0x30, 2>;

defm BALANCE
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_balance GPR:$addr))],
      "BALANCE $dst, $addr", 0x31, 100>;

defm ORIGIN
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_origin))],
      "ORIGIN $dst", 0x32, 2>;

defm CALLER
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_caller))],
      "CALLER $dst", 0x32, 2>;

defm CALLVALUE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_callvalue))],
      "CALLVALUE $dst", 0x34, 2>;

defm CALLDATALOAD
  : I<(outs GPR:$dst), (ins GPR:$off),
      [(set GPR:$dst, (int_evm_calldataload GPR:$off))],
      "CALLDATALOAD $dst, $off", 0x35, 2>;

def : Pat<(load_call_data GPR:$off), (CALLDATALOAD GPR:$off)>;

defm CALLDATASIZE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_calldatasize))],
      "CALLDATASIZE $dst", 0x36, 2>;

let mayStore = 1, hasSideEffects = 1 in
defm CALLDATACOPY
  : I<(outs), (ins GPR:$dst_off, GPR:$src_off, GPR:$size),
      [(int_evm_calldatacopy GPR:$dst_off, GPR:$src_off, GPR:$size)],
      "CALLDATACOPY $dst_off, $src_off, $size", 0x37, 3>;

def : Pat<(EVMMemcpy_call_data GPR:$dst, GPR:$src, GPR:$size),
          (CALLDATACOPY GPR:$dst, GPR:$src, GPR:$size)>;

defm CODESIZE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_codesize))],
      "CODESIZE $dst", 0x38, 2>;

let mayStore = 1, hasSideEffects = 1 in
defm CODECOPY
  : I<(outs), (ins GPR:$dst_off, GPR:$src_off, GPR:$size),
      [(int_evm_codecopy GPR:$dst_off, GPR:$src_off, GPR:$size)],
      "CODECOPY $dst_off, $src_off, $size", 0x39, 3>;

def : Pat<(EVMMemcpy_code GPR:$dst, GPR:$src, GPR:$size),
          (CODECOPY GPR:$dst, GPR:$src, GPR:$size)>;

defm GASPRICE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_gasprice))],
      "GASPRICE $dst", 0x3A, 2>;

defm EXTCODESIZE
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_extcodesize GPR:$addr))],
      "EXTCODESIZE $dst, $addr", 0x3B, 100>;

let mayStore = 1, hasSideEffects = 1 in
defm EXTCODECOPY
  : I<(outs), (ins GPR:$addr, GPR:$dst_off, GPR:$src_off, GPR:$size),
      [(int_evm_extcodecopy GPR:$addr, GPR:$dst_off, GPR:$src_off, GPR:$size)],
      "EXTCODECOPY $addr, $dst_off, $src_off, $size", 0x3C, 100>;

defm RETURNDATASIZE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_returndatasize))],
      "RETURNDATASIZE $dst", 0x3D, 2>;

let mayStore = 1, hasSideEffects = 1 in
defm RETURNDATACOPY
  : I<(outs), (ins GPR:$dst_off, GPR:$src_off, GPR:$size),
      [(int_evm_returndatacopy GPR:$dst_off, GPR:$src_off, GPR:$size)],
      "RETURNDATACOPY $dst_off, $src_off, $size", 0x3E, 3>;

def : Pat<(EVMMemcpy_return_data GPR:$dst, GPR:$src, GPR:$size),
          (RETURNDATACOPY GPR:$dst, GPR:$src, GPR:$size)>;

defm EXTCODEHASH
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_extcodehash GPR:$addr))],
      "EXTCODEHASH $dst, $addr", 0x3F, 100>;

defm BLOCKHASH
  : I<(outs GPR:$dst), (ins GPR:$addr),
      [(set GPR:$dst, (int_evm_blockhash GPR:$addr))],
      "BLOCKHASH $dst, $addr", 0x40, 20>;

defm COINBASE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_coinbase))],
      "COINBASE $dst", 0x41, 2>;

defm TIMESTAMP
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_timestamp))],
      "TIMESTAMP $dst", 0x42, 2>;

defm NUMBER
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_number))],
      "NUMBER $dst", 0x43, 2>;

defm DIFFICULTY
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_difficulty))],
      "DIFFICULTY $dst", 0x44, 2>;

defm GASLIMIT
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_gaslimit))],
      "GASLIMIT $dst", 0x45, 2>;

defm CHAINID
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_chainid))],
      "CHAINID $dst", 0x46, 2>;

let hasSideEffects = 1 in
defm SELFBALANCE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_selfbalance))],
      "SELFBALANCE $dst", 0x47, 5>;

defm BASEFEE
  : I<(outs GPR:$dst), (ins), [(set GPR:$dst, (int_evm_basefee))],
      "BASEFEE $dst", 0x48, 2>;


//===----------------------------------------------------------------------===//
// EVM instructions for logging.
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 1 in {
  defm LOG0
    : I<(outs), (ins GPR:$offset, GPR:$size),
        [(int_evm_log0 GPR:$offset, GPR:$size)],
        "LOG0 $offset, $size", 0xA0, 375>;

  defm LOG1
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1),
        [(int_evm_log1 GPR:$offset, GPR:$size, GPR:$t1)],
        "LOG1 $offset, $size, $t1", 0xA1, 750>;

  defm LOG2
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2),
        [(int_evm_log2 GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2)],
        "LOG2 $offset, $size, $t1, $t2", 0xA2, 1125>;

  defm LOG3
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3),
        [(int_evm_log3 GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3)],
        "LOG3 $offset, $size, $t1, $t2, $t3", 0xA3, 1500>;

  defm LOG4
    : I<(outs), (ins GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3, GPR:$t4),
        [(int_evm_log4 GPR:$offset, GPR:$size, GPR:$t1, GPR:$t2, GPR:$t3,
          GPR:$t4)],
        "LOG4 $offset, $size, $t1, $t2, $t3, $t4", 0xA4, 1875>;
}

//===----------------------------------------------------------------------===//
// EVM system instructions.
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 1 in
defm CREATE
  : I<(outs GPR:$dst), (ins GPR:$value, GPR:$offset, GPR:$size),
      [(set GPR:$dst, (int_evm_create GPR:$value, GPR:$offset, GPR:$size))],
      "CREATE $dst, $value, $offset, $size", 0xF0, 32000>;

let isCall = 1, hasSideEffects = 1, mayLoad = 1, mayStore = 1 in {
  defm CALL
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$value, GPR:$arg_off,
                          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_call GPR:$gas, GPR:$addr, GPR:$value,
          GPR:$arg_off, GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "CALL $dst, $gas, $addr, $value, $arg_off, $arg_size, $ret_off, $ret_size",
        0xF1, 100>;

  defm CALLCODE
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$value, GPR:$arg_off,
                          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_callcode GPR:$gas, GPR:$addr, GPR:$value,
          GPR:$arg_off, GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "CALLCODE $dst, $gas, $addr, $value, $arg_off, $arg_size, $ret_off, $ret_size",
        0xF2, 100>;

  defm DELEGATECALL
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$arg_off, GPR:$arg_size,
                          GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_delegatecall GPR:$gas, GPR:$addr, GPR:$arg_off,
          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "DELEGATECALL $dst, $gas, $addr, $arg_off, $arg_size, $ret_off, $ret_size",
        0xF4, 100>;

  defm STATICCALL
    : I<(outs GPR:$dst), (ins GPR:$gas, GPR:$addr, GPR:$arg_off, GPR:$arg_size,
                          GPR:$ret_off, GPR:$ret_size),
        [(set GPR:$dst, (int_evm_staticcall GPR:$gas, GPR:$addr, GPR:$arg_off,
          GPR:$arg_size, GPR:$ret_off, GPR:$ret_size))],
        "STATICCALL $dst, $gas, $addr, $arg_off, $arg_size, $ret_off, $ret_size",
        0xFA, 100>;
}

let mayLoad = 1, hasSideEffects = 1 in
defm CREATE2
  : I<(outs GPR:$dst), (ins GPR:$value, GPR:$offset, GPR:$size, GPR:$salt),
      [(set GPR:$dst, (int_evm_create2 GPR:$value, GPR:$offset, GPR:$size,
        GPR:$salt))],
      "CREATE2 $dst, $value, $offset, $size, $salt", 0xF5, 32000>;


//===----------------------------------------------------------------------===//
// EVM instructions to return with error.
//===----------------------------------------------------------------------===//

let isTerminator = 1, isBarrier = 1 in {
  defm REVERT
    : I<(outs), (ins GPR:$offset, GPR:$size),
        [(int_evm_revert GPR:$offset, GPR:$size)],
        "REVERT $offset, $size", 0xFD, 0>;

  defm RETURN
    : I<(outs), (ins GPR:$offset, GPR:$size),
        [(int_evm_return GPR:$offset, GPR:$size)],
        "RETURN $offset, $size", 0xF3, 0>;
}

let hasSideEffects = 1 in {
  defm SELFDESTRUCT
      : I<(outs), (ins GPR:$addr), [(int_evm_selfdestruct GPR:$addr)],
          "SELFDESTRUCT $addr", 0xFF, 5000>;

  defm STOP : I<(outs), (ins), [(int_evm_stop)], "STOP", 0x00, 0>;

  defm INVALID : I<(outs), (ins), [(int_evm_invalid)], "INVALID", 0xFE, 0>;
}
