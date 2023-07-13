//=--------- EVMInstrFormats.td - EVM Instr. Formats -*- tablegen -*---------=//
//
// This file describes the EVM instruction formats in TableGen format.n
//
//===----------------------------------------------------------------------===//

class ASList<list<int> lst> {
  list<int> List = lst;
}

// The 'AS' structure is defined in IntrinsicsEVM.td
// For each address space define a list containing the one address space
// to override the AddressSpaces property of the PatFrag.
def AS_stack  : ASList<[AS.STACK]>;
def AS_heap : ASList<[AS.HEAP]>;
def AS_call_data : ASList<[AS.CALL_DATA]>;
def AS_return_data : ASList<[AS.RETURN_DATA]>;

// EVM general instruction format.
class EVMInst<bits<8> inst, string asmstr, int cost>
  : Instruction {
  bits<8> Inst    = inst; // Instruction encoding.
  int GasCost     = cost;
  let Namespace   = "EVM";
  let Pattern     = [];
  let AsmString   = asmstr;
}

// Normal instructions. Default instantiation of a EVMInst.
class NI<dag oops, dag iops, list<dag> pattern,
         string asmstr = "", bits<8> inst = 0, int cost = 0>
    : EVMInst<inst, asmstr, cost> {
  dag OutOperandList = oops;
  dag InOperandList  = iops;
  let Pattern        = pattern;
  let GasCost        = cost;
  let Defs           = [ARGUMENTS];
}

// TODO: When implementing instructions stackification, this class
// will be extendent to define both register and stack-based instructions.
// As of now, all the instructions should be derived from it, which implies
// definition of only register-based ones.
multiclass I<dag oops_r, dag iops_r, list<dag> pattern_r,
             string asmstr_r = "", bits<8> inst = 0, int cost = 0> {
  def "" : NI<oops_r, iops_r, pattern_r, asmstr_r, inst, cost>;
}

class EVMPseudo<dag oops, dag iops, list<dag> pattern>
  : NI<oops, iops, pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
