//===-- EraVMInstrInfo.td - EraVM Instruction defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the EraVM instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "EraVMInstrFormats.td"

//===----------------------------------------------------------------------===//
// Type Profiles.
//===----------------------------------------------------------------------===//
def SDT_EraVMCall         : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def SDT_EraVMInvoke       : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def SDT_EraVMFarCall      : SDTypeProfile<0, -1, []>;
def SDT_EraVMCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i256>,
                                             SDTCisVT<1, i256>]>;
def SDT_EraVMCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i256>,
                                           SDTCisVT<1, i256>]>;
def SDT_EraVMWrapper      : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                                                  SDTCisPtrTy<0>]>;
def SDT_EraVMChangeSP     : SDTypeProfile<0,  1, [SDTCisVT<0, i256>]>;
def SDT_EraVMGetSP        : SDTypeProfile<1,  0, []>;
def SDT_EraVMBrCC         : SDTypeProfile<0,  2, [SDTCisVT<0, OtherVT>,
                                                   SDTCisVT<1, i256>]>;
def SDT_EraVMSelectCC     : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                  SDTCisSameAs<1, 2>,
                                                  SDTCisVT<3, i256>]>;
def SDT_EraVMCmp          : SDTypeProfile<0, 2, []>;
def SDT_EraVMThrow        : SDTypeProfile<0, 1, [SDTCisVT<0, i256>]>;

def SDT_EraVMCopyFromPtrReg : SDTypeProfile<1, 1, [SDTCisVT<0, fatptr>]>;

def SDT_EraVMPtrToInt     : SDTypeProfile<1, 1, [SDTCisVT<0, i256>, SDTCisVT<1, fatptr>]>;
def SDT_EraVMPtrOp        : SDTypeProfile<1, 2, [SDTCisVT<0, fatptr>, SDTCisVT<1, fatptr>, SDTCisVT<2, i256>]>;
def SDT_EraVMLogDecommit  : SDTypeProfile<1, 2, [SDTCisVT<0, fatptr>, SDTCisVT<1, i256>, SDTCisVT<2, i256>]>;

//===----------------------------------------------------------------------===//
// EraVM Specific Node Definitions.
//===----------------------------------------------------------------------===//

def EraVMret    : SDNode<"EraVMISD::RET", SDTNone,
                    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def EraVMcall   : SDNode<"EraVMISD::CALL", SDT_EraVMCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EraVMinvoke : SDNode<"EraVMISD::INVOKE", SDT_EraVMInvoke,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EraVMfarcall: SDNode<"EraVMISD::FARCALL", SDT_EraVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EraVMstaticcall: SDNode<"EraVMISD::STATICCALL", SDT_EraVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EraVMdelegatecall: SDNode<"EraVMISD::DELEGATECALL", SDT_EraVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EraVMmimiccall: SDNode<"EraVMISD::MIMICCALL", SDT_EraVMFarCall,
                    [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EraVMbrcc   : SDNode<"EraVMISD::BR_CC", SDT_EraVMBrCC,
                    [SDNPHasChain, SDNPInGlue]>;
def EraVMselectcc: SDNode<"EraVMISD::SELECT_CC", SDT_EraVMSelectCC,
                           [SDNPInGlue]>;
def EraVMcmp    : SDNode<"EraVMISD::CMP", SDT_EraVMCmp, [SDNPOutGlue]>;
def EraVMcallseq_start :
                 SDNode<"ISD::CALLSEQ_START", SDT_EraVMCallSeqStart,
                        [SDNPHasChain, SDNPOutGlue]>;
def EraVMcallseq_end :
                 SDNode<"ISD::CALLSEQ_END",   SDT_EraVMCallSeqEnd,
                        [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def EraVMGAStack : SDNode<"EraVMISD::GAStack", SDT_EraVMWrapper>;
def EraVMGACode : SDNode<"EraVMISD::GACode", SDT_EraVMWrapper>;
def EraVMget_sp : SDNode<"EraVMISD::GET_SP", SDT_EraVMGetSP,
                          [SDNPHasChain]>;
def EraVMchange_sp : SDNode<"EraVMISD::CHANGE_SP", SDT_EraVMChangeSP,
                             [SDNPHasChain]>;
def EraVMthrow : SDNode<"EraVMISD::THROW", SDT_EraVMThrow,
                         [SDNPHasChain, SDNPInGlue]>;
def EraVMreturn : SDNode<"EraVMISD::RETURN", SDT_EraVMThrow,
                         [SDNPHasChain, SDNPInGlue]>;
def EraVMrevert : SDNode<"EraVMISD::REVERT", SDT_EraVMThrow,
                         [SDNPHasChain, SDNPInGlue]>;

def EraVMcopy_from_ptrreg : SDNode<"EraVMISD::COPY_FROM_PTRREG",
                                    SDT_EraVMCopyFromPtrReg,
                                    [SDNPHasChain, SDNPOptInGlue]>;

def EraVMptr_to_int   : SDNode<"EraVMISD::PTR_TO_INT", SDT_EraVMPtrToInt, []>;
def EraVMptr_add      : SDNode<"EraVMISD::PTR_ADD", SDT_EraVMPtrOp, []>;
def EraVMptr_pack     : SDNode<"EraVMISD::PTR_PACK", SDT_EraVMPtrOp, []>;
def EraVMptr_shrink   : SDNode<"EraVMISD::PTR_SHRINK", SDT_EraVMPtrOp, []>;
def EraVMlog_decommit : SDNode<"EraVMISD::LOG_DECOMMIT", SDT_EraVMLogDecommit, [SDNPHasChain]>;

def EraVMTrap : SDNode<"EraVMISD::TRAP", SDTNone,
    [SDNPHasChain, SDNPOptInGlue]>;

//===----------------------------------------------------------------------===//
// EraVM Operand Definitions.
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

def init_flag : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNullValue() || Imm.isOneValue();
}]> {
  let PrintMethod = "printInitOperand";
}

