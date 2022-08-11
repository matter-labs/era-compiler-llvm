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
def SDT_SyncVMInvoke       : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def SDT_SyncVMFarCall      : SDTypeProfile<0, -1, []>;
def SDT_SyncVMCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i256>,
                                             SDTCisVT<1, i256>]>;
def SDT_SyncVMCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i256>,
                                           SDTCisVT<1, i256>]>;
def SDT_SyncVMChangeSP     : SDTypeProfile<0,  1, [SDTCisVT<0, i256>]>;
def SDT_SyncVMGetSP        : SDTypeProfile<1,  0, []>;
def SDT_SyncVMBrCC         : SDTypeProfile<0,  2, [SDTCisVT<0, OtherVT>,
                                                   SDTCisVT<1, i256>]>;
def SDT_SyncVMSelectCC     : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                  SDTCisSameAs<1, 2>,
                                                  SDTCisVT<3, i256>]>;
def SDT_SyncVMCmp          : SDTypeProfile<0, 2, []>;
def SDT_SyncVMThrow        : SDTypeProfile<0, 1, [SDTCisVT<0, i256>]>;

def SDT_SyncVMPtrPack      : SDTypeProfile<1, 2, [SDTCisPtrTy<0>, SDTCisVT<1, i256>]>;
def SDT_SyncVMPtrAdd       : SDTypeProfile<1, 2, [SDTCisPtrTy<0>, SDTCisVT<1, i256>]>;

//===----------------------------------------------------------------------===//
// SyncVM Specific Node Definitions.
//===----------------------------------------------------------------------===//

def SyncVMret    : SDNode<"SyncVMISD::RET", SDTNone,
                    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMcall   : SDNode<"SyncVMISD::CALL", SDT_SyncVMCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMinvoke : SDNode<"SyncVMISD::INVOKE", SDT_SyncVMInvoke,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMfarcall: SDNode<"SyncVMISD::FARCALL", SDT_SyncVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMstaticcall: SDNode<"SyncVMISD::STATICCALL", SDT_SyncVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMdelegatecall: SDNode<"SyncVMISD::DELEGATECALL", SDT_SyncVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def SyncVMmimiccall: SDNode<"SyncVMISD::MIMICCALL", SDT_SyncVMFarCall,
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
def SyncVMget_sp : SDNode<"SyncVMISD::GET_SP", SDT_SyncVMGetSP,
                          [SDNPHasChain]>;
def SyncVMchange_sp : SDNode<"SyncVMISD::CHANGE_SP", SDT_SyncVMChangeSP,
                             [SDNPHasChain]>;
def SyncVMthrow : SDNode<"SyncVMISD::THROW", SDT_SyncVMThrow,
                         [SDNPHasChain, SDNPInGlue]>;
def SyncVMreturn : SDNode<"SyncVMISD::RETURN", SDT_SyncVMThrow,
                         [SDNPHasChain, SDNPInGlue]>;
def SyncVMrevert : SDNode<"SyncVMISD::REVERT", SDT_SyncVMThrow,
                         [SDNPHasChain, SDNPInGlue]>;

def SyncVMPtrPack : SDNode<"SyncVMISD::PTR_PACK", SDT_SyncVMPtrPack, []>;
def SyncVMPtrAdd : SDNode<"SyncVMISD::PTR_ADD", SDT_SyncVMPtrAdd, []>;

//===----------------------------------------------------------------------===//
// SyncVM Operand Definitions.
//===----------------------------------------------------------------------===//

let OperandType = "OPERAND_IMMEDIATE" in
def i256imm : Operand<i256>;

def imm16 : Operand<i256>, IntImmLeaf<i256, [{
  return APInt(256, 1, false).shl(16).ugt(Imm);
}]> {}

def neg_imm16 : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNegative() && Imm.abs().isIntN(16);
}]> {}

def large_imm : Operand<i256>, IntImmLeaf<i256, [{
  return !Imm.abs().isIntN(16) && Imm.isIntN(256);
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

def context_op : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.getZExtValue() < 5;
}]> {
  let PrintMethod = "printContextOperand";
}

def context_se_op : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.getZExtValue() >= 6 && Imm.getZExtValue() < 8;
}]> {
  let PrintMethod = "printContextOperand";
}

def first_op : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.getZExtValue() < 2;
}]> {
  let PrintMethod = "printFirstOperand";
}

