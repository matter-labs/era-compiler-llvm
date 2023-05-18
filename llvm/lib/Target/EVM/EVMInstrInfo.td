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


//===----------------------------------------------------------------------===//
// EVM-specific DAG Nodes.
//===----------------------------------------------------------------------===//

def EVMargument : SDNode<"EVMISD::ARGUMENT", SDT_EVMArgument>;

def EVMreturn   : SDNode<"EVMISD::RETURN", SDT_EVMReturn,
                         [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;


//===----------------------------------------------------------------------===//
// EVM Operand Definitions.
//===----------------------------------------------------------------------===//

let OperandType = "OPERAND_IMMEDIATE" in
def i256imm : Operand<i256>;


//===----------------------------------------------------------------------===//
// EVM Instructions list.
//===----------------------------------------------------------------------===//

// Additional instructions

let hasSideEffects = 1, Uses = [ARGUMENTS] in
def ARGUMENT : NI<(outs GPR:$res), (ins i256imm:$argno),
                  [(set GPR:$res, (EVMargument timm:$argno))],
		  "ARGUMENT $res, $argno", 0, 0>;


// Arithmetic instructions

multiclass BinaryInst<SDNode node, string opcodeStr, bits<8> inst, int cost>
    : I<(outs GPR:$dst), (ins GPR:$lhs, GPR:$rhs),
        [(set GPR:$dst, (node GPR:$lhs, GPR:$rhs))],
        opcodeStr#" $dst, $lhs, $rhs", inst, cost>;

let isCommutable = 1 in {
  defm ADD : BinaryInst<add, "ADD", 0x01, 3>;
  defm MUL : BinaryInst<mul, "MUL", 0x02, 5>;
}
defm SUB : BinaryInst<sub, "SUB", 0x03, 3>;


// Control instructions

let isBarrier = 1, isTerminator = 1, isReturn = 1, isBranch = 1 in
defm RETURN : I<(outs), (ins variable_ops), [(EVMreturn)],
               "RETURN", 0xF3, 0>;
