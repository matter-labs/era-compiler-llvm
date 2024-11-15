//=--------- EVMInstrFormats.td - EVM Instr. Formats -*- tablegen -*---------=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the EVM instruction formats in TableGen format.n
//
//===----------------------------------------------------------------------===//

class ASList<list<int> lst> {
  list<int> List = lst;
}

// The 'AS' structure is defined in IntrinsicsEVM.td
// For each address space define a list containing the only address space
// to override the AddressSpaces property of the PatFrag.
def AS_stack  : ASList<[AS.STACK]>;
def AS_heap : ASList<[AS.HEAP]>;
def AS_call_data : ASList<[AS.CALL_DATA]>;
def AS_return_data : ASList<[AS.RETURN_DATA]>;
def AS_storage : ASList<[AS.STORAGE]>;
def AS_tstorage : ASList<[AS.TSTORAGE]>;

// EVM general instruction format.
class EVMInst<string asmstr, bit stack, int cost>
  : StackRel, RegisterRel, Instruction {
  // Instruction encoding. Bitwidth corresponds to the maximum
  // size of possible EVM insturction.
  // This is 'PUSH32 Imm'. 8 bits for opcode, and 256 bits for
  // Imm.
  bits<264> Inst;
  bits<8> Opc;
  bit StackBased = stack;
  string BaseName = NAME;
  int GasCost     = cost;
  let Namespace   = "EVM";
  let Pattern     = [];
  let AsmString   = asmstr;
  let TSFlags{0}  = stack;
}

// Normal instructions. Default instantiation of a EVMInst.
class NI<dag oops, dag iops, list<dag> pattern, bit stack,
         string asmstr = "", bits<8> inst = 0, int cost = 0>
    : EVMInst<asmstr, stack, cost> {
  dag OutOperandList = oops;
  dag InOperandList  = iops;
  let Pattern        = pattern;
  let Size           = 1;
  let Opc            = inst;
  let Inst{7-0}      = Opc;
  let GasCost        = cost;
  let Defs           = [ARGUMENTS];
}

// Generates both register and stack based versions of one actual instruction.
multiclass I<dag oops_r, dag iops_r, list<dag> pattern_r,
             string opcstr, string argstr_r, bits<8> inst,
             int cost = 0, dag oops_s = (outs), dag iops_s = (ins), string argstr_s = ""> {
  let isCodeGenOnly = 1 in
  def "" : NI<oops_r, iops_r, pattern_r, false, opcstr#argstr_r, inst, cost>;
  let BaseName = NAME, Defs = []<Register> in
  def _S : NI<oops_s, iops_s, [], true, opcstr#argstr_s, inst, cost>;
}

// For pseudo instructions that have no real counterparts. These instructions
// are used in codegen only and print out to assembly if "virtual register" mode
// is on.
let isCodeGenOnly = 1 in
class NRI<dag oops, dag iops, list<dag> pattern, string asmstr>
  : NI<oops, iops, pattern, false, asmstr> {
}

class EVMPseudo<dag oops, dag iops, list<dag> pattern>
  : NI<oops, iops, pattern, false> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
