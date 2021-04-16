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
def SDT_SyncVMBrCC         : SDTypeProfile<0,  3, [SDTCisVT<0, OtherVT>,
                                                   SDTCisVT<1, OtherVT>,
                                                   SDTCisVT<2, i256>]>;
def SDT_SyncVMBinary       : SDTypeProfile<0,  2, [SDTCisSameAs<0, 1>]>;

//===----------------------------------------------------------------------===//
// SyncVM Specific Node Definitions.
//===----------------------------------------------------------------------===//

def SyncVMret    : SDNode<"SyncVMISD::RET", SDTNone,
                    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMcall   : SDNode<"SyncVMISD::CALL", SDT_SyncVMCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMbrcc   : SDNode<"SyncVMISD::BR_CC", SDT_SyncVMBrCC,
                    [SDNPHasChain, SDNPInGlue]>;
def SyncVMsub    : SDNode<"SyncVMISD::SUB", SDT_SyncVMBinary, [SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// MSP430 Operand Definitions.
//===----------------------------------------------------------------------===//

def imm128 : Operand<i256>, IntImmLeaf<i256, [{
  auto Cmp = APInt(256, 1, false).shl(128);
  return Imm.ult(Cmp);
}]> {}

def z256 : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNullValue();
}]> {}

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
  let MIOperandInfo = (ops GR256, i256imm);
}

//===----------------------------------------------------------------------===//
// Complex Pattern Definitions.
//===----------------------------------------------------------------------===//

def addr : ComplexPattern<iPTR, 2, "SelectAddr", [], []>;

//===----------------------------------------------------------------------===//
// Pattern Fragments Definitions.
//===----------------------------------------------------------------------===//

class AddressSpacesImpl {
  int Stack = 0;
  int Heap = 1;
  int Parent = 2;
  int Child = 3;
}

def AddrSpaces : AddressSpacesImpl;

class AddressSpaceList<list<int> AS> {
  list<int> AddrSpaces = AS;
}

def Address_flat : AddressSpaceList<[ AddrSpaces.Stack,
                                          AddrSpaces.Heap ]>;
def Address_parent : AddressSpaceList<[ AddrSpaces.Parent ]>;
def Address_child : AddressSpaceList<[ AddrSpaces.Child ]>;

foreach as = [ "flat", "parent", "child" ] in {
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
// The NoOperation instruction
//===----------------------------------------------------------------------===//

def NOOP : Empty< 0x00, "noop", [] >;

//===----------------------------------------------------------------------===//
// Arithmetic
//===----------------------------------------------------------------------===//

let isCommutable = 1 in {
  def ADDrrr     : ArithmeticRRR<  0x01, "add", add >;
  def MULrrrz    : ArithmeticRRRZ< 0x03, "mul", mul >;
}

def SUBrrr     : ArithmeticRRR<  0x02, "sub", sub >;

let Defs = [Flags] in {
def CMPrr   : SyncVMInstruction<  0x02, (outs),
                                  (ins GR256:$src1, GR256:$src2),
                                  "sub\t$src1, $src2, r0",
                                  [(SyncVMsub GR256:$src1, GR256:$src2),
                                   (implicit Flags)] >;

def CMPrz   : SyncVMInstruction<  0x02, (outs),
                                  (ins GR256:$src1, GR256:$src2),
                                  "sub\t$src1, r0, r0",
                                  [(SyncVMsub GR256:$src1, z256:$src2),
                                   (implicit Flags)] >;
}

def DIVrrrz    : ArithmeticRRRZ< 0x04, "div", udiv >;
def REMrrzr    : ArithmeticRRZR< 0x04, "div", urem >;
// TODO: needs logic in isel lowering to be generated
def DIVREMrrrr : ArithmeticRRRR< 0x04, "div", udivrem >;

//===----------------------------------------------------------------------===//
// Operations with memory and registers
//===----------------------------------------------------------------------===//

// MOV is ADD rSrc, r0, rDst
def MOVrr      : ArithmeticRZR< 0x01, "add", add >;
let mayStore = 1 in {
def MOVrm      : SyncVMInstruction< 0x090, (outs), (ins GR256:$val, memop:$dst),
                                    "mov\t$val, $dst",
                                    [(store_flat GR256:$val, addr:$dst)]>;
def MOVrp      : SyncVMInstruction< 0x091, (outs), (ins GR256:$val, memop:$dst),
                                    "mov.p\t$val, $dst",
                                    [(store_parent GR256:$val, addr:$dst)]>;
def MOVrc      : SyncVMInstruction< 0x092, (outs), (ins GR256:$val, memop:$dst),
                                    "mov.c\t$val, $dst",
                                    [(store_child GR256:$val, addr:$dst)]>;
}

let mayLoad = 1 in {
def MOVmr      : SyncVMInstruction< 0x100, (outs GR256:$val), (ins memop:$addr),
                                    "mov\t$addr, $val",
                                    [(set GR256:$val, (load_flat addr:$addr))]>;
def MOVmp      : SyncVMInstruction< 0x101, (outs GR256:$val), (ins memop:$addr),
                                    "mov.p\t$addr, $val",
                                    [(set GR256:$val, (load_parent addr:$addr))]>;
def MOVmc      : SyncVMInstruction< 0x102, (outs GR256:$val), (ins memop:$addr),
                                    "mov.c\t$addr, $val",
                                    [(set GR256:$val, (load_child addr:$addr))]>;
}

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : SyncVMInstruction<0x07, (outs), (ins), "ret", [(SyncVMret)]>;

let isCall = 1,
    Defs = [R1, R2, R3, R4, R5, R6],
    Uses = [SP] in {
  def CALL : Call<0x43, (outs), (ins i16imm:$callee),
                  "call\t$callee", [(SyncVMcall tglobaladdr:$callee)]>;
}

let isTerminator = 1, isBranch = 1, Uses = [Flags] in {
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
def CONST : Pseudo< (outs GR256:$out), (ins i256imm:$val),
                    "#pseudo#", [(set GR256:$out, imm:$val)] >;
def MOVL  : SyncVMInstruction< 0x05, (outs GR256:$out), (ins imm128:$val),
                               "movl\t#$val, $out", [] >;
def MOVH  : SyncVMInstruction< 0x051, (outs GR256:$out), (ins imm128:$val),
                               "movh\t#$val, $out", [] >;

//===----------------------------------------------------------------------===//
// Intrinsics lowering
//===----------------------------------------------------------------------===//

def HABS: SyncVMInstruction<0x11, (outs), (ins GR256:$val),
                            "habs\t$val", [(int_syncvm_habs GR256:$val)]>;
def HABSR: SyncVMInstruction<0x12, (outs), (ins GR256:$val),
                             "habsr\t$val", [(int_syncvm_habsr GR256:$val)]>;
def HOUT: SyncVMInstruction<0x13, (outs GR256:$out), (ins),
                            "hout\t$out", [(set GR256:$out, (int_syncvm_hout))]>;
