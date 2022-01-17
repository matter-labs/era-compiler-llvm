//===-- SyncVMInstrInfo.td - SyncVM Instruction defs -------*- tablegen -*-===//
//
// This file describes the SyncVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "SyncVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// Type Profiles.
//===----------------------------------------------------------------------===//
def SDT_SyncVMCall         : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def SDT_SyncVMCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i256>,
                                             SDTCisVT<1, i256>]>;
def SDT_SyncVMCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i256>, SDTCisVT<1, i256>]>;
def SDT_SyncVMPush         : SDTypeProfile<0,  2, [SDTCisVT<0, i256>,
                                                   SDTCisVT<1, i256>]>;
def SDT_SyncVMPop          : SDTypeProfile<1,  1, [SDTCisVT<0, i256>]>;
def SDT_SyncVMBrCC         : SDTypeProfile<0,  3, [SDTCisVT<0, OtherVT>,
                                                   SDTCisVT<1, OtherVT>,
                                                   SDTCisVT<2, i256>]>;
def SDT_SyncVMSelectCC     : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                  SDTCisSameAs<1, 2>,
                                                  SDTCisVT<3, i256>]>;
def SDT_SyncVMCmp          : SDTypeProfile<0, 2, []>;

//===----------------------------------------------------------------------===//
// SyncVM Specific Node Definitions.
//===----------------------------------------------------------------------===//

def SyncVMret    : SDNode<"SyncVMISD::RET", SDTNone,
                    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMcall   : SDNode<"SyncVMISD::CALL", SDT_SyncVMCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMbrcc   : SDNode<"SyncVMISD::BR_CC", SDT_SyncVMBrCC,
                    [SDNPHasChain, SDNPInGlue]>;
def SyncVMselectcc: SDNode<"SyncVMISD::SELECT_CC", SDT_SyncVMSelectCC,
                           [SDNPInGlue]>;
def SyncVMcmp    : SDNode<"SyncVMISD::CMP", SDT_SyncVMCmp, [SDNPOutGlue]>;
def SyncVMcallseq_start :
                 SDNode<"ISD::CALLSEQ_START", SDT_SyncVMCallSeqStart,
                        [SDNPHasChain, SDNPOutGlue]>;
def SyncVMcallseq_end :
                 SDNode<"ISD::CALLSEQ_END",   SDT_SyncVMCallSeqEnd,
                        [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def SyncVMpush : SDNode<"SyncVMISD::PUSH", SDT_SyncVMPush,
                        [SDNPHasChain, SDNPInGlue, SDNPOutGlue]>;
def SyncVMpop : SDNode<"SyncVMISD::POP",   SDT_SyncVMPop,
                       [SDNPHasChain, SDNPInGlue, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// SyncVM Operand Definitions.
//===----------------------------------------------------------------------===//

let OperandType = "OPERAND_IMMEDIATE" in
def i256imm : Operand<i256>;

def imm16 : Operand<i256>, IntImmLeaf<i256, [{
  return APInt(256, 1, false).shl(16).ugt(Imm);
}]> {}

def imm128 : Operand<i256>, IntImmLeaf<i256, [{
  return APInt(256, 1, false).shl(128).ugt(Imm);
}]> {}

def z256 : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNullValue();
}]> {}

def ext_address_flag : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNullValue() || Imm.isOneValue();
}]> {
  let PrintMethod = "printEAFOperand";
}
def init_flag : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNullValue() || Imm.isOneValue();
}]> {
  let PrintMethod = "printInitOperand";
}

// Operand for printing out a condition code.
def cc : Operand<i256> {
  let PrintMethod = "printCCOperand";
  // TODO: Implement once encoding is defined.
  //let EncoderMethod = "getCCOpValue";
}

def jmptarget : Operand<OtherVT>;

// Address operands
def memop : Operand<i256> {
  let PrintMethod = "printMemOperand";
  let MIOperandInfo = (ops GR256, i16imm);
}

def stackop : Operand<i256> {
  let PrintMethod = "printStackOperand";
  let MIOperandInfo = (ops GR256, GR256, i16imm);
}

//===----------------------------------------------------------------------===//
// Complex Pattern Definitions.
//===----------------------------------------------------------------------===//

def memaddr      : ComplexPattern<iPTR, 2, "SelectMemAddr", [], []>;
def stackaddr    : ComplexPattern<iPTR, 3, "SelectStackAddr", [], []>;
def adjstackaddr : ComplexPattern<iPTR, 3, "SelectAdjStackAddr", [], []>;

//===----------------------------------------------------------------------===//
// Pattern Fragments Definitions.
//===----------------------------------------------------------------------===//

class AddressSpacesImpl {
  int Stack = 0;
  int Heap = 1;
  int CallData = 2;
  int ReturnData = 3;
  int Code = 4;
}

def AddrSpaces : AddressSpacesImpl;

class AddressSpaceList<list<int> AS> {
  list<int> AddrSpaces = AS;
}

def Address_stack       : AddressSpaceList<[ AddrSpaces.Stack ]>;
def Address_heap        : AddressSpaceList<[ AddrSpaces.Heap ]>;
def Address_calldata    : AddressSpaceList<[ AddrSpaces.CallData ]>;
def Address_returndata  : AddressSpaceList<[ AddrSpaces.ReturnData ]>;
def Address_code        : AddressSpaceList<[ AddrSpaces.Code ]>;

foreach as = [ "stack", "heap", "calldata", "returndata", "code" ] in {
let AddressSpaces = !cast<AddressSpaceList>("Address_"#as).AddrSpaces in {
def load_#as : PatFrag<(ops node:$ptr), (unindexedload node:$ptr)> {
  let IsLoad = 1;
}
def store_#as : PatFrag<(ops node:$val, node:$ptr),
                        (unindexedstore node:$val, node:$ptr)> {
  let IsStore = 1;
}
}
}