def spadv_op : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.getZExtValue() < 2;
}]> {
  let PrintMethod = "printSPAdvanceOperand";
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
// Custom DAG Selection Operations.
//===----------------------------------------------------------------------===//

def negate_imm : SDNodeXForm<imm, [{
  auto& neg = N->getAPIntValue();
  APInt pos = neg;
  pos.negate();
  return CurDAG->getTargetConstant(pos, SDLoc(N), MVT::i256);
}]>;

def constant_pool : SDNodeXForm<imm, [{
  MVT PtrVT = getTargetLowering()->getPointerTy(CurDAG->getDataLayout());
  return CurDAG->getTargetConstantPool(N->getConstantIntValue(), PtrVT);
}]>;

//===----------------------------------------------------------------------===//
// Complex Pattern Definitions.
//===----------------------------------------------------------------------===//

def inreg_memaddr: ComplexPattern<iPTR, 1, "SelectInRegMemAddr", [], []>;
def memaddr      : ComplexPattern<iPTR, 2, "SelectMemAddr", [], []>;
def stackaddr    : ComplexPattern<iPTR, 3, "SelectStackAddr", [], []>;
def adjstackaddr : ComplexPattern<iPTR, 3, "SelectAdjStackAddr", [], []>;

//===----------------------------------------------------------------------===//
// Pattern Fragments Definitions.
//===----------------------------------------------------------------------===//

class AddressSpacesImpl {
  int Stack = 0;
  int Heap = 1;
  int HeapAux = 2;
  int Generic = 3;
  int Code = 4;
}

def AddrSpaces : AddressSpacesImpl;

class AddressSpaceList<list<int> AS> {
  list<int> AddrSpaces = AS;
}

def Address_stack         : AddressSpaceList<[ AddrSpaces.Stack ]>;
def Address_heap          : AddressSpaceList<[ AddrSpaces.Heap ]>;
def Address_heapaux       : AddressSpaceList<[ AddrSpaces.HeapAux ]>;
def Address_generic       : AddressSpaceList<[ AddrSpaces.Generic ]>;
def Address_code          : AddressSpaceList<[ AddrSpaces.Code ]>;

foreach as = [ "stack", "heap", "heapaux", "generic", "code" ] in {
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

let isCodeGenOnly = 1, isPseudo = 1, Uses = [Flags] in {
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

// TODO: restore
// In case Select result is used both in register and stored in stack,
// two nodes are to be glued with comparison SUB and schedulling fails.
/*
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
*/
}

/*
def : Pat<(store_stack (load_stack(SyncVMselectcc stackaddr:$addr1, stackaddr:$addr2, imm:$cc)), stackaddr:$addr3),
          (SELsss stackaddr:$addr1, stackaddr:$addr2, imm:$cc, stackaddr:$addr3)>;
          */
def : Pat<(load_stack(SyncVMselectcc stackaddr:$addr1, stackaddr:$addr2, imm:$cc)),
          (SELssr stackaddr:$addr1, stackaddr:$addr2, imm:$cc)>;
def : Pat<(load_code(SyncVMselectcc memaddr:$addr1, memaddr:$addr2, imm:$cc)),
          (SELccr memaddr:$addr1, memaddr:$addr2, imm:$cc)>;

// TODO: stack and code forms
def : Pat<(int_syncvm_ifeq GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, 2 /*COND_LT*/)>;
def : Pat<(int_syncvm_iflt GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, 4 /*COND_LT*/)>;
def : Pat<(int_syncvm_ifgt GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, 8 /*COND_LT*/)>;

def : Pat<(int_syncvm_ifeq imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, 2 /*COND_LT*/)>;
def : Pat<(int_syncvm_iflt imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, 4 /*COND_LT*/)>;
def : Pat<(int_syncvm_ifgt imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, 8 /*COND_LT*/)>;

let hasSideEffects = 1, Defs = [SP], Uses = [SP] in {
def NOPSP : INop< 18, (ins spadv_op:$sp_advance, cc:$cc), "nop$cc\t$sp_advance", [] >;
def NOPSPr : INop< 181, (ins GR256:$sp_advance, cc:$cc), "nop$cc\tstack+=[$sp_advance]", [] >;
}
def : Pat<(SyncVMchange_sp spadv_op:$size), (NOPSP spadv_op:$size, 0)>;
def : Pat<(SyncVMchange_sp GR256:$size), (NOPSPr GR256:$size, 0)>;

def zext256 : PatFrag<(ops node:$src), (zext (i256 node:$src))>;

multiclass Arith<bits<8> opcode, string asmstring, SDPatternOperator node, bit commutes> {
  let BaseOpcode = asmstring, isCommutable = commutes in {

  let isPseudo = 1, isCodeGenOnly = 1 in
  def rrr_p : Pseudo<(outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                     !strconcat(asmstring, "\t$rs0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs0, GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndRR.Value;
                     }
  def rrr_s : Irrr<opcode, 0, 1,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$rs0, $rs1, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def rrr_v : Irrr<opcode, 0, 0,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$rs0, $rs1, $rd0"), []>, FlagRel;

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def crr_p : Pseudo<(outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                     !strconcat(asmstring, "\t$src0[0], $rs1, $rd0"),
                     [(set GR256:$rd0, (node (load_code memaddr:$src0), GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndCR.Value;
                     }
  def crr_s : Imrr<opcode, SrcCode, 0, 1,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0[0], $rs1, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def crr_v : Imrr<opcode, SrcCode, 0, 0,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0[0], $rs1, $rd0"), []>, FlagRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def srr_p : Pseudo<(outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1),
                     !strconcat(asmstring, "\t$src0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node (load_stack stackaddr:$src0), GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndSR.Value;
                     }
  def srr_s : Isrr<opcode, SrcStack, 0, 1,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def srr_v : Isrr<opcode, SrcStack, 0, 0,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0"), []>, FlagRel;
  }
  
  let mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def rrs_p : Pseudo<(outs), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$rs0, $rs1, $dst0"),
                     [(store_stack (node GR256:$rs0, GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndRR.Value;
                     }
  def rrs_s : Irrs<opcode, 0, 1,
                   (outs), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$rs0, $rs1, $dst0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def rrs_v : Irrs<opcode, 0, 0,
                   (outs), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$rs0, $rs1, $dst0"), []>, FlagRel;
  }

  let mayLoad = 1, mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def crs_p : Pseudo<(outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$src0[0], $rs1, $dst0"),
                     [(store_stack (node (load_code memaddr:$src0), GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndCR.Value;
                     }
  def crs_s : Imrs<opcode, SrcCode, 0, 1,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0[0], $rs1, $dst0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def crs_v : Imrs<opcode, SrcCode, 0, 0,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0[0], $rs1, $dst0"), []>, FlagRel;

  let isPseudo = 1, isCodeGenOnly = 1 in
  def srs_p : Pseudo<(outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$src0, $rs1, $dst0"),
                     [(store_stack (node (load_stack stackaddr:$src0), GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndSR.Value;
                     }
  def srs_s : Isrs<opcode, SrcStack, 0, 1,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel;

  let Defs = [Flags] in
  def srs_v : Isrs<opcode, SrcStack, 0, 0,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0"), []>, FlagRel;
  }
  }
}

multiclass ArithICommutable<bits<8> opcode, string asmstring, SDPatternOperator node> : Arith<opcode, asmstring, node, 1>{
  let BaseOpcode = asmstring, isCommutable = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def irr_p : Pseudo<(outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                     !strconcat(asmstring, "\t$imm, $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs1, imm16:$imm))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndIR.Value;
                     }
  def irr_s : Iirr<opcode, 0, 1,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def irr_v : Iirr<opcode, 0, 0,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$imm, $rs1, $rd0"), []>, FlagRel;
  let mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def irs_p : Pseudo<(outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$imm, $rs1, $dst0"),
                     [(store_stack (node GR256:$rs1, imm16:$imm), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndIR.Value;
                     }
  def irs_s : Iirs<opcode, 0, 1,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def irs_v : Iirs<opcode, 0, 0,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$imm, $rs1, $dst0"), []>, FlagRel;
  }
  } // end isCommutable = 1

  // commutative: irr_p -> crr_p
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # crr_p) (constant_pool imm:$imm), 0, GR256:$rs1)>;
  // commutative: irs_p -> crs_p
  def : Pat<(store_stack (node GR256:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # crs_p) (constant_pool imm:$imm), 0, GR256:$rs1, stackaddr:$dst0)>;
}

multiclass ArithINonCommutable<bits<8> opcode, string asmstring, SDPatternOperator node> : Arith<opcode, asmstring, node, 0> {
  let BaseOpcode = asmstring in {
  let isPseudo = 1, isCodeGenOnly = 1 in {
  def irr_p : Pseudo<(outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                     !strconcat(asmstring, "\t$imm, $rs1, $rd0"),
                     [(set GR256:$rd0, (node imm16:$imm, GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndIR.Value;
                     }
  def xrr_p : Pseudo<(outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                     !strconcat(asmstring, ".s\t$imm, $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs1, imm16:$imm))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndIR.Value;
                      let ReverseOperand = 1;
                     }
  }
  
  def irr_s : Iirr<opcode, 0, 1,
                       (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel;
  def xrr_s : Iirr<opcode, 1, 1,
                       (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"),
                                  "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel;

  foreach swap = ["i", "x"] in
  let Defs = [Flags] in
  def swap#rr_v : Iirr<opcode, !if(!eq(swap, "i"), 0, 1), 0,
                       (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, !if(!eq(swap, "i"), "", ".s")),
                                  "$cc!\t$imm, $rs1, $rd0"), []>;
  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrr_p : Pseudo<(outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                     !strconcat(asmstring, ".s\t$src0[0], $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs1, (load_code memaddr:$src0)))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndCR.Value;
                      let ReverseOperand = 1;
                     }
  def yrr_s : Imrr<opcode, SrcCode, 1, 1,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0[0], $rs1, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def yrr_v : Imrr<opcode, SrcCode, 1, 0,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0[0], $rs1, $rd0"), []>, FlagRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def zrr_p : Pseudo<(outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs1, (load_stack stackaddr:$src0)))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndSR.Value;
                      let ReverseOperand = 1;
                     }
  def zrr_s : Isrr<opcode, SrcStack, 1, 1,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def zrr_v : Isrr<opcode, SrcStack, 1, 0,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0"), []>, FlagRel;
  }
  let mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in {
  def irs_p : Pseudo<(outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$imm, $rs1, $dst0"),
                     [(store_stack (node imm16:$imm, GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndIR.Value;
                     }
  def xrs_p : Pseudo<(outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$imm, $rs1, $dst0"),
                     [(store_stack (node GR256:$rs1, imm16:$imm), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndIR.Value;
                      let ReverseOperand = 1;
                     }
  }
  def irs_s : Iirs<opcode, 0, 1,
                       (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel;

  def xrs_s : Iirs<opcode, 1, 1,
                       (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"), "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel;

  foreach swap = ["i", "x"] in
  let Defs = [Flags] in
  def swap#rs_v : Iirs<opcode, !if(!eq(swap, "i"), 0, 1), 0,
                       (outs), (ins imm16:$imm, GR256:$rs1, GR256:$dst0, cc:$cc),
                       !strconcat(!strconcat(asmstring, !if(!eq(swap, "i"), "", ".s")),
                                  "$cc!\t$imm, $rs1, $dst0"), []>, FlagRel;
  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrs_p : Pseudo<(outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$src0[0], $rs1, $dst0"),
                     [(store_stack (node GR256:$rs1, (load_code memaddr:$src0)), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndCR.Value;
                      let ReverseOperand = 1;
                     }

  def yrs_s : Imrs<opcode, SrcCode, 1, 1,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0[0], $rs1, $dst0"), []>, PseudoRel, FlagRel;

  let Defs = [Flags] in
  def yrs_v : Imrs<opcode, SrcCode, 1, 0,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0[0], $rs1, $dst0"), []>, FlagRel;

  let isPseudo = 1, isCodeGenOnly = 1 in
  def zrs_p : Pseudo<(outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $dst0"),
                     [(store_stack (node GR256:$rs1, (load_stack stackaddr:$src0)), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndSR.Value;
                      let ReverseOperand = 1;
                     }
  def zrs_s : Isrs<opcode, SrcStack, 1, 1,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def zrs_v : Isrs<opcode, SrcStack, 1, 0,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0"), []>, FlagRel;
  }
  }
  }

  // non-commutative: irr_p -> crr_p
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # crr_p) (constant_pool imm:$imm), 0, GR256:$rs1)>;

  // xrr_p -> yrr_p
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # yrr_p) (constant_pool imm:$imm), 0, GR256:$rs1)>;

  // non-commutative: irs_p -> crs_p
  def : Pat<(store_stack (node GR256:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # crs_p) (constant_pool imm:$imm), 0, GR256:$rs1, stackaddr:$dst0)>;

  // xrs_p -> yrs_p
  def : Pat<(store_stack (node GR256:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # yrs_p) (constant_pool imm:$imm), 0, GR256:$rs1, stackaddr:$dst0)>;

}

multiclass Arith2<bits<8> opcode, string asmstring, SDPatternOperator node, bit commutes> {
  let BaseOpcode = asmstring, isCommutable = commutes in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def rrrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1),
                      !strconcat(asmstring, "\t$rs0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs0, GR256:$rs1))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndRR.Value;
                      }
  def rrrr_s : Irrrr<opcode, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$rs0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def rrrr_v : Irrrr<opcode, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$rs0, $rs1, $rd0, $rd1"), []>, FlagRel;

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def crrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                      !strconcat(asmstring, "\t$src0[0], $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node (load_code memaddr:$src0), GR256:$rs1))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndCR.Value;
                      }
  def crrr_s : Imrrr<opcode, SrcCode, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0[0], $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def crrr_v : Imrrr<opcode, SrcCode, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0[0], $rs1, $rd0, $rd1"), []>, FlagRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def srrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1),
                      !strconcat(asmstring, "\t$src0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node (load_stack stackaddr:$src0), GR256:$rs1))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndSR.Value;
                      }
  def srrr_s : Isrrr<opcode, SrcStack, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def srrr_v : Isrrr<opcode, SrcStack, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0, $rd1"), []>, FlagRel;
  }

  let mayStore = 1 in {
  def rrsr_s : Irrsr<opcode, 0, 1,
                     (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$rs0, $rs1, $dst0, $rd1"), []>, FlagRel;
  let Defs = [Flags] in
  def rrsr_v : Irrsr<opcode, 0, 0,
                     (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$rs0, $rs1, $dst0, $rd1"), []>, FlagRel;
  let mayLoad = 1 in {
  def crsr_s : Imrsr<opcode, SrcCode, 0, 1,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0[0], $rs1, $dst0, $rd1"), []>, FlagRel;
  let Defs = [Flags] in
  def crsr_v : Imrsr<opcode, SrcCode, 0, 0,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0[0], $rs1, $dst0, $rd1"), []>, FlagRel;
  def srsr_s : Isrsr<opcode, SrcStack, 0, 1,
                     (outs GR256:$rd1), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0, $rd1"), []>, FlagRel;
  let Defs = [Flags] in
  def srsr_v : Isrsr<opcode, SrcStack, 0, 0,
                     (outs GR256:$rd1), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0, $rd1"), []>, FlagRel;
  }
  }
  }
}

multiclass Arith2ICommutable<bits<8> opcode, string asmstring, SDPatternOperator node>
         : Arith2<opcode, asmstring, node, 1> {
  let BaseOpcode = asmstring, isCommutable = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def irrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                      !strconcat(asmstring, "\t$imm, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, imm16:$imm))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndIR.Value;
                      }
  def irrr_s : Iirrr<opcode, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def irrr_v : Iirrr<opcode, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$imm, $rs1, $rd0, $rd1"), []>, FlagRel;
  let mayStore = 1 in {
  def irsr_s : Iirsr<opcode, 0, 1,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0, $rd1"), []>, FlagRel;
  let Defs = [Flags] in
  def irsr_v : Iirsr<opcode, 0, 0,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$imm, $rs1, $dst0, $rd1"), []>, FlagRel;
  }
  }

  // commutative: rrrr_p -> crrr_p
  def : Pat<(node GR256:$rs1, large_imm:$imm), 
            (!cast<Instruction>(NAME # crrr_p) (constant_pool imm:$imm), 0, GR256:$rs1)>;
}

multiclass Arith2INonCommutable<bits<8> opcode, string asmstring, SDPatternOperator node>
         : Arith2<opcode, asmstring, node, 0> {
  let BaseOpcode = asmstring in {
  let isPseudo = 1, isCodeGenOnly = 1 in {
  def irrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                       !strconcat(asmstring, "\t$imm, $rs1, $rd0, $rd1"),
                       [(set GR256:$rd0, GR256:$rd1, (node imm16:$imm, GR256:$rs1))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndIR.Value;
                       }
  def xrrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                       !strconcat(asmstring, ".s\t$imm, $rs1, $rd0, $rd1"),
                       [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, imm16:$imm))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndIR.Value;
                        let ReverseOperand = 1;
                       }
  }
  def irrr_s : Iirrr<opcode, 0, 1,
                       (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  def xrrr_s : Iirrr<opcode, 1, 1,
                       (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"), "$cc\t$imm, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  }

  foreach swap = ["i", "x"] in
  let Defs = [Flags] in
  def swap#rrr_v : Iirrr<opcode, !if(!eq(swap, "i"), 0, 1), 0,
                       (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, !if(!eq(swap, "i"), "", ".s")),
                                  "$cc!\t$imm, $rs1, $rd0, $rd1"), []>, FlagRel;

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                      !strconcat(asmstring, ".s\t$src0[0], $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, (load_code memaddr:$src0)))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndCR.Value;
                        let ReverseOperand = 1;
                      }
  def yrrr_s : Imrrr<opcode, SrcCode, 1, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0[0], $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def yrrr_v : Imrrr<opcode, SrcCode, 1, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, ".s$cc!\t$src0[0], $rs1, $rd0, $rd1"), []>, FlagRel;
  let isPseudo = 1 in
  def zrrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1),
                      !strconcat(asmstring, ".s\t$src0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, (load_stack stackaddr:$src0)))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndSR.Value;
                        let ReverseOperand = 1;
                      }
  def zrrr_s : Isrrr<opcode, SrcStack, 1, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def zrrr_v : Isrrr<opcode, SrcStack, 1, 0,
                   (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0, $rd1"), []>, FlagRel;
  }
  let mayStore = 1 in {
  foreach swap = ["i", "x"] in
  def swap#rsr_s : Iirsr<opcode, !if(!eq(swap, "i"), 0, 1), 1,
                         (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                         !strconcat(!strconcat(asmstring, !if(!eq(swap, "i"), "", ".s")),
                                    "$cc\t$imm, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel;
  foreach swap = ["i", "x"] in
  let Defs = [Flags] in
  def swap#rsr_v : Iirsr<opcode, !if(!eq(swap, "i"), 0, 1), 0,
                         (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, GR256:$dst0, cc:$cc),
                         !strconcat(!strconcat(asmstring, !if(!eq(swap, "i"), "", ".s")),
                                    "$cc!\t$imm, $rs1, $dst0, $rd1"), []>, FlagRel;
  let mayLoad = 1 in {
  def yrsr_s : Imrsr<opcode, SrcCode, 1, 1,
                     (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0[0], $rs1, $dst0, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def yrsr_v : Imrsr<opcode, SrcCode, 1, 0,
                     (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc!\t$src0[0], $rs1, $dst0, $rd0"), []>, FlagRel;
  def zrsr_s : Isrsr<opcode, SrcStack, 1, 1,
                     (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0, $rd0"), []>, PseudoRel, FlagRel;
  let Defs = [Flags] in
  def zrsr_v : Isrsr<opcode, SrcStack, 1, 0,
                     (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0, $rd0"), []>, FlagRel;
  }
  }

  // non-commutative:
  def : Pat<(node large_imm:$imm, GR256:$rs1), 
            (!cast<Instruction>(NAME # crrr_p) (constant_pool imm:$imm), 0, GR256:$rs1)>;
  def : Pat<(node GR256:$rs1, large_imm:$imm), 
            (!cast<Instruction>(NAME # yrrr_p) (constant_pool imm:$imm), 0, GR256:$rs1)>;
}

defm ADD  : ArithICommutable<2,  "add", add>;
defm SUB  : ArithINonCommutable<4, "sub", sub>;


// pointer arithmetic
def PTR_PACK : Pseudo<(outs GR256:$out),
                      (ins GR256:$ptr, GR256:$rs2),
                      "ptr.pack\t $ptr, $rs2",
                      [(set GR256:$out, (SyncVMPtrPack GR256:$ptr, GR256:$rs2))]>;
defm PTR_ADD  : ArithINonCommutable<9, "ptr.add", SyncVMPtrAdd>;

// MOV patterns
def : Pat<(store_stack (load_code memaddr:$src), stackaddr:$dst),  (ADDcrs_p memaddr:$src, R0, stackaddr:$dst)>;
def : Pat<(store_stack (load_stack stackaddr:$src), stackaddr:$dst),  (ADDsrs_p stackaddr:$src, R0, stackaddr:$dst)>;
def : Pat<(imm16:$val),  (ADDirr_p imm16:$val, R0)>;
def : Pat<(load_code memaddr:$addr),  (ADDcrr_p memaddr:$addr, R0)>;
def : Pat<(load_stack stackaddr:$addr),  (ADDsrr_p stackaddr:$addr, R0)>;
def : Pat<(store_stack GR256:$src, stackaddr:$dst),  (ADDrrs_p GR256:$src, R0, stackaddr:$dst)>;
def : Pat<(store_stack imm16:$src, stackaddr:$dst),  (ADDirs_p imm16:$src, R0, stackaddr:$dst)>;

// SelecCC, BR_CC supplement
def : Pat<(SyncVMcmp GR256:$lhs, GR256:$rhs), (SUBrrr_v GR256:$lhs, GR256:$rhs, 0)>;
def : Pat<(SyncVMcmp GR256:$lhs, imm16:$rhs), (SUBxrr_v imm:$rhs, GR256:$lhs, 0)>;
def : Pat<(SyncVMcmp GR256:$lhs, large_imm:$rhs), (SUByrr_v (constant_pool imm:$rhs), 0, GR256:$lhs, 0)>;

def : Pat<(SyncVMcmp (load_code memaddr:$lhs), GR256:$rhs), (SUBcrr_v memaddr:$lhs, GR256:$rhs, 0)>;
def : Pat<(SyncVMcmp GR256:$lhs, (load_code memaddr:$rhs)), (SUByrr_v memaddr:$rhs, GR256:$lhs, 0)>;
def : Pat<(SyncVMcmp (load_stack stackaddr:$lhs), GR256:$rhs), (SUBsrr_v stackaddr:$lhs, GR256:$rhs, 0)>;
def : Pat<(SyncVMcmp GR256:$lhs, (load_stack stackaddr:$rhs)), (SUBzrr_v stackaddr:$rhs, GR256:$lhs, 0)>;

defm MUL  : Arith2ICommutable<6, "mul", umullohi>;
defm DIV  : Arith2INonCommutable<8, "div", udivrem>;

// FIXME: Wrong encoding
// FIXME: Wrong flags
defm AND  : ArithICommutable<33, "and", and>;
defm OR   : ArithICommutable<34, "or", or>;
defm XOR  : ArithICommutable<35, "xor", xor>;
defm SHL  : ArithINonCommutable<36, "shl", shl>;
defm SHR  : ArithINonCommutable<37, "shr", srl>;
defm ROL  : ArithINonCommutable<38, "rol", rotl>;
defm ROR  : ArithINonCommutable<39, "ror", rotr>;

//===----------------------------------------------------------------------===//
// Memory operations
//===----------------------------------------------------------------------===//

let mayLoad = 1 in {
def LD1  : IUMA < 41, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "ld.1$cc\t$rs0, $rd0", []>;
def LD2  : IUMA < 42, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "ld.2$cc\t$rs0, $rd0", []>;
def LD   : IUMA < 43, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "ld$cc\t$rs0, $rd0", []>;
}
def     : Pat<(load_heap GR256:$rs0), (LD1 GR256:$rs0, 0)>;
def     : Pat<(load_heapaux GR256:$rs0), (LD2 GR256:$rs0, 0)>;
def     : Pat<(load_generic GR256:$rs0), (LD GR256:$rs0, 0)>;

let mayStore = 1 in {
def ST1  : IUMA < 44, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                  "st.1$cc\t$rs0, $rs1", []>;
def ST2  : IUMA < 54, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                  "st.2$cc\t$rs0, $rs1", []>;
}
def     : Pat<(store_heap GR256:$rs1, GR256:$rs0),
              (ST1 GR256:$rs0, $rs1, 0)>;
def     : Pat<(store_heapaux GR256:$rs1, GR256:$rs0),
              (ST2 GR256:$rs0, $rs1, 0)>;

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
let isTerminator = 1, isBranch = 1, isBarrier = 1, Uses = [Flags] in
def J: IJump<10, (ins jmptarget:$dest, cc:$cc), "jump$cc\t$dest",
             [(SyncVMbrcc bb:$dest, imm:$cc)]>;
def : Pat<(br bb:$dst), (J bb:$dst, 0)>;

let isReturn = 1, isTerminator = 1, isBarrier = 1  in
def RET   : IRet<7, RFNone, (ins), "ret", [(SyncVMret)]>;
let isTerminator = 1, isBarrier = 1, isTrap = 1 in
def THROW : IRet<7, RFErr, (ins GR256:$rs0), "revert",
                 [(SyncVMthrow GR256:$rs0)]>;
let isTerminator = 1, isBarrier = 1 in {
def RETURN : IRet<52, RFErr, (ins GR256:$rs0), "ret.ok.to_label\t$rs0, @DEFAULT_FAR_RETURN",
                  [(SyncVMreturn GR256:$rs0)]>;
def REVERT : IRet<53, RFErr, (ins GR256:$rs0), "ret.revert.to_label\t$rs0, @DEFAULT_FAR_REVERT",
                  [(SyncVMrevert GR256:$rs0)]>;
}

//===----------------------------------------------------------------------===//
// Constants materialization
//===----------------------------------------------------------------------===//

let isReMaterializable = 1, hasSideEffects = 0, isPseudo = 1 in {
def LOADCONST : Pseudo<(outs GR256:$val), (ins i256imm:$addr),
                         "LOADCONST\t$addr, $val",
                         [(set GR256:$val, (load tconstpool:$addr))]>;
def CONST   : Pseudo< (outs GR256:$out), (ins i256imm:$val),
                      "CONST\t$val", [(set GR256:$out, imm:$val)] >;
}

//===----------------------------------------------------------------------===//
// Intrinsics lowering
//===----------------------------------------------------------------------===//

def CTXr : IContextR<45, (outs GR256:$rd0), (ins context_op:$cop, cc:$cc),
                     "context$cop$cc\t$rd0", []>;
let hasSideEffects = 1 in {
def CTXr_se : IContextR<46, (outs GR256:$rd0), (ins context_se_op:$cop, cc:$cc),
                        "context$cop$cc\t$rd0", []>;
def CTXr_set : IContextR<52, (outs), (ins GR256:$rs0, context_se_op:$cop, cc:$cc),
                         "context$cop$cc\t$rs0", []>;
}

def : Pat<(int_syncvm_this), (CTXr 0, 0)>;
def : Pat<(int_syncvm_caller), (CTXr 1, 0)>;
def : Pat<(int_syncvm_codesource), (CTXr 2, 0)>;
def : Pat<(int_syncvm_meta), (CTXr 3, 0)>;
def : Pat<(int_syncvm_txorigin), (CTXr 4, 0)>;
def : Pat<(int_syncvm_ergsleft), (CTXr_se 6, 0)>;
def : Pat<(SyncVMget_sp), (CTXr_se 7, 0)>;
def : Pat<(int_syncvm_getu128), (CTXr_se 8, 0)>;
def : Pat<(int_syncvm_setu128 GR256:$rs0), (CTXr_set GR256:$rs0, 9, 0)>;

def SLDrr : ILogR<47, DstReg, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "sload$cc\t$rs0, $rd0", []> {
  bits<4> rd0;
  let rs1 = 0;
  let Inst{40-43} = rd0;
}
//===----------------------------------------------------------------------===//
// Fat Pointer
//===----------------------------------------------------------------------===//
def : Pat<(int_syncvm_sload GR256:$rs0), (SLDrr GR256:$rs0, 0)>;

let hasSideEffects = 1 in
  def SSTr : ILogR<48, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
      "sstore$cc\t$rs0, $rs1", []>;

def : Pat<(int_syncvm_sstore GR256:$rs0, GR256:$rs1),
          (SSTr GR256:$rs0, GR256:$rs1, 0)>;

let hasSideEffects = 1 in
def L1r : ILogR<49, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, first_op:$first, cc:$cc),
                 "to_l1$first$cc\t$rs0, $rs1", []>;

def : Pat<(int_syncvm_tol1 GR256:$rs0, GR256:$rs1, first_op:$first),
          (L1r GR256:$rs0, GR256:$rs1, first_op:$first, 0)>;

let hasSideEffects = 1 in
def EVTr : ILogR<50, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, first_op:$first, cc:$cc),
                 "event$first$cc\t$rs0, $rs1", []>;

def : Pat<(int_syncvm_event GR256:$rs0, GR256:$rs1, first_op:$first),
          (EVTr GR256:$rs0, GR256:$rs1, first_op:$first, 0)>;

let hasSideEffects = 1 in
def PCOMPr : ILogR<51, DstNone, (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                 "precompile$cc\t$rs0, $rs1, $rd0", []> {
  let rs1 = 0;
}

def : Pat<(int_syncvm_precompile GR256:$rs0, GR256:$rs1),
          (PCOMPr GR256:$rs0, GR256:$rs1, 0)>;

let Defs = [R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15],
    Uses = [SP], isCall = 1 in {
   def CALL : Pseudo<(outs), (ins GR256:$in1, imm16:$callee),
                   "CALL\t$in1, $callee", [(SyncVMcall GR256:$in1, tglobaladdr:$callee)]>;
   let isTerminator = 1 in
   def INVOKE : Pseudo<(outs), (ins GR256:$in1, imm16:$callee, jmptarget:$unwind),
                   "INVOKE\t$in1, $callee, $unwind", [(SyncVMinvoke GR256:$in1, tglobaladdr:$callee, bb:$unwind)]>;

   def NEAR_CALL : ICall<9, CFNormal, (ins GR256:$in1, imm16:$callee, jmptarget:$unwind),
                   "near_call\t$in1, $callee, $unwind", []>;
}

let Defs = [R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15],
    isCall = 1, isTerminator = 1 in {
def FAR_CALL  : ICall<8, CFNormal, (ins GR256:$abi, GR256:$address, jmptarget:$unwind),
               "far_call\t$abi, $address, $unwind", [(SyncVMfarcall GR256:$abi, GR256:$address, bb:$unwind)]>;
def STATIC_CALL  : ICall<10, CFNormal, (ins GR256:$abi, GR256:$address, jmptarget:$unwind),
               "far_call.static\t$abi, $address, $unwind", [(SyncVMstaticcall GR256:$abi, GR256:$address, bb:$unwind)]>;
def DELEGATE_CALL  : ICall<11, CFDelegate, (ins GR256:$abi, GR256:$address, jmptarget:$unwind),
               "far_call.delegate\t$abi, $address, $unwind", [(SyncVMdelegatecall GR256:$abi, GR256:$address, bb:$unwind)]>;
def MIMIC_CALL  : ICall<12, CFMimic, (ins GR256:$abi, GR256:$address, GR256:$mimic, jmptarget:$unwind),
               "far_call.mimic\t$abi, $address, $unwind", [(SyncVMmimiccall GR256:$abi, GR256:$address, GR256:$mimic, bb:$unwind)]>;
}

def : Pat<(store_stack (add GR256:$rs0, neg_imm16:$imm), stackaddr:$dst0),
          (SUBxrs_p (negate_imm imm:$imm), GR256:$rs0, stackaddr:$dst0)>;
def : Pat<(sub GR256:$rs0, neg_imm16:$imm), (ADDirr_p (negate_imm imm:$imm), GR256:$rs0)>;
def : Pat<(add GR256:$rs0, neg_imm16:$imm), (SUBxrr_p (negate_imm imm:$imm), GR256:$rs0)>;
