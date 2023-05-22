//===-------- EVMInstrInfo.td - EVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the EVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "EVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// EVM-specific DAG Node Types.
//===----------------------------------------------------------------------===//

def SDT_EVMArgument   : SDTypeProfile<1, 1, [SDTCisVT<1, i256>]>;
def SDT_EVMReturn     : SDTypeProfile<0, -1, []>;
def SDT_EVMSignextend : SDTypeProfile<1, 2, [SDTCisSameAs<0, 1>,
                                             SDTCisVT<1, i256>]>;


//===----------------------------------------------------------------------===//
// EVM-specific DAG Nodes.
//===----------------------------------------------------------------------===//

def EVMargument : SDNode<"EVMISD::ARGUMENT", SDT_EVMArgument>;

def EVMreturn   : SDNode<"EVMISD::RETURN", SDT_EVMReturn,
                         [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def EVMSignextend: SDNode<"EVMISD::SIGNEXTEND", SDT_EVMSignextend>;

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
// EVM Instructions list.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Additional instructions.
//===----------------------------------------------------------------------===//

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
def : Pat<(sext_inreg GPR:$src, i8), (SIGNEXTEND 0, GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i16), (SIGNEXTEND 1, GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i32), (SIGNEXTEND 3, GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i64), (SIGNEXTEND 7, GPR:$src)>;
def : Pat<(sext_inreg GPR:$src, i128), (SIGNEXTEND 15, GPR:$src)>;

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


//===----------------------------------------------------------------------===//
// EVM shift instructions.
//===----------------------------------------------------------------------===//

defm SHL : BinaryInst<shl, "SHL", 0x1b, 3>;
defm SHR : BinaryInst<srl, "SHR", 0x1c, 3>;
defm SAR : BinaryInst<sra, "SAR", 0x1d, 3>;


//===----------------------------------------------------------------------===//
// EVM control instructions.
//===----------------------------------------------------------------------===//

let isBarrier = 1, isTerminator = 1, isReturn = 1, isBranch = 1 in
defm RETURN
  : I<(outs), (ins variable_ops), [(EVMreturn)], "RETURN", 0xF3, 0>;