//===----------------------------------------------------------------------===//
// Instruction list..
// ADJCALLSTACKDOWN/UP implicitly use/def SP because they may be expanded into
// a stack adjustment and the codegen must know that they may modify the stack
// pointer before prolog-epilog rewriting occurs.
// Pessimistically assume ADJCALLSTACKDOWN / ADJCALLSTACKUP will become
// sub / add which can clobber SR.
let isCodeGenOnly = 1, Defs = [SP, Flags], Uses = [SP] in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
                              "#ADJCALLSTACKDOWN $amt1 $amt2",
                              [(SyncVMcallseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : Pseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
                              "#ADJCALLSTACKUP $amt1 $amt2",
                              [(SyncVMcallseq_end timm:$amt1, timm:$amt2)]>;
}

let isCodeGenOnly = 1, Defs = [Flags], Uses = [SP] in {
def ADDframe : Pseudo<(outs GR256:$dst), (ins i256imm:$base, i256imm:$offset),
                      "# ADDframe PSEUDO", []>;
}

let isCodeGenOnly = 1, isPseudo = 1 in {
  def SELrrr : Pseudo<(outs GR256:$dst), (ins GR256:$src, GR256:$src2, i256imm:$cc),
                       "# SELrrr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc GR256:$src, GR256:$src2, imm:$cc))]>;
  def SELirr : Pseudo<(outs GR256:$dst), (ins imm16:$src, GR256:$src2, i256imm:$cc),
                       "# SELirr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc imm16:$src, GR256:$src2, imm:$cc))]>;
let mayLoad = 1 in {
  def SELcrr : Pseudo<(outs GR256:$dst), (ins memop:$src, GR256:$src2, i256imm:$cc),
                       "# SELcrr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_code memaddr:$src), GR256:$src2, imm:$cc))]>;
  def SELsrr : Pseudo<(outs GR256:$dst), (ins stackop:$src, GR256:$src2, i256imm:$cc),
                       "# SELsrr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_stack stackaddr:$src), GR256:$src2, imm:$cc))]>;
}
  def SELrir : Pseudo<(outs GR256:$dst), (ins GR256:$src, imm16:$src2, i256imm:$cc),
                       "# SELrir PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc GR256:$src, imm16:$src2, imm:$cc))]>;
  def SELiir : Pseudo<(outs GR256:$dst), (ins imm16:$src, imm16:$src2, i256imm:$cc),
                       "# SELiir PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc imm16:$src, imm16:$src2, imm:$cc))]>;
let mayLoad = 1 in {
  def SELcir : Pseudo<(outs GR256:$dst), (ins memop:$src, imm16:$src2, i256imm:$cc),
                       "# SELcir PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_code memaddr:$src), imm16:$src2, imm:$cc))]>;
  def SELsir : Pseudo<(outs GR256:$dst), (ins stackop:$src, imm16:$src2, i256imm:$cc),
                       "# SELsir PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_stack stackaddr:$src), imm16:$src2, imm:$cc))]>;
  def SELrcr : Pseudo<(outs GR256:$dst), (ins GR256:$src, memop:$src2, i256imm:$cc),
                       "# SELrcr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc GR256:$src, (load_code memaddr:$src2), imm:$cc))]>;
  def SELicr : Pseudo<(outs GR256:$dst), (ins imm16:$src, memop:$src2, i256imm:$cc),
                       "# SELicr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc imm16:$src, (load_code memaddr:$src2), imm:$cc))]>;
  def SELccr : Pseudo<(outs GR256:$dst), (ins memop:$src, memop:$src2, i256imm:$cc),
                       "# SELccr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_code memaddr:$src), (load_code memaddr:$src2), imm:$cc))]>;
  def SELscr : Pseudo<(outs GR256:$dst), (ins stackop:$src, memop:$src2, i256imm:$cc),
                       "# SELscr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_stack stackaddr:$src), (load_code memaddr:$src2), imm:$cc))]>;
  def SELrsr : Pseudo<(outs GR256:$dst), (ins GR256:$src, stackop:$src2, i256imm:$cc),
                       "# SELrsr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc GR256:$src, (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELisr : Pseudo<(outs GR256:$dst), (ins imm16:$src, stackop:$src2, i256imm:$cc),
                       "# SELisr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc imm16:$src, (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELcsr : Pseudo<(outs GR256:$dst), (ins memop:$src, stackop:$src2, i256imm:$cc),
                       "# SELcsr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_code memaddr:$src), (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELssr : Pseudo<(outs GR256:$dst), (ins stackop:$src, stackop:$src2, i256imm:$cc),
                       "# SELssr PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc (load_stack stackaddr:$src), (load_stack stackaddr:$src2), imm:$cc))]>;
}

let mayStore = 1 in {
  def SELrrs : Pseudo<(outs), (ins GR256:$src, GR256:$src2, i256imm:$cc, stackop:$dst),
                       "# SELrrs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc GR256:$src, GR256:$src2, imm:$cc),
                          stackaddr:$dst)]>;
  def SELirs : Pseudo<(outs), (ins imm16:$src, GR256:$src2, i256imm:$cc, stackop:$dst),
                       "# SELirs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc imm16:$src, GR256:$src2, imm:$cc),
                          stackaddr:$dst)]>;
