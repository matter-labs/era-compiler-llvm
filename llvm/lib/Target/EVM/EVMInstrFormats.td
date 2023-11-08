//=--------- EVMInstrFormats.td - EVM Instr. Formats -*- tablegen -*---------=//
//
// This file describes the EVM instruction formats in TableGen format.n
//
//===----------------------------------------------------------------------===//

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
