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
def SyncVMselectcc: SDNode<"SyncVMISD::SELECT_CC", SDT_SyncVMSelectCC,
                           [SDNPInGlue]>;
def SyncVMsub    : SDNode<"SyncVMISD::SUB", SDT_SyncVMBinary, [SDNPOutGlue]>;
def SyncVMcallseq_start :
                 SDNode<"ISD::CALLSEQ_START", SDT_SyncVMCallSeqStart,
                        [SDNPHasChain, SDNPOutGlue]>;
def SyncVMcallseq_end :
                 SDNode<"ISD::CALLSEQ_END",   SDT_SyncVMCallSeqEnd,
                        [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def SyncVMpush : SDNode<"SyncVMISD::PUSH", SDT_SyncVMPush, [SDNPHasChain]>;
def SyncVMpop : SDNode<"SyncVMISD::POP",   SDT_SyncVMPop, [SDNPHasChain]>;

//===----------------------------------------------------------------------===//
// MSP430 Operand Definitions.
//===----------------------------------------------------------------------===//

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
  let MIOperandInfo = (ops GR256, i256imm);
}

def stackop : Operand<i256> {
  let PrintMethod = "printStackOperand";
  let MIOperandInfo = (ops GR256, GR256, i256imm);
}

//===----------------------------------------------------------------------===//
// Complex Pattern Definitions.
//===----------------------------------------------------------------------===//

def memaddr   : ComplexPattern<iPTR, 2, "SelectMemAddr", [], []>;
def stackaddr : ComplexPattern<iPTR, 3, "SelectStackAddr", [], []>;
def adjstackaddr : ComplexPattern<iPTR, 3, "SelectAdjStackAddr", [], []>;

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

def Address_stack  : AddressSpaceList<[ AddrSpaces.Stack ]>;
def Address_heap   : AddressSpaceList<[ AddrSpaces.Heap ]>;
def Address_parent : AddressSpaceList<[ AddrSpaces.Parent ]>;
def Address_child  : AddressSpaceList<[ AddrSpaces.Child ]>;

foreach as = [ "stack", "heap", "parent", "child" ] in {
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
let isCodeGenOnly = 1 in {
// Since all stack addressing modes are SP based, SyncVM needs to insert PSEUDOs
// to adjust for a callee frame size. AdjSPBasedOffsets pass is to replace
// adjusted load and stores with their non-pseudo counterparts.
def AdjSP : Pseudo<(outs GR256:$dst), (ins GR256:$base),
                   "# AdjSP PSEUDO", []>;
def AdjSPDown : Pseudo<(outs GR256:$dst), (ins GR256:$base),
                       "# AdjSPDown PSEUDO", []>;
let mayStore = 1 in {
  def AdjMOVrs      : Pseudo< (outs), (ins GR256:$val, stackop:$dst),
                              "# AdjMOVrs PSEUDO",
                              [(store_stack GR256:$val, adjstackaddr:$dst)]>;
}
let mayLoad = 1 in {
  def AdjMOVsr      : Pseudo< (outs GR256:$val), (ins stackop:$addr),
                              "# AdjMOVsr PSEUDO",
                              [(set GR256:$val, (load_stack adjstackaddr:$addr))]>;
}
}

let isCodeGenOnly = 1, usesCustomInserter = 1 in
  def Select : Pseudo<(outs GR256:$dst), (ins GR256:$src, GR256:$src2, i256imm:$cc),
                       "# Select PSEUDO",
                       [(set GR256:$dst,
                          (SyncVMselectcc GR256:$src, GR256:$src2, imm:$cc))]>;

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
def ADDirr     : SyncVMInstruction< 0x011, (outs GR256:$out),
                                    (ins GR256:$in1, imm128:$in2),
                                    "add\t#$in2, $in1, $out",
                                    [(set GR256:$out, (add GR256:$in1, imm128:$in2))]>;
def MULirr     : SyncVMInstruction< 0x011, (outs GR256:$out),
                                    (ins GR256:$in1, imm128:$in2),
                                    "mul\t#$in2, $in1, $out, r0",
                                    [(set GR256:$out, (mul GR256:$in1, imm128:$in2))]>;
def SUBrir     : SyncVMInstruction< 0x011, (outs GR256:$out),
                                    (ins GR256:$in1, imm128:$in2),
                                    "sub\t$in1, #$in2, $out",
                                    [(set GR256:$out, (add GR256:$in1, imm128:$in2))]>;

let Defs = [Flags], isTerminator = 1 in {
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

//===----------------------------------------------------------------------===//
// Operations with memory and registers
//===----------------------------------------------------------------------===//

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
def Event      : SyncVMInstruction< 0x121, (outs), (ins GR256:$val, GR256:$addr, init_flag:$init),
                                    "evt$init\t$val, $addr",
                                    [(int_syncvm_event GR256:$val, GR256:$addr, imm:$init)]>;
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
    Defs = [R1, R2, R3, R4, R5, R6],
    Uses = [SP] in {
  def CALL : Call<0x43, (outs), (ins i16imm:$callee),
                  "call\t$callee", [(SyncVMcall tglobaladdr:$callee)]>;
  def CALLF : SyncVMInstruction<0x45, (outs), (ins GR256:$call_config),
                                "callf\t$call_config", [(int_syncvm_farcall GR256:$call_config)]>;
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
def POP     : SyncVMInstruction< 0x1a, (outs GR256:$val), (ins i256imm:$size),
                                 "pop\t#$size, $val",
                                 [(set GR256:$val, (SyncVMpop timm:$size))] >;

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