let mayLoad = 1 in {
  def SELcrs : Pseudo<(outs), (ins memop:$src, GR256:$src2, i256imm:$cc, stackop:$dst),
                       "# SELcrs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_code memaddr:$src), GR256:$src2, imm:$cc),
                          stackaddr:$dst)]>;
  def SELsrs : Pseudo<(outs), (ins stackop:$src, GR256:$src2, i256imm:$cc, stackop:$dst),
                       "# SELsrs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_stack stackaddr:$src), GR256:$src2, imm:$cc),
                          stackaddr:$dst)]>;
}
  def SELris : Pseudo<(outs), (ins GR256:$src, imm16:$src2, i256imm:$cc, stackop:$dst),
                       "# SELris PSEUDO",
                       [(store_stack
                          (SyncVMselectcc GR256:$src, imm16:$src2, imm:$cc),
                          stackaddr:$dst)]>;
  def SELiis : Pseudo<(outs), (ins imm16:$src, imm16:$src2, i256imm:$cc, stackop:$dst),
                       "# SELiis PSEUDO",
                       [(store_stack
                          (SyncVMselectcc imm16:$src, imm16:$src2, imm:$cc),
                          stackaddr:$dst)]>;
let mayLoad = 1 in {
  def SELcis : Pseudo<(outs), (ins memop:$src, imm16:$src2, i256imm:$cc, stackop:$dst),
                       "# SELcis PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_code memaddr:$src), imm16:$src2, imm:$cc),
                          stackaddr:$dst)]>;
  def SELsis : Pseudo<(outs), (ins stackop:$src, imm16:$src2, i256imm:$cc, stackop:$dst),
                       "# SELsis PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_stack stackaddr:$src), imm16:$src2, imm:$cc),
                          stackaddr:$dst)]>;
  def SELrcs : Pseudo<(outs), (ins GR256:$src, memop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELrcs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc GR256:$src, (load_code memaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELics : Pseudo<(outs), (ins imm16:$src, memop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELics PSEUDO",
                       [(store_stack
                          (SyncVMselectcc imm16:$src, (load_code memaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELccs : Pseudo<(outs), (ins memop:$src, memop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELccs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_code memaddr:$src), (load_code memaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELscs : Pseudo<(outs), (ins stackop:$src, memop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELscs PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_stack stackaddr:$src), (load_code memaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELrss : Pseudo<(outs), (ins GR256:$src, stackop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELrss PSEUDO",
                       [(store_stack
                          (SyncVMselectcc GR256:$src, (load_stack stackaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELiss : Pseudo<(outs), (ins imm16:$src, stackop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELiss PSEUDO",
                       [(store_stack
                          (SyncVMselectcc imm16:$src, (load_stack stackaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELcss : Pseudo<(outs), (ins memop:$src, stackop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELcss PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_code memaddr:$src), (load_stack stackaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
  def SELsss : Pseudo<(outs), (ins stackop:$src, stackop:$src2, i256imm:$cc, stackop:$dst),
                       "# SELsss PSEUDO",
                       [(store_stack
                          (SyncVMselectcc (load_stack stackaddr:$src), (load_stack stackaddr:$src2), imm:$cc),
                          stackaddr:$dst)]>;
}
}
}

def : Pat<(store_stack (load_stack(SyncVMselectcc stackaddr:$addr1, stackaddr:$addr2, imm:$cc)), stackaddr:$addr3),
          (SELsss stackaddr:$addr1, stackaddr:$addr2, imm:$cc, stackaddr:$addr3)>;
def : Pat<(load_stack(SyncVMselectcc stackaddr:$addr1, stackaddr:$addr2, imm:$cc)),
          (SELssr stackaddr:$addr1, stackaddr:$addr2, imm:$cc)>;
def : Pat<(load_code(SyncVMselectcc memaddr:$addr1, memaddr:$addr2, imm:$cc)),
          (SELccr memaddr:$addr1, memaddr:$addr2, imm:$cc)>;

def NOOP : INop< 18, "noop", [] >;

def zext256 : PatFrag<(ops node:$src), (zext (i256 node:$src))>;

multiclass Arith<bits<8> opcode, string asmstring, SDPatternOperator node, bit commutes> {
  let Defs = [Flags] in {
  let isCommutable = commutes in {

  def rrr : Irrr<opcode, 0, (outs GR256:$rd), (ins GR256:$rs1, GR256:$rs2),
                 !strconcat(asmstring, "\t$rs1, $rs2, $rd"),
                 [(set GR256:$rd, (node GR256:$rs1, GR256:$rs2)),
                  (implicit Flags)]>;

  let mayLoad = 1 in {
  def crr : Icrr<opcode, 0, (outs GR256:$rd), (ins memop:$src1, GR256:$rs2),
                 !strconcat(asmstring, "\tcode[$src1], $rs2, $rd"),
                 [(set GR256:$rd, (node (load_code memaddr:$src1), GR256:$rs2)),
                  (implicit Flags)]>;
  def srr : Isrr<opcode, 0, (outs GR256:$rd), (ins stackop:$src1, GR256:$rs2),
                 !strconcat(asmstring, "\tstack-[$src1], $rs2, $rd"),
                 [(set GR256:$rd, (node (load_stack stackaddr:$src1), GR256:$rs2)),
                  (implicit Flags)]>;
  }
  
  let mayStore = 1 in {
  def rrs : Irrs<opcode, 0, (outs), (ins GR256:$rs1, GR256:$rs2, stackop:$dst),
                 !strconcat(asmstring, "\t$rs1, $rs2, stack-[$dst]"),
                 [(store_stack (node GR256:$rs1, GR256:$rs2), stackaddr:$dst),
                  (implicit Flags)]>;
  }

  let mayLoad = 1, mayStore = 1 in {
  def crs : Icrs<opcode, 0, (outs), (ins memop:$src1, GR256:$rs2, stackop:$dst),
                 !strconcat(asmstring, "\tcode[$src1], $rs2, stack-[$dst]"),
                 [(store_stack (node (load_code memaddr:$src1), GR256:$rs2), stackaddr:$dst),
                  (implicit Flags)]>;
  def srs : Isrs<opcode, 0, (outs), (ins stackop:$src1, GR256:$rs2, stackop:$dst),
                 !strconcat(asmstring, "\tstack-[$src1], $rs2, stack-[$dst]"),
                 [(store_stack (node (load_stack stackaddr:$src1), GR256:$rs2), stackaddr:$dst),
                  (implicit Flags)]>;
  }
  }
  }
}

multiclass ArithICommutable<bits<8> opcode, string asmstring, SDPatternOperator node> {
  let Defs = [Flags] in {
  let isCommutable = 1 in {
  def irr : Iirr<opcode, 0, (outs GR256:$rd), (ins GR256:$rs2, imm16:$imm),
                 !strconcat(asmstring, "\t$imm, $rs2, $rd"),
                 [(set GR256:$rd, (node GR256:$rs2, imm16:$imm)),
                  (implicit Flags)]>;
  let mayStore = 1 in {
  def irs : Iirs<opcode, 0, (outs), (ins GR256:$rs2, imm16:$imm, stackop:$dst),
                 !strconcat(asmstring, "\t$imm, $rs2, stack-[$dst]"),
                 [(store_stack (node GR256:$rs2, imm16:$imm), stackaddr:$dst),
                  (implicit Flags)]>;
  }
  }
  }
}

multiclass ArithINonCommutable<bits<8> opcode, string asmstring, SDPatternOperator node> {
  let Defs = [Flags] in {
  def irr : Iirr<opcode, 0, (outs GR256:$rd), (ins GR256:$rs2, imm16:$imm),
                 !strconcat(asmstring, "\t$imm, $rs2, $rd"),
                 [(set GR256:$rd, (node imm16:$imm, GR256:$rs2)),
                  (implicit Flags)]>;
  def xrr : Iirr<opcode, 1, (outs GR256:$rd), (ins GR256:$rs2, imm16:$imm),
                 !strconcat(asmstring, ".s\t$imm, $rs2, $rd"),
                 [(set GR256:$rd, (node GR256:$rs2, imm16:$imm)),
                  (implicit Flags)]>;
  let mayLoad = 1 in {
  def yrr : Icrr<opcode, 1, (outs GR256:$rd), (ins memop:$src1, GR256:$rs2),
                 !strconcat(asmstring, ".s\tcode[$src1], $rs2, $rd"),
                 [(set GR256:$rd, (node GR256:$rs2, (load_code memaddr:$src1))),
                  (implicit Flags)]>;
  def zrr : Isrr<opcode, 1, (outs GR256:$rd), (ins stackop:$src1, GR256:$rs2),
                 !strconcat(asmstring, ".s\tstack-[$src1], $rs2, $rd"),
                 [(set GR256:$rd, (node GR256:$rs2, (load_stack stackaddr:$src1))),
                  (implicit Flags)]>;
  }
  let mayStore = 1 in {
  def xrs : Iirs<opcode, 1, (outs), (ins GR256:$rs2, imm16:$imm, stackop:$dst),
                 !strconcat(asmstring, ".s\t$imm, $rs2, stack-[$dst]"),
                 [(store_stack (node GR256:$rs2, imm16:$imm), stackaddr:$dst),
                  (implicit Flags)]>;
  def irs : Iirs<opcode, 0, (outs), (ins GR256:$rs2, imm16:$imm, stackop:$dst),
                 !strconcat(asmstring, "\t$imm, $rs2, stack-[$dst]"),
                 [(store_stack (node imm16:$imm, GR256:$rs2), stackaddr:$dst),
                  (implicit Flags)]>;
  let mayLoad = 1 in {
  def yrs : Icrs<opcode, 1, (outs), (ins memop:$src1, GR256:$rs2, stackop:$dst),
                 !strconcat(asmstring, ".s\tcode[$src1], $rs2, stack-[$dst]"),
                 [(store_stack (node GR256:$rs2, (load_code memaddr:$src1)), stackaddr:$dst),
                  (implicit Flags)]>;
  def zrs : Isrs<opcode, 1, (outs), (ins stackop:$src1, GR256:$rs2, stackop:$dst),
                 !strconcat(asmstring, ".s\tstack-[$src1], $rs2, stack-[$dst]"),
                 [(store_stack (node GR256:$rs2, (load_stack stackaddr:$src1)), stackaddr:$dst),
                  (implicit Flags)]>;
  }
  }
  }
}

multiclass Arith2<bits<8> opcode, string asmstring, SDPatternOperator node, bit commutes> {
  let Defs = [Flags] in {
  let isCommutable = commutes in {

  def rrrr : Irrrr<opcode, 0, (outs GR256:$rd1, GR256:$rd2), (ins GR256:$rs1, GR256:$rs2),
                   !strconcat(asmstring, "\t$rs1, $rs2, $rd1, $rd2"),
                   [(set GR256:$rd1, GR256:$rd2, (node GR256:$rs1, GR256:$rs2)),
                    (implicit Flags)]>;

  let mayLoad = 1 in {
  def crrr : Icrrr<opcode, 0, (outs GR256:$rd1, GR256:$rd2), (ins memop:$src1, GR256:$rs2),
                  !strconcat(asmstring, "\tcode[$src1], $rs2, $rd1, $rd2"),
                  [(set GR256:$rd1, GR256:$rd2, (node (load_code memaddr:$src1), GR256:$rs2)),
                   (implicit Flags)]>;
  def srrr : Isrrr<opcode, 0, (outs GR256:$rd1, GR256:$rd2), (ins stackop:$src1, GR256:$rs2),
                  !strconcat(asmstring, "\tstack-[$src1], $rs2, $rd1, $rd2"),
                  [(set GR256:$rd1, GR256:$rd2, (node (load_stack stackaddr:$src1), GR256:$rs2)),
                   (implicit Flags)]>;
  }

  let mayStore = 1 in {
  def rrsr : Irrsr<opcode, 0, (outs GR256:$rd2), (ins GR256:$rs1, GR256:$rs2, stackop:$dst),
                   !strconcat(asmstring, "\t$rs1, $rs2, stack-[$dst], $rd2"), []>;
  let mayLoad = 1 in {
  def crsr : Icrsr<opcode, 0, (outs GR256:$rd2), (ins memop:$rs1, GR256:$rs2, stackop:$dst),
                   !strconcat(asmstring, "\tcode[$rs1], $rs2, stack-[$dst], $rd2"), []>;
  def srsr : Isrsr<opcode, 0, (outs GR256:$rd2), (ins stackop:$rs1, GR256:$rs2, stackop:$dst),
                   !strconcat(asmstring, "\tstack-[$rs1], $rs2, stack-[$dst], $rd2"), []>;
  }
  }
  }
  }
}

multiclass Arith2ICommutable<bits<8> opcode, string asmstring, SDPatternOperator node> {
  let Defs = [Flags] in {
  let isCommutable = 1 in {
  def irrr : Iirrr<opcode, 0, (outs GR256:$rd1, GR256:$rd2), (ins GR256:$rs2, imm16:$imm),
                   !strconcat(asmstring, "\t$imm, $rs2, $rd1, $rd2"),
                   [(set GR256:$rd1, GR256:$rd2, (node GR256:$rs2, imm16:$imm)),
                    (implicit Flags)]>;
  let mayStore = 1 in
  def irsr : Iirsr<opcode, 0, (outs GR256:$rd2), (ins GR256:$rs2, imm16:$imm, stackop:$dst),
                   !strconcat(asmstring, "\t$imm, $rs2, stack-[$dst], $rd2"),
                   []>;
  }
  }
}

multiclass Arith2INonCommutable<bits<8> opcode, string asmstring, SDPatternOperator node> {
  let Defs = [Flags] in {
  def irrr : Iirrr<opcode, 0, (outs GR256:$rd1, GR256:$rd2), (ins GR256:$rs2, imm16:$imm),
                   !strconcat(asmstring, "\t$imm, $rs2, $rd1, $rd2"),
                   [(set GR256:$rd1, GR256:$rd2, (node imm16:$imm, GR256:$rs2)),
                    (implicit Flags)]>;
  def xrrr : Iirrr<opcode, 1, (outs GR256:$rd1, GR256:$rd2), (ins GR256:$rs2, imm16:$imm),
                   !strconcat(asmstring, ".s\t$imm, $rs2, $rd1, $rd2"),
                   [(set GR256:$rd1, GR256:$rd2, (node GR256:$rs2, imm16:$imm)),
                    (implicit Flags)]>;
  let mayLoad = 1 in {
  def yrrr : Icrrr<opcode, 1, (outs GR256:$rd1, GR256:$rd2), (ins memop:$src1, GR256:$rs2),
                   !strconcat(asmstring, ".s\tcode[$src1], $rs2, $rd1, $rd2"),
                   [(set GR256:$rd1, GR256:$rd2, (node GR256:$rs2, (load_code memaddr:$src1))),
                    (implicit Flags)]>;
  def zrrr : Isrrr<opcode, 1, (outs GR256:$rd1, GR256:$rd2), (ins stackop:$src1, GR256:$rs2),
                   !strconcat(asmstring, ".s\tstack-[$src1], $rs2, $rd1, $rd2"),
                   [(set GR256:$rd1, GR256:$rd2, (node GR256:$rs2, (load_stack stackaddr:$src1))),
                    (implicit Flags)]>;
  }
  let mayStore = 1 in {
  def irsr : Iirsr<opcode, 0, (outs GR256:$rd2), (ins GR256:$rs2, imm16:$imm, stackop:$dst),
                   !strconcat(asmstring, "\t$imm, $rs2, stack-[$dst], $rd2"), []>;
  def xrsr : Iirsr<opcode, 1, (outs GR256:$rd2), (ins GR256:$rs2, imm16:$imm, stackop:$dst),
                   !strconcat(asmstring, ".s\t$imm, $rs2, stack-[$dst], $rd2"), []>;
  let mayLoad = 1 in {
  def yrsr : Icrsr<opcode, 1, (outs GR256:$rd2), (ins memop:$src1, GR256:$rs2, stackop:$dst),
                   !strconcat(asmstring, ".s\tcode[$src1], $rs2, stack-[$dst], $rd2"), []>;
  def zrsr : Isrsr<opcode, 1, (outs GR256:$rd2), (ins stackop:$src1, GR256:$rs2, stackop:$dst),
                   !strconcat(asmstring, ".s\tstack-[$src1], $rs2, stack-[$dst], $rd2"), []>;
  }
  }
  }
}

defm ADD  : Arith<2,  "add", add, 1>;
defm ADD  : ArithICommutable<2,  "add", add>;

// MOV patterns
def : Pat<(store_stack (load_code memaddr:$src), stackaddr:$dst),  (ADDcrs memaddr:$src, 0, stackaddr:$dst)>;
def : Pat<(store_stack (load_stack stackaddr:$src), stackaddr:$dst),  (ADDsrs stackaddr:$src, 0, stackaddr:$dst)>;
def : Pat<(imm16:$val),  (ADDirr 0, imm16:$val)>;
def : Pat<(load_code memaddr:$addr),  (ADDcrr memaddr:$addr, 0)>;
def : Pat<(load_stack stackaddr:$addr),  (ADDsrr stackaddr:$addr, 0)>;
def : Pat<(store_stack GR256:$src, stackaddr:$dst),  (ADDrrs GR256:$src, 0, stackaddr:$dst)>;
def : Pat<(store_stack imm16:$src, stackaddr:$dst),  (ADDirs 0, imm16:$src, stackaddr:$dst)>;

defm SUB  : Arith<4,  "sub", sub, 0>;
defm SUB  : ArithINonCommutable<4, "sub", sub>;

// SelecCC, BR_CC supplement
// TODO: CPR-447 enable patterns
// def : Pat<(SyncVMcmp i16imm:$lhs, GR256:$rhs), (SUBirr i16imm:$lhs, GR256:$rhs)>;
// def : Pat<(SyncVMcmp GR256:$lhs, i16imm:$rhs), (SUBxrr i16imm:$rhs, GR256:$lhs)>;
def : Pat<(SyncVMcmp GR256:$lhs, GR256:$rhs), (SUBrrr GR256:$lhs, GR256:$rhs)>;
def : Pat<(SyncVMcmp (load_code memaddr:$lhs), GR256:$rhs), (SUBcrr memaddr:$lhs, GR256:$rhs)>;
def : Pat<(SyncVMcmp GR256:$lhs, (load_code memaddr:$rhs)), (SUByrr memaddr:$rhs, GR256:$lhs)>;
def : Pat<(SyncVMcmp (load_stack stackaddr:$lhs), GR256:$rhs), (SUBsrr stackaddr:$lhs, GR256:$rhs)>;
def : Pat<(SyncVMcmp GR256:$lhs, (load_stack stackaddr:$rhs)), (SUBzrr stackaddr:$rhs, GR256:$lhs)>;

defm MUL  : Arith2<6, "mul", umullohi, 1>;
defm MUL  : Arith2ICommutable<6, "mul", umullohi>;
defm DIV  : Arith2<8, "div", udivrem, 0>;
defm DIV  : Arith2INonCommutable<8, "div", udivrem>;

// FIXME: Wrong encoding
// FIXME: Wrong flags
defm AND  : Arith<33, "and", and, 1>;
defm AND  : ArithICommutable<33, "and", and>;
defm OR   : Arith<34, "or", or, 1>;
defm OR   : ArithICommutable<34, "or", or>;
defm XOR  : Arith<35, "xor", xor, 1>;
defm XOR  : ArithICommutable<35, "xor", xor>;
defm SHL  : Arith<36, "shl", shl, 0>;
defm SHL  : ArithINonCommutable<36, "shl", shl>;
defm SHR  : Arith<37, "shr", srl, 0>;
defm SHR  : ArithINonCommutable<37, "shr", srl>;
defm ROL  : Arith<38, "rol", rotl, 0>;
defm ROL  : ArithINonCommutable<38, "rol", rotl>;
defm ROR  : Arith<38, "ror", rotr, 0>;
defm ROR  : ArithINonCommutable<38, "ror", rotr>;

// TODO: should be mayStore.
let hasSideEffects = 1, Defs = [SP, Flags], Uses = [SP] in
def PUSH    : IForm < 39, SrcReg, DstReg, 0, (outs), (ins i256imm:$size, GR256:$val),
                      "push\t#$size, $val",
                      [(SyncVMpush timm:$size, GR256:$val)] >;

let mayLoad = 1, hasSideEffects = 1, Defs = [SP, Flags], Uses = [SP] in
def POP     : IForm < 40, SrcReg, DstReg, 0, (outs GR256:$val), (ins i256imm:$size),
                      "pop\t#$size, $val",
                      [(set GR256:$val, (SyncVMpop timm:$size))] >;

let Uses = [Flags] in {
def CMOVrrr : IForm < 41, SrcReg, DstReg, 0, (outs GR256:$rd1), (ins GR256:$rs1, GR256:$rs2, cc:$flags),
                      "cmov$flags\t$rs1, r0, $rd1", []>;
def CMOVirr : IForm < 41, SrcImm, DstReg, 0, (outs GR256:$rd1), (ins imm16:$rs1, GR256:$rs2, cc:$flags),
                      "cmov$flags\t$rs1, r0, $rd1", []>;
def CMOVcrr : IForm < 41, SrcCode, DstReg, 0, (outs GR256:$rd1), (ins memop:$rs1, GR256:$rs2, cc:$flags),
                      "cmov$flags\tcode[$rs1], r0, $rd1", []>;
def CMOVsrr : IForm < 41, SrcStack, DstReg, 0, (outs GR256:$rd1), (ins stackop:$rs1, GR256:$rs2, cc:$flags),
                      "cmov$flags\tstack-[$rs1], r0, $rd1", []>;
def CMOVrrs : IForm < 41, SrcReg, DstStack, 0, (outs), (ins GR256:$rs1, GR256:$rs2, cc:$flags, stackop:$dst),
                      "cmov$flags\t$rs1, r0, stack-[$dst]", []>;
def CMOVirs : IForm < 41, SrcImm, DstStack, 0, (outs), (ins imm16:$rs1, GR256:$rs2, cc:$flags, stackop:$dst),
                      "cmov$flags\t$rs1, r0, stack-[$dst]", []>;
def CMOVcrs : IForm < 41, SrcCode, DstStack, 0, (outs), (ins memop:$rs1, GR256:$rs2, cc:$flags, stackop:$dst),
                      "cmov$flags\tcode[$rs1], r0, stack-[$dst]", []>;
def CMOVsrs : IForm < 41, SrcStack, DstStack, 0, (outs), (ins stackop:$rs1, GR256:$rs2, cc:$flags, stackop:$dst),
                      "cmov$flags\tstack-[$rs1], r0, stack-[$dst]", []>;
}

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
let isTerminator = 1, isBranch = 1, isBarrier = 1, Uses = [Flags] in
def J: IJump<10, (ins jmptarget:$dst_true, jmptarget:$dst_false, cc:$flags),
              "jump$flags\tr0, $dst_true, $dst_false",
              [(SyncVMbrcc bb:$dst_true, bb:$dst_false, imm:$flags)]>;

let isReturn = 1, isTerminator = 1, isBarrier = 1  in
def RET   : IRet<7, RFNone, "ret", [(SyncVMret)]>;
let isTerminator = 1, isBarrier = 1, isTrap = 1 in
def THROW : IRet<7, RFErr, "ret.err", [(int_syncvm_throw)]>;
              /*
def JCC: IJump<10, (outs), (ins jmptarget:$addrtrue, jmptarget:$addrfalse, GR256:$cond),
               "j$cond\t$dst_true, $dst_false",
               [(SyncVMbrcc bb:$dst_true, bb:$dst_false, GR256:$cond)]>;
               */

/*
let Defs = [Flags], isTerminator = 1 in {
def CMPrr   : SyncVMInstruction<  0x02, (outs),
                                  (ins GR256:$src1, GR256:$src2),
                                  "sub\t$src1, $src2, r0",
                                  [(SyncVMsub GR256:$src1, GR256:$src2),
                                   (implicit Flags)] >;

def CMPrz   : SyncVMInstruction<  0x02, (outs),
                                  (ins GR256:$src1, i256imm:$src2),
                                  "sub\t$src1, r0, r0",
                                  [(SyncVMsub GR256:$src1, z256:$src2),
                                   (implicit Flags)] >;
}

def DIVrrrz    : ArithmeticRRRZ< 0x04, "div", udiv >;
def DIVrirz    : SyncVMInstruction< 0x04, (outs GR256:$out),
                                    (ins GR256:$in1, imm128:$in2),
                                    "div\t$in1, #$in2, $out, r0",
                                    [(set GR256:$out, (udiv GR256:$in1, imm128:$in2))]>;
def REMrrzr    : ArithmeticRRZR< 0x04, "div", urem >;
def DIVrizr    : SyncVMInstruction< 0x04, (outs GR256:$out),
                                    (ins GR256:$in1, imm128:$in2),
                                    "div\t$in1, #$in2, r0, $out",
                                    [(set GR256:$out, (urem GR256:$in1, imm128:$in2))]>;
// TODO: needs logic in isel lowering to be generated
def DIVREMrrrr : ArithmeticRRRR< 0x04, "div", udivrem >;

// Operations with memory and registers

let Defs = [Flags] in
// MOV is ADD rSrc, r0, rDst
def MOVrr      : ArithmeticRZR< 0x01, "add", add >;
let mayStore = 1, Defs = [Flags] in {
def MOVrs      : SyncVMInstruction< 0x090, (outs), (ins GR256:$val, stackop:$dst),
                                    "mov\t$val, $dst",
                                    [(store_stack GR256:$val, stackaddr:$dst)]>;
def MOVrh      : SyncVMInstruction< 0x091, (outs), (ins GR256:$val, memop:$dst),
                                    "mov\t$val, $dst",
                                    [(store_heap GR256:$val, memaddr:$dst)]>;
def MOVrp      : SyncVMInstruction< 0x092, (outs), (ins GR256:$val, memop:$dst),
                                    "mov.p\t$val, $dst",
                                    [(store_parent GR256:$val, memaddr:$dst)]>;
def MOVrc      : SyncVMInstruction< 0x093, (outs), (ins GR256:$val, memop:$dst),
                                    "mov.c\t$val, $dst",
                                    [(store_child GR256:$val, memaddr:$dst)]>;
def Store      : SyncVMInstruction< 0x12, (outs), (ins GR256:$val, GR256:$addr, ext_address_flag:$eaf),
                                    "st$eaf\t$val, $addr",
                                    [(int_syncvm_sstore GR256:$val, GR256:$addr, imm:$eaf)]>;
def Event      : SyncVMInstruction< 0x121, (outs), (ins GR256:$val1, GR256:$val2, init_flag:$init),
                                    "evt$init\t$val1, $val2",
                                    [(int_syncvm_event GR256:$val1, GR256:$val2, imm:$init)]>;
}

let mayLoad = 1, Defs = [Flags] in {
def MOVsr      : SyncVMInstruction< 0x100, (outs GR256:$val), (ins stackop:$addr),
                                    "mov\t$addr, $val",
                                    [(set GR256:$val, (load_stack stackaddr:$addr))]>;
def MOVhr      : SyncVMInstruction< 0x101, (outs GR256:$val), (ins memop:$addr),
                                    "mov\t$addr, $val",
                                    [(set GR256:$val, (load_heap memaddr:$addr))]>;
def MOVpr      : SyncVMInstruction< 0x102, (outs GR256:$val), (ins memop:$addr),
                                    "mov.p\t$addr, $val",
                                    [(set GR256:$val, (load_parent memaddr:$addr))]>;
def MOVcr      : SyncVMInstruction< 0x103, (outs GR256:$val), (ins memop:$addr),
                                    "mov.c\t$addr, $val",
                                    [(set GR256:$val, (load_child memaddr:$addr))]>;
def Load       : SyncVMInstruction< 0x13, (outs GR256:$val), (ins GR256:$addr, ext_address_flag:$eaf),
                                    "ld$eaf\t$addr, $val",
                                    [(set GR256:$val, (int_syncvm_sload GR256:$addr, imm:$eaf))]>;
}

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
let isReturn = 1, isTerminator = 1, isBarrier = 1 in {
def RET   : SyncVMInstruction<0x07, (outs), (ins), "ret", [(SyncVMret)]>;
def THROW : SyncVMInstruction<0x071, (outs), (ins), "throw", [(int_syncvm_throw)]>;
}

let isCall = 1,
    Defs = [R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15],
    Uses = [SP] in {
  def CALL : Call<0x43, (outs), (ins i16imm:$callee),
                  "call\t$callee", [(SyncVMcall tglobaladdr:$callee)]>;
  def CALLF : SyncVMInstruction<0x45, (outs), (ins GR256:$call_config),
                                "callf\t$call_config", [(int_syncvm_farcall GR256:$call_config)]>;
  def CALLD : SyncVMInstruction<0x46, (outs), (ins GR256:$call_config),
                                "callfd\t$call_config", [(int_syncvm_delegatecall GR256:$call_config)]>;
  def CALLC : SyncVMInstruction<0x47, (outs), (ins GR256:$call_config),
                                "callfc\t$call_config", [(int_syncvm_callcode GR256:$call_config)]>;
  def CALLS : SyncVMInstruction<0x48, (outs), (ins GR256:$call_config),
                                "callfs\t$call_config", [(int_syncvm_staticcall GR256:$call_config)]>;
}

let isTerminator = 1, isBranch = 1, isBarrier = 1, Uses = [Flags] in {
def JCC: Jump<0x08, (outs), (ins jmptarget:$dst_true, jmptarget:$dst_false, cc:$cond),
              "j$cond\t$dst_true, $dst_false",
              [(SyncVMbrcc bb:$dst_true, bb:$dst_false, imm:$cond)]>;
def J: Jump<0x08, (outs), (ins jmptarget:$dst_true, jmptarget:$dst_false, GR256:$cond),
            "j$cond\t$dst_true, $dst_false",
            [(SyncVMbrcc bb:$dst_true, bb:$dst_false, GR256:$cond)]>;
}

//===----------------------------------------------------------------------===//
// Constants materialization
//===----------------------------------------------------------------------===//

let isPseudo = 1 in
def CONST   : Pseudo< (outs GR256:$out), (ins i256imm:$val),
                      "#pseudo#", [(set GR256:$out, imm:$val)] >;
def SFLLir  : SyncVMInstruction< 0x050, (outs GR256:$out), (ins imm128:$val, GR256:$reg),
                                 "sfll\t#$val, $reg, $out", [] >;
def SFLHir  : SyncVMInstruction< 0x051, (outs GR256:$out), (ins imm128:$val, GR256:$reg),
                                 "sflh\t#$val, $reg, $out", [] >;
//===----------------------------------------------------------------------===//
// Stack adjustments
//===----------------------------------------------------------------------===//
// TODO: should be mayStore.
let hasSideEffects = 1, Defs = [SP, Flags], Uses = [SP] in
def PUSH    : SyncVMInstruction< 0x19, (outs), (ins i256imm:$size, GR256:$val),
                                 "push\t#$size, $val",
                                 [(SyncVMpush timm:$size, GR256:$val)] >;

let mayLoad = 1, hasSideEffects = 1, Defs = [SP, Flags], Uses = [SP] in
def POP     : SyncVMInstruction< 0x1a, (outs Z256:$val), (ins i256imm:$size),
                                 "pop\t#$size, $val",
                                 [(set Z256:$val, (SyncVMpop timm:$size))] >;

//===----------------------------------------------------------------------===//
// Intrinsics lowering
//===----------------------------------------------------------------------===//

def HABS: SyncVMInstruction<0x13, (outs), (ins GR256:$val),
                            "habs\t$val", [(int_syncvm_habs GR256:$val)]>;
def HABSR: SyncVMInstruction<0x14, (outs), (ins GR256:$val),
                             "habsr\t$val", [(int_syncvm_habsr GR256:$val)]>;
def HOUT: SyncVMInstruction<0x15, (outs GR256:$out), (ins),
                            "hout\t$out", [(set GR256:$out, (int_syncvm_hout))]>;
let hasSideEffects = 1 in {
def CTX: SyncVMInstruction<0x16, (outs GR256:$out), (ins i256imm:$val),
                           "ctx\t#$val, $out", [(set GR256:$out, (int_syncvm_getfromcontext imm:$val))]>;
def SC: SyncVMInstruction<0x17, (outs), (ins),
                           "sc", [(int_syncvm_switchcontext)]>;
def SETST: SyncVMInstruction<0x18, (outs), (ins GR256:$storage),
                           "setst", [(int_syncvm_setstorage GR256:$storage)]>;
}
*/

let isCodeGenOnly = 1 in {
// Since all stack addressing modes are SP based, SyncVM needs to insert PSEUDOs
// to adjust for a callee frame size. AdjSPBasedOffsets pass is to replace
// adjusted load and stores with their non-pseudo counterparts.
def AdjSP : Pseudo<(outs GR256:$dst), (ins GR256:$base),
                   "# AdjSP PSEUDO", []>;
def AdjSPDown : Pseudo<(outs GR256:$dst), (ins GR256:$base),
                       "# AdjSPDown PSEUDO", []>;
                       /*
let mayStore = 1 in {
  def AdjMOVrs      : Pseudo< (outs), (ins GR256:$val, stackop:$dst),
                              "# AdjMOVrs PSEUDO",
                              [(store_stack GR256:$val, stackaddr:$dst)]>;
}
let mayLoad = 1 in {
  def AdjMOVsr      : Pseudo< (outs GR256:$val), (ins stackop:$addr),
                              "# AdjMOVsr PSEUDO",
                              [(set GR256:$val, (load_stack stackaddr:$addr))]>;
}
*/
}