// Operand for printing out a condition code.
def cc : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isIntN(4) && Imm != 0;
}]> {
  let PrintMethod = "printCCOperand";
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
  let MIOperandInfo = (ops GRStack, GR256, i16imm);
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
  int Storage = 5;
  int Transient = 6;
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
def Address_storage       : AddressSpaceList<[ AddrSpaces.Storage ]>;
def Address_transient     : AddressSpaceList<[ AddrSpaces.Transient ]>;

foreach as = [ "stack", "heap", "heapaux", "generic", "code", "storage", "transient" ] in {
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
let isCodeGenOnly = 1, Defs = [SP], Uses = [SP] in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
                              "#ADJCALLSTACKDOWN $amt1 $amt2",
                              [(EraVMcallseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : Pseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
                              "#ADJCALLSTACKUP $amt1 $amt2",
                              [(EraVMcallseq_end timm:$amt1, timm:$amt2)]>;
}

let isCodeGenOnly = 1, Defs = [Flags], Uses = [SP] in {
def ADDframe  : Pseudo<(outs GR256:$dst), (ins i256imm:$base, i256imm:$offset),
                       "# ADDframe PSEUDO", []>;
def FRAMEirrr : Pseudo<(outs GR256:$dst), (ins i256imm:$imm, GR256:$src, GR256:$sp),
                       "# FRAMEirrr PSEUDO", []>;
}

let isCodeGenOnly = 1, isPseudo = 1, isSelect = 1, Uses = [Flags] in {
  def SELrrr : Pseudo<(outs GR256:$dst), (ins GR256:$src, GR256:$src2, i256imm:$cc),
                       "# SELrrr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, GR256:$src2, imm:$cc))]>;

  def FATPTR_SELrrr : Pseudo<(outs GRPTR:$dst), (ins GRPTR:$src, GRPTR:$src2, i256imm:$cc),
                             "# FATPTR_SELrrr PSEUDO",
                             [(set GRPTR:$dst,
                               (EraVMselectcc GRPTR:$src, GRPTR:$src2, imm:$cc))]>;

  def SELirr : Pseudo<(outs GR256:$dst), (ins imm16:$src, GR256:$src2, i256imm:$cc),
                       "# SELirr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, GR256:$src2, imm:$cc))]> {
                            let Constraints = "$dst = $src2";
                          }
let mayLoad = 1 in {
  def SELcrr : Pseudo<(outs GR256:$dst), (ins memop:$src, GR256:$src2, i256imm:$cc),
                       "# SELcrr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), GR256:$src2, imm:$cc))]> {
                            let Constraints = "$dst = $src2";
                          }
  def SELsrr : Pseudo<(outs GR256:$dst), (ins stackop:$src, GR256:$src2, i256imm:$cc),
                       "# SELsrr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), GR256:$src2, imm:$cc))]> {
                            let Constraints = "$dst = $src2";
                          }
}
  def SELrir : Pseudo<(outs GR256:$dst), (ins GR256:$src, imm16:$src2, i256imm:$cc),
                       "# SELrir PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, imm16:$src2, imm:$cc))]> {
                            let Constraints = "$dst = $src";
                          }
  def SELiir : Pseudo<(outs GR256:$dst), (ins imm16:$src, imm16:$src2, i256imm:$cc),
                       "# SELiir PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, imm16:$src2, imm:$cc))]>;
let mayLoad = 1 in {
  def SELcir : Pseudo<(outs GR256:$dst), (ins memop:$src, imm16:$src2, i256imm:$cc),
                       "# SELcir PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), imm16:$src2, imm:$cc))]>;
  def SELsir : Pseudo<(outs GR256:$dst), (ins stackop:$src, imm16:$src2, i256imm:$cc),
                       "# SELsir PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), imm16:$src2, imm:$cc))]>;
  def SELrcr : Pseudo<(outs GR256:$dst), (ins GR256:$src, memop:$src2, i256imm:$cc),
                       "# SELrcr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, (load_code memaddr:$src2), imm:$cc))]> {
                            let Constraints = "$dst = $src";
                          }
  def SELicr : Pseudo<(outs GR256:$dst), (ins imm16:$src, memop:$src2, i256imm:$cc),
                       "# SELicr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, (load_code memaddr:$src2), imm:$cc))]>;
  def SELccr : Pseudo<(outs GR256:$dst), (ins memop:$src, memop:$src2, i256imm:$cc),
                       "# SELccr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), (load_code memaddr:$src2), imm:$cc))]>;
  def SELscr : Pseudo<(outs GR256:$dst), (ins stackop:$src, memop:$src2, i256imm:$cc),
                       "# SELscr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), (load_code memaddr:$src2), imm:$cc))]>;
  def SELrsr : Pseudo<(outs GR256:$dst), (ins GR256:$src, stackop:$src2, i256imm:$cc),
                       "# SELrsr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, (load_stack stackaddr:$src2), imm:$cc))]> {
                            let Constraints = "$dst = $src";
                          }
  def SELisr : Pseudo<(outs GR256:$dst), (ins imm16:$src, stackop:$src2, i256imm:$cc),
                       "# SELisr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELcsr : Pseudo<(outs GR256:$dst), (ins memop:$src, stackop:$src2, i256imm:$cc),
                       "# SELcsr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELssr : Pseudo<(outs GR256:$dst), (ins stackop:$src, stackop:$src2, i256imm:$cc),
                       "# SELssr PSEUDO",
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), (load_stack stackaddr:$src2), imm:$cc))]>;
}

}

// To be able to fold (op in0, (select.cc x, y)) to (op in0, y) + (op.cc in0, y) for
// non-register in0 reduce immediate 0 to r0 in select.
def : Pat<(EraVMselectcc 0, GR256:$src2, imm:$cc), (SELrrr R0, GR256:$src2, imm:$cc)>;
def : Pat<(EraVMselectcc GR256:$src1, 0, imm:$cc), (SELrrr GR256:$src1, R0, imm:$cc)>;

def : Pat<(load_stack(EraVMselectcc stackaddr:$addr1, stackaddr:$addr2, imm:$cc)),
          (SELssr stackaddr:$addr1, stackaddr:$addr2, imm:$cc)>;
def : Pat<(load_code(EraVMselectcc memaddr:$addr1, memaddr:$addr2, imm:$cc)),
          (SELccr memaddr:$addr1, memaddr:$addr2, imm:$cc)>;

// TODO: CPR-1356 stack and code forms
def : Pat<(int_eravm_ifeq GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, 2 /*COND_EQ*/)>;
def : Pat<(int_eravm_iflt GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, 4 /*COND_LT*/)>;
def : Pat<(int_eravm_ifgt GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, 8 /*COND_GT*/)>;

def : Pat<(int_eravm_ifeq imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, 2 /*COND_EQ*/)>;
def : Pat<(int_eravm_iflt imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, 4 /*COND_LT*/)>;
def : Pat<(int_eravm_ifgt imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, 8 /*COND_GT*/)>;

let hasSideEffects = 1, Defs = [SP], Uses = [SP] in {
def NOPSP : INop< 18, (ins spadv_op:$sp_advance, cc:$cc), "nop$cc\t$sp_advance", [] >;
def NOPSPr : INop< 181, (ins GR256:$sp_advance, cc:$cc), "nop$cc\tstack+=[$sp_advance]", [] >;
}
def : Pat<(EraVMchange_sp spadv_op:$size), (NOPSP spadv_op:$size, 0)>;
def : Pat<(EraVMchange_sp GR256:$size), (NOPSPr GR256:$size, 0)>;

def zext256 : PatFrag<(ops node:$src), (zext (i256 node:$src))>;

multiclass Arith<bits<8> opcode, string asmstring, SDPatternOperator node, bit commutes> {
  let BaseOpcode = asmstring, isCommutable = commutes in {

  let isPseudo = 1, isCodeGenOnly = 1 in
  def rrr_p : Pseudo<(outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                     !strconcat(asmstring, "\t$rs0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs0, GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndRR.Value;
                      let OperandAM = 0;
                     }
  def rrr_s : Irrr<opcode, 0, 1,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$rs0, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def rrr_v : Irrr<opcode, 0, 0,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$rs0, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def crr_p : Pseudo<(outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                     !strconcat(asmstring, "\t$src0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node (load_code memaddr:$src0), GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndCR.Value;
                     }
  def crr_s : Imrr<opcode, SrcCode, 0, 1,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def crr_v : Imrr<opcode, SrcCode, 0, 0,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def srr_p : Pseudo<(outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1),
                     !strconcat(asmstring, "\t$src0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node (load_stack stackaddr:$src0), GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndSR.Value;
                     }
  def srr_s : Isrr<opcode, SrcStack, 0, 1,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def srr_v : Isrr<opcode, SrcStack, 0, 0,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
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
                   !strconcat(asmstring, "$cc\t$rs0, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def rrs_v : Irrs<opcode, 0, 0,
                   (outs), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$rs0, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  }

  let mayLoad = 1, mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def crs_p : Pseudo<(outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$src0, $rs1, $dst0"),
                     [(store_stack (node (load_code memaddr:$src0), GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndCR.Value;
                     }
  def crs_s : Imrs<opcode, SrcCode, 0, 1,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def crs_v : Imrs<opcode, SrcCode, 0, 0,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;

  let isPseudo = 1, isCodeGenOnly = 1 in
  def srs_p : Pseudo<(outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$src0, $rs1, $dst0"),
                     [(store_stack (node (load_stack stackaddr:$src0), GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndSR.Value;
                     }
  def srs_s : Isrs<opcode, SrcStack, 0, 1,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;

  let Defs = [Flags] in
  def srs_v : Isrs<opcode, SrcStack, 0, 0,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
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
                   !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def irr_v : Iirr<opcode, 0, 0,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$imm, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
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
                   !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def irs_v : Iirs<opcode, 0, 0,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$imm, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
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
                       !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrr_s : Iirr<opcode, 1, 1,
                       (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"),
                                  "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel, RetAddrModeRel;

  let Defs = [Flags] in {
  def irr_v : Iirr<opcode, 0, 0,
                       (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(asmstring, "$cc!\t$imm, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrr_v : Iirr<opcode, 1, 0,
                       (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(asmstring, ".s$cc!\t$imm, $rs1, $rd0"), []>, FlagRel, RetAddrModeRel;
  }

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrr_p : Pseudo<(outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $rd0"),
                     [(set GR256:$rd0, (node GR256:$rs1, (load_code memaddr:$src0)))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndCR.Value;
                      let ReverseOperand = 1;
                     }
  def yrr_s : Imrr<opcode, SrcCode, 1, 1,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def yrr_v : Imrr<opcode, SrcCode, 1, 0,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, RetAddrModeRel;
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
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def zrr_v : Isrr<opcode, SrcStack, 1, 0,
                   (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, RetAddrModeRel;
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
                       !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;

  def xrs_s : Iirs<opcode, 1, 1,
                       (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"), "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel, RetAddrModeRel;

  let Defs = [Flags] in {
  def irs_v : Iirs<opcode, 0, 0,
                       (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(asmstring, "$cc!\t$imm, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrs_v : Iirs<opcode, 1, 0,
                       (outs), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(asmstring, ".s$cc!\t$imm, $rs1, $dst0"), []>, FlagRel, RetAddrModeRel;
  }
  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrs_p : Pseudo<(outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $dst0"),
                     [(store_stack (node GR256:$rs1, (load_code memaddr:$src0)), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndCR.Value;
                      let ReverseOperand = 1;
                     }

  def yrs_s : Imrs<opcode, SrcCode, 1, 1,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, RetAddrModeRel;

  let Defs = [Flags] in
  def yrs_v : Imrs<opcode, SrcCode, 1, 0,
                   (outs), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, RetAddrModeRel;

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
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def zrs_v : Isrs<opcode, SrcStack, 1, 0,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, RetAddrModeRel;
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

multiclass PtrInstr<bits<8> opcode, string asmstring, SDPatternOperator node> {
  let BaseOpcode = asmstring in {

  let isPseudo = 1, isCodeGenOnly = 1 in
  def rrr_p : Pseudo<(outs GRPTR:$rd0), (ins GRPTR:$rs0, GR256:$rs1),
                     !strconcat(asmstring, "\t$rs0, $rs1, $rd0"),
                     [(set GRPTR:$rd0, (node GRPTR:$rs0, GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndRR.Value;
                     }
  def rrr_s : Irrr<opcode, 0, 1,
                   (outs GRPTR:$rd0), (ins GRPTR:$rs0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$rs0, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def rrr_v : Irrr<opcode, 0, 0,
                   (outs GRPTR:$rd0), (ins GRPTR:$rs0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$rs0, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def srr_p : Pseudo<(outs GRPTR:$rd0), (ins stackop:$src0, GR256:$rs1),
                     !strconcat(asmstring, "\t$src0, $rs1, $rd0"),
                     [(set GRPTR:$rd0, (node (load_stack stackaddr:$src0), GR256:$rs1))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndSR.Value;
                     }
  def srr_s : Isrr<opcode, SrcStack, 0, 1,
                   (outs GRPTR:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def srr_v : Isrr<opcode, SrcStack, 0, 0,
                   (outs GRPTR:$rd0), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  }
  
  let mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def rrs_p : Pseudo<(outs), (ins GRPTR:$rs0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$rs0, $rs1, $dst0"),
                     [(store_stack (node GRPTR:$rs0, GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndRR.Value;
                     }
  def rrs_s : Irrs<opcode, 0, 1,
                   (outs), (ins GRPTR:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$rs0, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def rrs_v : Irrs<opcode, 0, 0,
                   (outs), (ins GRPTR:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$rs0, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  }

  let mayLoad =1, mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def srs_p : Pseudo<(outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0),
                     !strconcat(asmstring, "\t$src0, $rs1, $dst0"),
                     [(store_stack (node (load_stack stackaddr:$src0), GR256:$rs1), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndSR.Value;
                     }
  def srs_s : Isrs<opcode, SrcStack, 0, 1,
                   (outs), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def srs_v : Isrs<opcode, SrcStack, 0, 0,
                   (outs), (ins GRPTR:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  }

  let isPseudo = 1, isCodeGenOnly = 1 in {
  def xrr_p : Pseudo<(outs GRPTR:$rd0), (ins imm16:$imm, GRPTR:$rs1),
                     !strconcat(asmstring, ".s\t$imm, $rs1, $rd0"),
                     [(set GRPTR:$rd0, (node GRPTR:$rs1, imm16:$imm))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndIR.Value;
                      let ReverseOperand = 1;
                     }
  }
  def xrr_s : Iirr<opcode, 1, 1,
                       (outs GRPTR:$rd0), (ins imm16:$imm, GRPTR:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"),
                                  "$cc\t$imm, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;

  let Defs = [Flags] in
  def xrr_v : Iirr<opcode, 1, 0,
                       (outs GRPTR:$rd0), (ins imm16:$imm, GRPTR:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"),
                                  "$cc!\t$imm, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrr_p : Pseudo<(outs GRPTR:$rd0), (ins memop:$src0, GRPTR:$rs1),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $rd0"),
                     [(set GRPTR:$rd0, (node GRPTR:$rs1, (load_code memaddr:$src0)))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndCR.Value;
                      let ReverseOperand = 1;
                     }
  def yrr_s : Imrr<opcode, SrcCode, 1, 1,
                   (outs GRPTR:$rd0), (ins memop:$src0, GRPTR:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;

  let Defs = [Flags] in
  def yrr_v : Imrr<opcode, SrcCode, 1, 0,
                   (outs GRPTR:$rd0), (ins memop:$src0, GRPTR:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;

  let isPseudo = 1, isCodeGenOnly = 1 in
  def zrr_p : Pseudo<(outs GRPTR:$rd0), (ins stackop:$src0, GRPTR:$rs1),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $rd0"),
                     [(set GRPTR:$rd0, (node GRPTR:$rs1, (load_stack stackaddr:$src0)))]> {
                      let DestAddrMode = ToReg.Value;
                      let OperandAddrMode = OpndSR.Value;
                      let ReverseOperand = 1;
                     }

  def zrr_s : Isrr<opcode, SrcStack, 1, 1,
                   (outs GRPTR:$rd0), (ins stackop:$src0, GRPTR:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0"), []>, PseudoRel, FlagRel, RetAddrModeRel;

  let Defs = [Flags] in
  def zrr_v : Isrr<opcode, SrcStack, 1, 0,
                   (outs GRPTR:$rd0), (ins stackop:$src0, GRPTR:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0"), []>, FlagRel, RetAddrModeRel;
  }

  let mayStore = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in {
  def xrs_p : Pseudo<(outs), (ins imm16:$imm, GRPTR:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$imm, $rs1, $dst0"),
                     [(store_stack (node GRPTR:$rs1, imm16:$imm), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndIR.Value;
                      let ReverseOperand = 1;
                     }
  }

  def xrs_s : Iirs<opcode, 1, 1,
                       (outs), (ins imm16:$imm, GRPTR:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"), "$cc\t$imm, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;

  let Defs = [Flags] in
  def xrs_v : Iirs<opcode, 1, 0,
                       (outs), (ins imm16:$imm, GRPTR:$rs1, stackop:$dst0, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"),
                                  "$cc!\t$imm, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrs_p : Pseudo<(outs), (ins memop:$src0, GRPTR:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $dst0"),
                     [(store_stack (node GRPTR:$rs1, (load_code memaddr:$src0)), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndCR.Value;
                      let ReverseOperand = 1;
                     }

  def yrs_s : Imrs<opcode, SrcCode, 1, 1,
                   (outs), (ins memop:$src0, GRPTR:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;

  let Defs = [Flags] in
  def yrs_v : Imrs<opcode, SrcCode, 1, 0,
                   (outs), (ins memop:$src0, GRPTR:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, AddrModeRel, RetAddrModeRel;

  let isPseudo = 1, isCodeGenOnly = 1 in
  def zrs_p : Pseudo<(outs), (ins stackop:$src0, GRPTR:$rs1, stackop:$dst0),
                     !strconcat(asmstring, ".s\t$src0, $rs1, $dst0"),
                     [(store_stack (node GRPTR:$rs1, (load_stack stackaddr:$src0)), stackaddr:$dst0)]> {
                      let DestAddrMode = ToStack.Value;
                      let OperandAddrMode = OpndSR.Value;
                      let ReverseOperand = 1;
                     }

  def zrs_s : Isrs<opcode, SrcStack, 1, 1,
                   (outs), (ins stackop:$src0, GRPTR:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0"), []>, PseudoRel, FlagRel, RetAddrModeRel;

  let Defs = [Flags] in
  def zrs_v : Isrs<opcode, SrcStack, 1, 0,
                   (outs), (ins stackop:$src0, GRPTR:$rs1, stackop:$dst0, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0"), []>, FlagRel, RetAddrModeRel;
  }
  }
  }

  // xrr_p -> yrr_p
  def : Pat<(node GRPTR:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # yrr_p) (constant_pool imm:$imm), 0, GRPTR:$rs1)>;

  // xrs_p -> yrs_p
  def : Pat<(store_stack (node GRPTR:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # yrs_p) (constant_pool imm:$imm), 0, GRPTR:$rs1, stackaddr:$dst0)>;
}

// This is a bridge to convert GRPTR type to GR256 type. It will get eliminated in a post RA pass.
def PTR_TO_INT : Pseudo<(outs GR256:$rd0), (ins GRPTR:$rs0),
                        "ptrtoint\t$rs0, $rd0",
                        [(set GR256:$rd0, (EraVMptr_to_int GRPTR:$rs0))]> {
                            let Constraints = "$rd0 = $rs0";
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
                     !strconcat(asmstring, "$cc\t$rs0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def rrrr_v : Irrrr<opcode, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$rs0, $rs1, $rd0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def crrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                      !strconcat(asmstring, "\t$src0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node (load_code memaddr:$src0), GR256:$rs1))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndCR.Value;
                      }
  def crrr_s : Imrrr<opcode, SrcCode, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def crrr_v : Imrrr<opcode, SrcCode, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def srrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1),
                      !strconcat(asmstring, "\t$src0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node (load_stack stackaddr:$src0), GR256:$rs1))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndSR.Value;
                      }
  def srrr_s : Isrrr<opcode, SrcStack, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def srrr_v : Isrrr<opcode, SrcStack, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0, $rs1, $rd0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  }

  let mayStore = 1 in {
  def rrsr_p : Pseudo<(outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0),
                      !strconcat(asmstring, "\t$rs0, $rs1, $dst0, $rd1"), []> {
                        let DestAddrMode = ToStackReg.Value;
                        let OperandAddrMode = OpndRR.Value;
                      }
  def rrsr_s : Irrsr<opcode, 0, 1,
                     (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$rs0, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def rrsr_v : Irrsr<opcode, 0, 0,
                     (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$rs0, $rs1, $dst0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  let mayLoad = 1 in {
  def crsr_p : Pseudo<(outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackop:$dst0),
                      !strconcat(asmstring, "\t$src0, $rs1, $dst0, $rd1"), []> {
                        let DestAddrMode = ToStackReg.Value;
                        let OperandAddrMode = OpndCR.Value;
                      }
  def crsr_s : Imrsr<opcode, SrcCode, 0, 1,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def crsr_v : Imrsr<opcode, SrcCode, 0, 0,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  def srsr_p : Pseudo<(outs GR256:$rd1), (ins stackop:$src0, GR256:$rs1, stackop:$dst0),
                      !strconcat(asmstring, "\t$src0, $rs1, $dst0, $rd1"), []> {
                        let DestAddrMode = ToStackReg.Value;
                        let OperandAddrMode = OpndSR.Value;
                        let ReverseOperand = 0;
                      }
  def srsr_s : Isrsr<opcode, SrcStack, 0, 1,
                     (outs GR256:$rd1), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$src0, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def srsr_v : Isrsr<opcode, SrcStack, 0, 0,
                     (outs GR256:$rd1), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$src0, $rs1, $dst0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  }
  }
  }
}

multiclass Arith2ICommutable<bits<8> opcode, string asmstring, SDPatternOperator node>
         : Arith2<opcode, asmstring, node, 1> {
  let BaseOpcode = asmstring, isCommutable = 1 in {
  def irrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                      !strconcat(asmstring, "\t$imm, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, imm16:$imm))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndIR.Value;
                      }
  def irrr_s : Iirrr<opcode, 0, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def irrr_v : Iirrr<opcode, 0, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$imm, $rs1, $rd0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  let mayStore = 1 in {
  def irsr_p : Pseudo<(outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0),
                      !strconcat(asmstring, "\t$imm, $rs1, $dst0, $rd1"), []> {
                        let DestAddrMode = ToStackReg.Value;
                        let OperandAddrMode = OpndIR.Value;
                      }
  def irsr_s : Iirsr<opcode, 0, 1,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  let Defs = [Flags] in
  def irsr_v : Iirsr<opcode, 0, 0,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, "$cc!\t$imm, $rs1, $dst0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
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
                       !strconcat(asmstring, "$cc\t$imm, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrrr_s : Iirrr<opcode, 1, 1,
                       (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(!strconcat(asmstring, ".s"), "$cc\t$imm, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, RetAddrModeRel;

  let Defs = [Flags] in {
  def irrr_v : Iirrr<opcode, 0, 0,
                       (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(asmstring, "$cc!\t$imm, $rs1, $rd0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrrr_v : Iirrr<opcode, 1, 0,
                       (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1, cc:$cc),
                       !strconcat(asmstring, ".s$cc!\t$imm, $rs1, $rd0, $rd1"), []>, FlagRel, RetAddrModeRel;
  }

  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                      !strconcat(asmstring, ".s\t$src0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, (load_code memaddr:$src0)))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndCR.Value;
                        let ReverseOperand = 1;
                      }
  def yrrr_s : Imrrr<opcode, SrcCode, 1, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def yrrr_v : Imrrr<opcode, SrcCode, 1, 0,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0, $rd1"), []>, FlagRel, RetAddrModeRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def zrrr_p : Pseudo<(outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1),
                      !strconcat(asmstring, ".s\t$src0, $rs1, $rd0, $rd1"),
                      [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, (load_stack stackaddr:$src0)))]> {
                        let DestAddrMode = ToRegReg.Value;
                        let OperandAddrMode = OpndSR.Value;
                        let ReverseOperand = 1;
                      }
  def zrrr_s : Isrrr<opcode, SrcStack, 1, 1,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0, $rs1, $rd0, $rd1"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def zrrr_v : Isrrr<opcode, SrcStack, 1, 0,
                   (outs GR256:$rd0, GR256:$rd1), (ins stackop:$src0, GR256:$rs1, cc:$cc),
                   !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $rd0, $rd1"), []>, FlagRel, RetAddrModeRel;
  }
  let mayStore = 1 in {
  foreach swap = ["i", "x"] in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def swap#rsr_p : Pseudo<(outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0), 
                          !strconcat(!strconcat(asmstring, !if(!eq(swap, "i"), "", ".s")),
                                    "\t$imm, $rs1, $dst0, $rd1"), []> {
      let DestAddrMode = ToStackReg.Value;
      let OperandAddrMode = OpndIR.Value;
      let ReverseOperand = !if(!eq(swap, "i"), 0, 1);
    }
  }
  def irsr_s : Iirsr<opcode, 0, 1,
                         (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                         !strconcat(asmstring, "$cc\t$imm, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrsr_s : Iirsr<opcode, 1, 1,
                         (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                         !strconcat(asmstring, ".s$cc\t$imm, $rs1, $dst0, $rd1"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in {
  def irsr_v : Iirsr<opcode, 0, 0,
                         (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                         !strconcat(asmstring, "$cc!\t$imm, $rs1, $dst0, $rd1"), []>, FlagRel, AddrModeRel, RetAddrModeRel;
  def xrsr_v : Iirsr<opcode, 1, 0,
                         (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackop:$dst0, cc:$cc),
                         !strconcat(asmstring, ".s$cc!\t$imm, $rs1, $dst0, $rd1"), []>, FlagRel, RetAddrModeRel;
  }
  let mayLoad = 1 in {
  let isPseudo = 1, isCodeGenOnly = 1 in
  def yrsr_p : Pseudo<(outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, stackop:$dst0),
                      !strconcat(asmstring, ".s\t$src0, $rs1, $dst0, $rd0"), []> {
                        let DestAddrMode = ToStackReg.Value;
                        let OperandAddrMode = OpndCR.Value;
                        let ReverseOperand = 1;
                      }
  def yrsr_s : Imrsr<opcode, SrcCode, 1, 1,
                     (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0, $rd0"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def yrsr_v : Imrsr<opcode, SrcCode, 1, 0,
                     (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0, $rd0"), []>, FlagRel, RetAddrModeRel;
  let isPseudo = 1, isCodeGenOnly = 1 in
  def zrsr_p : Pseudo<(outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, stackop:$dst0),
                      !strconcat(asmstring, ".s\t$src0, $rs1, $dst0, $rd0"), []> {
                        let DestAddrMode = ToStackReg.Value;
                        let OperandAddrMode = OpndSR.Value;
                        let ReverseOperand = 1;
                      }
  def zrsr_s : Isrsr<opcode, SrcStack, 1, 1,
                     (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc\t$src0, $rs1, $dst0, $rd0"), []>, PseudoRel, FlagRel, RetAddrModeRel;
  let Defs = [Flags] in
  def zrsr_v : Isrsr<opcode, SrcStack, 1, 0,
                     (outs GR256:$rd0), (ins stackop:$src0, GR256:$rs1, stackop:$dst0, cc:$cc),
                     !strconcat(asmstring, ".s$cc!\t$src0, $rs1, $dst0, $rd0"), []>, FlagRel, RetAddrModeRel;
  }
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
defm PTR_ADD    : PtrInstr<159, "ptr.add", EraVMptr_add>;
defm PTR_PACK   : PtrInstr<160, "ptr.pack", EraVMptr_pack>;
defm PTR_SHRINK : PtrInstr<161, "ptr.shrink", EraVMptr_shrink>;

// MOV patterns
def : Pat<(store_stack (load_code memaddr:$src), stackaddr:$dst),  (ADDcrs_p memaddr:$src, R0, stackaddr:$dst)>;
def : Pat<(store_stack (load_stack stackaddr:$src), stackaddr:$dst),  (ADDsrs_p stackaddr:$src, R0, stackaddr:$dst)>;
def : Pat<(imm16:$val),  (ADDirr_p imm16:$val, R0)>;
def : Pat<(load_code memaddr:$addr),  (ADDcrr_p memaddr:$addr, R0)>;
def : Pat<(load_stack stackaddr:$addr),  (ADDsrr_p stackaddr:$addr, R0)>;
def : Pat<(store_stack GR256:$src, stackaddr:$dst),  (ADDrrs_p GR256:$src, R0, stackaddr:$dst)>;
def : Pat<(store_stack imm16:$src, stackaddr:$dst),  (ADDirs_p imm16:$src, R0, stackaddr:$dst)>;

// fat pointer move patterns
def : Pat<(fatptr (load_stack stackaddr:$addr)),  (PTR_ADDsrr_p stackaddr:$addr, R0)>;
def : Pat<(store_stack GRPTR:$src, stackaddr:$dst),  (PTR_ADDrrs_p GRPTR:$src, R0, stackaddr:$dst)>;

// fat pointer load/store
def : Pat<(store_stack GRPTR:$src, stackaddr:$dst), (PTR_ADDrrs_p GRPTR:$src, R0, stackaddr:$dst)>;

def : Pat<(EraVMcopy_from_ptrreg GRPTR:$src), (PTR_ADDrrr_p GRPTR:$src, R0)>;

// SelecCC, BR_CC supplement
def : Pat<(EraVMcmp GR256:$lhs, GR256:$rhs), (SUBrrr_v GR256:$lhs, GR256:$rhs, 0)>;
// r0 is more profitable than imm 0 because it makes sub! x, r0 combinable with x = load y.
def : Pat<(EraVMcmp GR256:$lhs, 0), (SUBrrr_v GR256:$lhs, R0, 0)>;
def : Pat<(EraVMcmp GR256:$lhs, imm16:$rhs), (SUBxrr_v imm:$rhs, GR256:$lhs, 0)>;
def : Pat<(EraVMcmp GR256:$lhs, large_imm:$rhs), (SUByrr_v (constant_pool imm:$rhs), 0, GR256:$lhs, 0)>;

def : Pat<(EraVMcmp (load_code memaddr:$lhs), GR256:$rhs), (SUBcrr_v memaddr:$lhs, GR256:$rhs, 0)>;
def : Pat<(EraVMcmp GR256:$lhs, (load_code memaddr:$rhs)), (SUByrr_v memaddr:$rhs, GR256:$lhs, 0)>;
def : Pat<(EraVMcmp (load_stack stackaddr:$lhs), GR256:$rhs), (SUBsrr_v stackaddr:$lhs, GR256:$rhs, 0)>;
def : Pat<(EraVMcmp GR256:$lhs, (load_stack stackaddr:$rhs)), (SUBzrr_v stackaddr:$rhs, GR256:$lhs, 0)>;

defm MUL  : Arith2ICommutable<6, "mul", umullohi>;
defm DIV  : Arith2INonCommutable<8, "div", udivrem>;

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
def LD1     : IUMA <41, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "ld.1$cc\t$rs0, $rd0", []>;
def LD1Inc  : IUMA <42, (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, cc:$cc),
                  "ld.1.inc$cc\t$rs0, $rd0, $rd1", []>;
def LD1i    : IUMA < 141, (outs GR256:$rd0), (ins imm16:$addr, cc:$cc),
                  "ld.1$cc\t$addr, $rd0", []>;
def LD2     : IUMA < 43, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "ld.2$cc\t$rs0, $rd0", []>;
def LD2Inc  : IUMA <44, (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, cc:$cc),
                  "ld.2.inc$cc\t$rs0, $rd0, $rd1", []>;
def LD2i    : IUMA < 142, (outs GR256:$rd0), (ins imm16:$addr, cc:$cc),
                  "ld.2$cc\t$addr, $rd0", []>;
def LD      : IUMA <45, (outs GR256:$rd0), (ins GRPTR:$rs0, cc:$cc),
                  "ld$cc\t$rs0, $rd0", []>;
def LDInc   : IUMA <46, (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, cc:$cc),
                  "ld.inc$cc\t$rs0, $rd0, $rd1", []>;
// Pseudo alias version of `LD` which has different type than `LD`
// We will manually select it when we see that we are loading a fat pointer
def FATPTR_LD : IUMA < 48, (outs GRPTR:$rd0), (ins GRPTR:$rs0, cc:$cc),
                  "ld$cc\t$rs0, $rd0 ;fat load", []>;
}

def     : Pat<(load_heap GR256:$rs0), (LD1 GR256:$rs0, 0)>;
def     : Pat<(load_heap imm16:$addr), (LD1i imm16:$addr, 0)>;
def     : Pat<(load_heapaux GR256:$rs0), (LD2 GR256:$rs0, 0)>;
def     : Pat<(load_heapaux imm16:$addr), (LD2i imm16:$addr, 0)>;

let mayStore = 1 in {
def ST1     : IUMA <49, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                  "st.1$cc\t$rs0, $rs1", []>;
def ST1Inc  : IUMA <50, (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                  "st.1.inc$cc\t$rs0, $rs1, $rd0", []>;
def ST1i  : IUMA < 144, (outs), (ins imm16:$addr, GR256:$rs1, cc:$cc),
                  "st.1$cc\t$addr, $rs1", []>;
def ST2  : IUMA < 54, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                  "st.2$cc\t$rs0, $rs1", []>;
def ST2Inc  : IUMA <53, (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                  "st.2.inc$cc\t$rs0, $rs1, $rd0", []>;
def ST2i  : IUMA < 154, (outs), (ins imm16:$addr, GR256:$rs1, cc:$cc),
                  "st.2$cc\t$addr, $rs1", []>;
}
def     : Pat<(store_heap GR256:$rs1, GR256:$rs0),
              (ST1 GR256:$rs0, $rs1, 0)>;
def     : Pat<(store_heap GR256:$rs1, imm16:$addr),
              (ST1i imm16:$addr, $rs1, 0)>;
def     : Pat<(store_heapaux GR256:$rs1, GR256:$rs0),
              (ST2 GR256:$rs0, $rs1, 0)>;
def     : Pat<(store_heapaux GR256:$rs1, imm16:$addr),
              (ST2i imm16:$addr, $rs1, 0)>;
//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
// Conditional jump
let isTerminator = 1, isBranch = 1, Uses = [Flags] in
def JC: ICondJump<10, (ins jmptarget:$dest, cc:$cc), "jump$cc\t$dest",
             [(EraVMbrcc bb:$dest, imm:$cc)]>;

// Unconditional jump
let isTerminator = 1, isBranch = 1, isBarrier = 1 in {
def J: IJump<10, SrcNone, (ins jmptarget:$dest), "jump\t$dest",
             [(EraVMbrcc bb:$dest, 0)]>;
def J_s: IJump<10, SrcStack, (ins stackop:$dest), "jump\t$dest", [] >;
}
def : Pat<(br bb:$dst), (J bb:$dst)>;

// Dynamic jumps
let isPseudo = 1, isCall = 1 in
def JCALL: IJump<10, SrcNone, (ins jmptarget:$dest), "jump\t$dest", [] >;

let isReturn = 1, isTerminator = 1, isBarrier = 1  in {
def RET   : IRet<7, RFNone, (ins), "ret", [(EraVMret)]>;
def PANIC : IRet<7, RFPanic, (ins GR256:$rs0), "ret.panic\t$rs0", []>;
}
let isTerminator = 1, isBarrier = 1, isTrap = 1 in
def THROW : IRet<7, RFErr, (ins GR256:$rs0), "ret.error\t$rs0",
                 [(EraVMthrow GR256:$rs0)]>;

def : InstAlias<"revert", (THROW R1)>;
def : InstAlias<"panic", (PANIC R1)>;

let isTerminator = 1, isBarrier = 1, isReturn = 1 in {
def RETURN : IRet<52, RFErr, (ins GR256:$rs0), "ret.ok.to_label\t$rs0, @DEFAULT_FAR_RETURN",
                  [(EraVMreturn GR256:$rs0)]>;
def REVERT : IRet<53, RFErr, (ins GR256:$rs0), "ret.revert.to_label\t$rs0, @DEFAULT_FAR_REVERT",
                  [(EraVMrevert GR256:$rs0)]>;
}

//===----------------------------------------------------------------------===//
// Constants materialization
//===----------------------------------------------------------------------===//

let isAsCheapAsAMove = 1, mayLoad = 1, isReMaterializable = 1,
    hasSideEffects = 0, isPseudo = 1 in {
def LOADCONST : Pseudo<(outs GR256:$val), (ins i256imm:$addr),
                         "LOADCONST\t$addr, $val",
                         [(set GR256:$val, (load tconstpool:$addr))]>;
}

//===----------------------------------------------------------------------===//
// Intrinsics lowering
//===----------------------------------------------------------------------===//

// TODO: CPR-1505 Regroup instructions based on the attributes from IntrinsicsEraVM.td.
def CTXr : IContextR<45, (outs GR256:$rd0), (ins context_op:$cop, cc:$cc),
                     "context$cop$cc\t$rd0", []>;
let hasSideEffects = 1 in {
let hasPostISelHook = 1 in
def CTXr_se : IContextR<46, (outs GR256:$rd0), (ins context_se_op:$cop, cc:$cc),
                        "context$cop$cc\t$rd0", []>;
def CTXr_set : IContextR<52, (outs), (ins GR256:$rs0, context_se_op:$cop, cc:$cc),
                         "context$cop$cc\t$rs0", []>;
def CTXr_se0 : IContextR<152, (outs), (ins context_se_op:$cop, cc:$cc),
                         "context$cop$cc", []>;
}

def : Pat<(int_eravm_meta), (CTXr 3, 0)>;
def : Pat<(int_eravm_this), (CTXr 0, 0)>;
def : Pat<(int_eravm_caller), (CTXr 1, 0)>;
def : Pat<(int_eravm_codesource), (CTXr 2, 0)>;
def : Pat<(int_eravm_txorigin), (CTXr 4, 0)>;
def : Pat<(int_eravm_gasleft), (CTXr_se 6, 0)>;
def : Pat<(EraVMget_sp), (CTXr_se 7, 0)>;
def : Pat<(int_eravm_getu128), (CTXr_se 8, 0)>;
def : Pat<(int_eravm_setu128 GR256:$rs0), (CTXr_set GR256:$rs0, 9, 0)>;
def : Pat<(int_eravm_inctx), (CTXr_se0 10, 0)>;
def : Pat<(int_eravm_setpubdataprice GR256:$rs0), (CTXr_set GR256:$rs0, 11, 0)>;

def SLDrr : ILogR<47, DstReg, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "sload$cc\t$rs0, $rd0", []> {
  bits<4> rd0;
  let rs1 = 0;
  let Inst{40-43} = rd0;
}

def TLDrr : ILogR<471, DstReg, (outs GR256:$rd0), (ins GR256:$rs0, cc:$cc),
                  "tload$cc\t$rs0, $rd0", []> {
  bits<4> rd0;
  let rs1 = 0;
  let Inst{40-43} = rd0;
}
//===----------------------------------------------------------------------===//
// Fat Pointer
//===----------------------------------------------------------------------===//
def : Pat<(load_storage GR256:$rs0), (SLDrr GR256:$rs0, 0)>;
def : Pat<(load_transient GR256:$rs0), (TLDrr GR256:$rs0, 0)>;

let hasSideEffects = 1 in {
  def SSTr : ILogR<48, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
      "sstore$cc\t$rs0, $rs1", []>;
  def TSTr : ILogR<481, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
      "tstore$cc\t$rs0, $rs1", []>;
}

def : Pat<(store_storage GR256:$rs1, GR256:$rs0),
          (SSTr GR256:$rs0, GR256:$rs1, 0)>;
def : Pat<(store_transient GR256:$rs1, GR256:$rs0),
          (TSTr GR256:$rs0, GR256:$rs1, 0)>;

let hasSideEffects = 1 in
def L1r : ILogR<49, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, first_op:$first, cc:$cc),
                 "to_l1$first$cc\t$rs0, $rs1", []>;

def : Pat<(int_eravm_tol1 GR256:$rs0, GR256:$rs1, first_op:$first),
          (L1r GR256:$rs0, GR256:$rs1, first_op:$first, 0)>;

let hasSideEffects = 1 in
def EVTr : ILogR<50, DstNone, (outs), (ins GR256:$rs0, GR256:$rs1, first_op:$first, cc:$cc),
                 "event$first$cc\t$rs0, $rs1", []>;

def : Pat<(int_eravm_event GR256:$rs0, GR256:$rs1, first_op:$first),
          (EVTr GR256:$rs0, GR256:$rs1, first_op:$first, 0)>;

let hasSideEffects = 1 in {
def PCOMPr : ILogR<51, DstNone, (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                   "precompile$cc\t$rs0, $rs1, $rd0", []> {
  let rs1 = 0;
}
def DECOMMITr : ILogR<511, DstNone, (outs GRPTR:$rd0), (ins GR256:$rs0, GR256:$rs1, cc:$cc),
                      "log.decommit$cc\t$rs0, $rs1, $rd0", []> {
  let rs1 = 0;
}
}

def : Pat<(int_eravm_precompile GR256:$rs0, GR256:$rs1),
          (PCOMPr GR256:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMlog_decommit GR256:$rs0, GR256:$rs1),
          (DECOMMITr GR256:$rs0, GR256:$rs1, 0)>;

let Defs = [R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, Flags],
    Uses = [SP], isCall = 1 in {
   def CALL : Pseudo<(outs), (ins GR256:$in1, imm16:$callee),
                   "CALL\t$in1, $callee", [(EraVMcall GR256:$in1, (EraVMGAStack tglobaladdr:$callee))]>;
   def INVOKE : Pseudo<(outs), (ins GR256:$in1, imm16:$callee, jmptarget:$unwind),
                   "INVOKE\t$in1, $callee, $unwind", [(EraVMinvoke GR256:$in1, (EraVMGAStack tglobaladdr:$callee), bb:$unwind)]>;
   def NEAR_CALL : ICall<9, CFNormal, (ins GR256:$in1, imm16:$callee, jmptarget:$unwind),
                   "near_call\t$in1, $callee, $unwind", []>;

  def FAR_CALL       : ICall<8, CFNormal, (ins jmptarget:$unwind),
                             "far_call\tr1, r2, $unwind",
                             [(EraVMfarcall bb:$unwind)]>;
  def STATIC_CALL    : ICall<10, CFNormal, (ins jmptarget:$unwind),
                             "far_call.static\tr1, r2, $unwind",
                             [(EraVMstaticcall bb:$unwind)]>;
  def DELEGATE_CALL  : ICall<11, CFDelegate, (ins jmptarget:$unwind),
                             "far_call.delegate\tr1, r2, $unwind",
                             [(EraVMdelegatecall bb:$unwind)]>;
  def MIMIC_CALL     : ICall<12, CFMimic, (ins jmptarget:$unwind),
                             "far_call.mimic\tr1, r2, $unwind",
                             [(EraVMmimiccall bb:$unwind)]>;
}

def : Pat<(store_stack (add GR256:$rs0, neg_imm16:$imm), stackaddr:$dst0),
          (SUBxrs_p (negate_imm imm:$imm), GR256:$rs0, stackaddr:$dst0)>;
def : Pat<(sub GR256:$rs0, neg_imm16:$imm), (ADDirr_p (negate_imm imm:$imm), GR256:$rs0)>;
def : Pat<(add GR256:$rs0, neg_imm16:$imm), (SUBxrr_p (negate_imm imm:$imm), GR256:$rs0)>;

//===----------------------------------------------------------------------===//
// Non-instruction patterns
//===----------------------------------------------------------------------===//

// GlobalAddress, ExternalSymbol
def : Pat<(i256 (EraVMGAStack tglobaladdr:$dst)), (ADDsrr_p tglobaladdr:$dst, 0, 0, R0)>;
def : Pat<(i256 (EraVMGACode tglobaladdr:$dst)), (ADDcrr_p tglobaladdr:$dst, 0, R0)>;
def : Pat<(i256 (EraVMGACode texternalsym:$dst)), (ADDcrr_p texternalsym:$dst, 0, R0)>;
def : Pat<(i256 (EraVMGACode tblockaddress:$dst)), (ADDcrr_p tblockaddress:$dst, 0, R0)>;

// Exceptions
def : Pat<(EraVMTrap), (PANIC R0)>;

def : Pat<(EraVMselectcc GRPTR:$rs0, GRPTR:$rs1, imm:$cc),
          (FATPTR_SELrrr GRPTR:$rs0, GRPTR:$rs1, imm:$cc)>;
