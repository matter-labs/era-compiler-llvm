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
def SDT_EraVMAddToSP      : SDTypeProfile<0,  1, [SDTCisVT<0, i256>]>;
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

def SDT_EraVMArith  : SDTypeProfile<1, 2, [SDTCisVT<0, i256>, SDTCisVT<1, i256>, SDTCisVT<2, i256>]>;

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
def EraVMadd_to_sp : SDNode<"EraVMISD::ADD_TO_SP", SDT_EraVMAddToSP,
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
def EraVMptr_sub      : SDNode<"EraVMISD::PTR_SUB", SDT_EraVMPtrOp, []>;
def EraVMptr_pack     : SDNode<"EraVMISD::PTR_PACK", SDT_EraVMPtrOp, []>;
def EraVMptr_shrink   : SDNode<"EraVMISD::PTR_SHRINK", SDT_EraVMPtrOp, []>;
def EraVMlog_decommit : SDNode<"EraVMISD::LOG_DECOMMIT", SDT_EraVMLogDecommit, [SDNPHasChain]>;

def EraVMTrap : SDNode<"EraVMISD::TRAP", SDTNone,
    [SDNPHasChain, SDNPOptInGlue]>;

// Those instructions will set flag register and need SDNPOutGlue attribute.
// The followed flag user will use the outputted GLUE to prevent the DAG scheduler
// from scheduling another flag-setting instruction betweem them and their users.
def EraVMAdd_v : SDNode<"EraVMISD::ADD_V", SDT_EraVMArith, [SDNPHasChain, SDNPOutGlue]>;
def EraVMSub_v : SDNode<"EraVMISD::SUB_V", SDT_EraVMArith, [SDNPHasChain, SDNPOutGlue]>;
def EraVMMul_v : SDNode<"EraVMISD::MUL_V", SDT_EraVMArith, [SDNPHasChain, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// EraVM Operand Definitions.
//===----------------------------------------------------------------------===//

def UImm16Operand : AsmOperandClass {
  let Name = "UImm16";
  let ParserMethod = "tryParseUImm16Operand";
  let PredicateMethod = "isImm";
  let RenderMethod = "addImmOperands";
}

def imm16 : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isIntN(16);
}]> {
  let ParserMatchClass = UImm16Operand;
}

def neg_imm16 : Operand<i256>, IntImmLeaf<i256, [{
  return Imm.isNegative() && Imm.abs().isIntN(16);
}]> {}

def large_imm : Operand<i256>, IntImmLeaf<i256, [{
  return !Imm.abs().isIntN(16);
}]> {}

// This operand class represents those jump targets described
// as imm_in operand in the spec.
def JumpTargetOperand : AsmOperandClass {
  let Name = "JumpTarget";
  let ParserMethod = "tryParseJumpTargetOperand";
  let PredicateMethod = "isImm";
  let RenderMethod = "addImmOperands";
}

def jmptarget : Operand<OtherVT> {
  let ParserMatchClass = JumpTargetOperand;
  let EncoderMethod = "getJumpTargetValue";
}

// Address operands

def CodeOperand : AsmOperandClass {
  let Name = "CodeReference";
  let ParserMethod = "tryParseCodeOperand";
}

def memop : Operand<i256> {
  let PrintMethod = "printMemOperand";
  let EncoderMethod = "getMemOpValue";
  let ParserMatchClass = CodeOperand;
  let DecoderMethod = "DecodeCodeOperand";
  let MIOperandInfo = (ops GR256, i16imm);
}

let ParserMethod = "tryParseStackOperand",
    RenderMethod = "addStackReferenceOperands" in {
def StackInOperand : AsmOperandClass {
  let Name = "StackInReference";
  let PredicateMethod = "isStackReference<true>";
}
def StackOutOperand : AsmOperandClass {
  let Name = "StackOutReference";
  let PredicateMethod = "isStackReference<false>";
}
}

def stackin : Operand<i256> {
  let PrintMethod = "printStackOperand<true>";
  let EncoderMethod = "getStackOpValue<true>";
  let ParserMatchClass = StackInOperand;
  let DecoderMethod = "DecodeStackOperand";
  let MIOperandInfo = (ops GRStackRefMarker, GR256, i16imm);
}

def stackout : Operand<i256> {
  let PrintMethod = "printStackOperand<false>";
  let EncoderMethod = "getStackOpValue<false>";
  let ParserMatchClass = StackOutOperand;
  let DecoderMethod = "DecodeStackOperand";
  let MIOperandInfo = (ops GRStackRefMarker, GR256, i16imm);
}

//===----------------------------------------------------------------------===//
// Custom DAG Selection Operations.
//===----------------------------------------------------------------------===//

def negate_imm : SDNodeXForm<imm, [{
  APInt pos = N->getAPIntValue();
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

class AddressSpace<string name, int id> {
  string Name = name;
  int Id = id;
}

def Address_stack         : AddressSpace<"AS_STACK",     0>;
def Address_heap          : AddressSpace<"AS_HEAP",      1>;
def Address_heapaux       : AddressSpace<"AS_HEAP_AUX",  2>;
def Address_generic       : AddressSpace<"AS_GENERIC",   3>;
def Address_code          : AddressSpace<"AS_CODE",      4>;
def Address_storage       : AddressSpace<"AS_STORAGE",   5>;
def Address_transient     : AddressSpace<"AS_TRANSIENT", 6>;
def Address_static        : AddressSpace<"AS_STATIC",    7>;
def Address_max_id        : AddressSpace<"MAX_ADDRESS",  Address_static.Id>;

def AddressSpaces : GenericEnum {
  let FilterClass = "AddressSpace";
  let NameField = "Name";
  let ValueField = "Id";
}

foreach as_name = [ "stack", "heap", "heapaux", "generic", "code", "storage", "transient", "static" ] in {
  defvar as = !cast<AddressSpace>("Address_"#as_name);

  let AddressSpaces = [as.Id] in {
  def load_#as_name : PatFrag<(ops node:$ptr), (unindexedload node:$ptr)> {
    let IsLoad = 1;
  }
  def store_#as_name : PatFrag<(ops node:$val, node:$ptr),
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
let Defs = [SP], Uses = [SP] in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
                              [(EraVMcallseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : Pseudo<(outs), (ins i256imm:$amt1, i256imm:$amt2),
                              [(EraVMcallseq_end timm:$amt1, timm:$amt2)]>;
}

let Defs = [Flags], Uses = [SP] in {
def ADDframe  : Pseudo<(outs GR256:$dst), (ins i256imm:$base, i256imm:$offset), []>;
def FRAMEirrr : Pseudo<(outs GR256:$dst), (ins i256imm:$imm, GR256:$src, GR256:$sp), []>;
}

let isSelect = 1, Uses = [Flags] in {
  def SELrrr : Pseudo<(outs GR256:$dst), (ins GR256:$src, GR256:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, GR256:$src2, imm:$cc))]> {
                            let hasPostISelHook = 1;
                          }

  def FATPTR_SELrrr : Pseudo<(outs GRPTR:$dst), (ins GRPTR:$src, GRPTR:$src2, i256imm:$cc),
                             [(set GRPTR:$dst,
                               (EraVMselectcc GRPTR:$src, GRPTR:$src2, imm:$cc))]>;

  def SELirr : Pseudo<(outs GR256:$dst), (ins imm16:$src, GR256:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, GR256:$src2, imm:$cc))]> {
                            let Constraints = "$dst = $src2";
                          }
let mayLoad = 1 in {
  def SELcrr : Pseudo<(outs GR256:$dst), (ins memop:$src, GR256:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), GR256:$src2, imm:$cc))]>;
  def SELsrr : Pseudo<(outs GR256:$dst), (ins stackin:$src, GR256:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), GR256:$src2, imm:$cc))]> {
                            let Constraints = "$dst = $src2";
                          }
}
  def SELrir : Pseudo<(outs GR256:$dst), (ins GR256:$src, imm16:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, imm16:$src2, imm:$cc))]> {
                            let hasPostISelHook = 1;
                          }
  def SELiir : Pseudo<(outs GR256:$dst), (ins imm16:$src, imm16:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, imm16:$src2, imm:$cc))]>;
let mayLoad = 1 in {
  def SELcir : Pseudo<(outs GR256:$dst), (ins memop:$src, imm16:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), imm16:$src2, imm:$cc))]>;
  def SELsir : Pseudo<(outs GR256:$dst), (ins stackin:$src, imm16:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), imm16:$src2, imm:$cc))]>;
  def SELrcr : Pseudo<(outs GR256:$dst), (ins GR256:$src, memop:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, (load_code memaddr:$src2), imm:$cc))]> {
                            let hasPostISelHook = 1;
                          }
  def SELicr : Pseudo<(outs GR256:$dst), (ins imm16:$src, memop:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, (load_code memaddr:$src2), imm:$cc))]>;
  def SELccr : Pseudo<(outs GR256:$dst), (ins memop:$src, memop:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), (load_code memaddr:$src2), imm:$cc))]>;
  def SELscr : Pseudo<(outs GR256:$dst), (ins stackin:$src, memop:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_stack stackaddr:$src), (load_code memaddr:$src2), imm:$cc))]>;
  def SELrsr : Pseudo<(outs GR256:$dst), (ins GR256:$src, stackin:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc GR256:$src, (load_stack stackaddr:$src2), imm:$cc))]> {
                            let hasPostISelHook = 1;
                          }
  def SELisr : Pseudo<(outs GR256:$dst), (ins imm16:$src, stackin:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc imm16:$src, (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELcsr : Pseudo<(outs GR256:$dst), (ins memop:$src, stackin:$src2, i256imm:$cc),
                       [(set GR256:$dst,
                          (EraVMselectcc (load_code memaddr:$src), (load_stack stackaddr:$src2), imm:$cc))]>;
  def SELssr : Pseudo<(outs GR256:$dst), (ins stackin:$src, stackin:$src2, i256imm:$cc),
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

// Patterns for selectcc instructions with large immediates that will be loaded from constant pool.
// Following two patterns are to put zero immediate to R0 register.
def : Pat<(EraVMselectcc 0, large_imm:$src2, imm:$cc),
          (SELrcr R0, (constant_pool imm:$src2), 0, imm:$cc)>;
def : Pat<(EraVMselectcc large_imm:$src1, 0, imm:$cc),
          (SELcrr (constant_pool imm:$src1), 0, R0, imm:$cc)>;
def : Pat<(EraVMselectcc large_imm:$src1, GR256:$src2, imm:$cc),
          (SELcrr (constant_pool imm:$src1), 0, GR256:$src2, imm:$cc)>;
def : Pat<(EraVMselectcc GR256:$src1, large_imm:$src2, imm:$cc),
          (SELrcr GR256:$src1, (constant_pool imm:$src2), 0, imm:$cc)>;
def : Pat<(EraVMselectcc large_imm:$src1, imm16:$src2, imm:$cc),
          (SELcir (constant_pool imm:$src1), 0, imm16:$src2, imm:$cc)>;
def : Pat<(EraVMselectcc imm16:$src1, large_imm:$src2, imm:$cc),
          (SELicr imm16:$src1, (constant_pool imm:$src2), 0, imm:$cc)>;
def : Pat<(EraVMselectcc (load_code memaddr:$src1), large_imm:$src2, imm:$cc),
          (SELccr memaddr:$src1, (constant_pool imm:$src2), 0, imm:$cc)>;
def : Pat<(EraVMselectcc large_imm:$src1, (load_code memaddr:$src2), imm:$cc),
          (SELccr (constant_pool imm:$src1), 0, memaddr:$src2, imm:$cc)>;
def : Pat<(EraVMselectcc large_imm:$src1, large_imm:$src2, imm:$cc),
          (SELccr (constant_pool imm:$src1), 0, (constant_pool imm:$src2), 0, imm:$cc)>;
def : Pat<(EraVMselectcc large_imm:$src1, (load_stack stackaddr:$src2), imm:$cc),
          (SELcsr (constant_pool imm:$src1), 0, stackaddr:$src2, imm:$cc)>;
def : Pat<(EraVMselectcc (load_stack stackaddr:$src1), large_imm:$src2, imm:$cc),
          (SELscr stackaddr:$src1, (constant_pool imm:$src2), 0, imm:$cc)>;

// TODO: CPR-1356 stack and code forms
def : Pat<(int_eravm_ifeq GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, COND_E.Encoding)>;
def : Pat<(int_eravm_iflt GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, COND_LT.Encoding)>;
def : Pat<(int_eravm_ifgt GR256:$src0, GR256:$src1), (SELrrr GR256:$src0, GR256:$src1, COND_GT.Encoding)>;

def : Pat<(int_eravm_ifeq imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, COND_E.Encoding)>;
def : Pat<(int_eravm_iflt imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, COND_LT.Encoding)>;
def : Pat<(int_eravm_ifgt imm16:$src0, imm16:$src1), (SELiir imm16:$src0, imm16:$src1, COND_GT.Encoding)>;

let hasSideEffects = 1, Defs = [SP], Uses = [SP], mayStore = 0 in {
def NOPrrr : Irr_r<OpNoOp, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                   "$rs0, $rs1, $rd0", []>;
def NOPirr : Iir_r<OpNoOp, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0", []>;
def NOPcrr : Imr_r<OpNoOp, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0", []>;
def NOPsrr : Isr_r<OpNoOp, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins stackin:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0", []>;
def NOPrrs : Irr_s<OpNoOp, NoSwap, PreserveFlags,
                   (outs), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                   "$rs0, $rs1, $dst0", []>;
def NOPirs : Iir_s<OpNoOp, NoSwap, PreserveFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0", []>;
def NOPcrs : Imr_s<OpNoOp, NoSwap, PreserveFlags,
                   (outs), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0", []>;
def NOPsrs : Isr_s<OpNoOp, NoSwap, PreserveFlags,
                   (outs), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0", []>;
}

def : Pat<(EraVMadd_to_sp GR256:$reg), (NOPrrs R0, R0, R0, $reg, 0, 0)>;

def : InstAlias<"nop${cc}\t${dst0}", (NOPrrs R0, R0, stackout:$dst0, pred:$cc)>;
def : InstAlias<"nop${cc}\t${src0}", (NOPsrr R0, stackin:$src0, R0, pred:$cc)>;
def : InstAlias<"nop${cc}",          (NOPrrr R0, R0, R0, pred:$cc)>;

// Note that while technically commutative arithmetic instructions are not swappable,
// it is not a problem as there will be no corresponding instruction defined with SwapOperand = 1
// and therefore such instruction will be not emitted in `Swappable`-related tables.
multiclass Arith<EraVMOpcode opcode, SDPatternOperator node, bit commutes> {
  let BaseOpcode = opcode.Name, isCommutable = commutes in {

  def rrr_s : Irr_r<opcode, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                   "$rs0, $rs1, $rd0",
                   [(set GR256:$rd0, (node GR256:$rs0, GR256:$rs1))]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def rrr_v : Irr_r<opcode, NoSwap, SetFlags,
                   (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                   "$rs0, $rs1, $rd0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def crr_s : Imr_r<opcode, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GR256:$rd0, (node (load_code memaddr:$src0), GR256:$rs1))]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def crr_v : Imr_r<opcode, NoSwap, SetFlags,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srr_s : Isr_r<opcode, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins stackin:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GR256:$rd0, (node (load_stack stackaddr:$src0), GR256:$rs1))]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srr_v : Isr_r<opcode, NoSwap, SetFlags,
                   (outs GR256:$rd0), (ins stackin:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def rrs_s : Irr_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                   "$rs0, $rs1, $dst0",
                   [(store_stack (node GR256:$rs0, GR256:$rs1), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def rrs_v : Irr_s<opcode, NoSwap, SetFlags,
                   (outs), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                   "$rs0, $rs1, $dst0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def crs_s : Imr_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node (load_code memaddr:$src0), GR256:$rs1), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def crs_v : Imr_s<opcode, NoSwap, SetFlags,
                   (outs), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def srs_s : Isr_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node (load_stack stackaddr:$src0), GR256:$rs1), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srs_v : Isr_s<opcode, NoSwap, SetFlags,
                   (outs), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  }
}

multiclass ArithICommutable<EraVMOpcode opcode, SDPatternOperator node> : Arith<opcode, node, 1> {
  let BaseOpcode = opcode.Name, isCommutable = 1 in {
  def irr_s : Iir_r<opcode, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0", [(set GR256:$rd0, (node GR256:$rs1, imm16:$imm))]>,
                   FlagSetting, AddrModeRel, RetAddrModeRel;
  def irr_v : Iir_r<opcode, NoSwap, SetFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0", []>,
                   FlagSetting, AddrModeRel, RetAddrModeRel;
  def irs_s : Iir_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0",
                   [(store_stack (node GR256:$rs1, imm16:$imm), stackaddr:$dst0)]>,
                   FlagSetting, AddrModeRel, RetAddrModeRel;
  def irs_v : Iir_s<opcode, NoSwap, SetFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0", []>,
                   FlagSetting, AddrModeRel, RetAddrModeRel;
  } // end isCommutable = 1

  // commutative: irr_s -> crr_s
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # crr_s) (constant_pool imm:$imm), 0, GR256:$rs1, 0)>;
  // commutative: irs_s -> crs_s
  def : Pat<(store_stack (node GR256:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # crs_s) (constant_pool imm:$imm), 0, GR256:$rs1, stackaddr:$dst0, 0)>;
}

multiclass ArithINonCommutable<EraVMOpcode opcode, SDPatternOperator node> : Arith<opcode, node, 0> {
  let BaseOpcode = opcode.Name in {
  def irr_s : Iir_r<opcode, NoSwap, PreserveFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0",
                   [(set GR256:$rd0, (node imm16:$imm, GR256:$rs1))]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrr_s : Iir_r<opcode, Swap, PreserveFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0",
                   [(set GR256:$rd0, (node GR256:$rs1, imm16:$imm))]>,
                   FlagSetting, Swappable, RetAddrModeRel;

  // The four *_alias instructions below are defined to make asm parser handle
  // "mnemonic.s rs0, rs1, rd0" instructions for the sake of uniformity, that
  // are otherwise redundant. *Rel base classes are dropped, as these
  // instructions are not intended to participate in InstrMappings.
  def rrr_s_alias : Irr_r<opcode, Swap, PreserveFlags,
                          (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                          "$rs0, $rs1, $rd0", []>;
  def rrr_v_alias : Irr_r<opcode, Swap, SetFlags,
                          (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                          "$rs0, $rs1, $rd0", []>;
  def rrs_s_alias : Irr_s<opcode, Swap, PreserveFlags,
                          (outs), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                          "$rs0, $rs1, $dst0", []>;
  def rrs_v_alias : Irr_s<opcode, Swap, SetFlags,
                          (outs), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                          "$rs0, $rs1, $dst0", []>;

  def irr_v : Iir_r<opcode, NoSwap, SetFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrr_v : Iir_r<opcode, Swap, SetFlags,
                   (outs GR256:$rd0), (ins imm16:$imm, GR256:$rs1),
                   "$imm, $rs1, $rd0", []>,
                   FlagSetting, Swappable, RetAddrModeRel;

  def yrr_s : Imr_r<opcode, Swap, PreserveFlags,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GR256:$rd0, (node GR256:$rs1, (load_code memaddr:$src0)))]>,
                   FlagSetting, Swappable, RetAddrModeRel;
  def yrr_v : Imr_r<opcode, Swap, SetFlags,
                   (outs GR256:$rd0), (ins memop:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0", []>,
                   FlagSetting, Swappable, RetAddrModeRel;
  def zrr_s : Isr_r<opcode, Swap, PreserveFlags,
                   (outs GR256:$rd0), (ins stackin:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GR256:$rd0, (node GR256:$rs1, (load_stack stackaddr:$src0)))]>,
                   FlagSetting, Swappable, RetAddrModeRel;
  def zrr_v : Isr_r<opcode, Swap, SetFlags,
                   (outs GR256:$rd0), (ins stackin:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0", []>,
                   FlagSetting, Swappable, RetAddrModeRel;

  def irs_s : Iir_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0",
                   [(store_stack (node imm16:$imm, GR256:$rs1), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def xrs_s : Iir_s<opcode, Swap, PreserveFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0",
                   [(store_stack (node GR256:$rs1, imm16:$imm), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, RetAddrModeRel;

  def irs_v : Iir_s<opcode, NoSwap, SetFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0", []>,
                   FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrs_v : Iir_s<opcode, Swap, SetFlags,
                   (outs), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0", []>,
                   FlagSetting, Swappable, RetAddrModeRel;

  def yrs_s : Imr_s<opcode, Swap, PreserveFlags,
                   (outs), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node GR256:$rs1, (load_code memaddr:$src0)), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, RetAddrModeRel;

  def yrs_v : Imr_s<opcode, Swap, SetFlags,
                   (outs), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0", []>,
                   FlagSetting, Swappable, RetAddrModeRel;

  def zrs_s : Isr_s<opcode, Swap, PreserveFlags,
                   (outs), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node GR256:$rs1, (load_stack stackaddr:$src0)), stackaddr:$dst0)]>,
                   FlagSetting, Swappable, RetAddrModeRel;
  def zrs_v : Isr_s<opcode, Swap, SetFlags,
                   (outs), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0", []>,
                   FlagSetting, Swappable, RetAddrModeRel;
  }

  // non-commutative: irr_s -> crr_s
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # crr_s) (constant_pool imm:$imm), 0, GR256:$rs1, 0)>;

  // xrr_s -> yrr_s
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # yrr_s) (constant_pool imm:$imm), 0, GR256:$rs1, 0)>;

  // non-commutative: irs_s -> crs_s
  def : Pat<(store_stack (node GR256:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # crs_s) (constant_pool imm:$imm), 0, GR256:$rs1, stackaddr:$dst0, 0)>;

  // xrs_s -> yrs_s
  def : Pat<(store_stack (node GR256:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # yrs_s) (constant_pool imm:$imm), 0, GR256:$rs1, stackaddr:$dst0, 0)>;
}

multiclass PtrInstr<EraVMOpcode opcode, SDPatternOperator node> {
  let BaseOpcode = opcode.Name in {

  def rrr_s : Irr_r<opcode, NoSwap, PreserveFlags,
                   (outs GRPTR:$rd0), (ins GRPTR:$rs0, GR256:$rs1),
                   "$rs0, $rs1, $rd0",
                   [(set GRPTR:$rd0, (node GRPTR:$rs0, GR256:$rs1))]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  // The two *_alias instructions below are defined to make asm parser handle
  // "ptr.<op>.s rs0, rs1, rd0" instructions for the sake of uniformity, that
  // are otherwise redundant. *Rel base classes are dropped, as these
  // instructions are not intended to participate in InstrMappings.
  def rrr_s_alias : Irr_r<opcode, Swap, PreserveFlags,
                          (outs GRPTR:$rd0), (ins GRPTR:$rs0, GR256:$rs1),
                          "$rs0, $rs1, $rd0", []>;
  def rrs_s_alias : Irr_s<opcode, Swap, PreserveFlags,
                          (outs), (ins GRPTR:$rs0, GR256:$rs1, stackout:$dst0),
                          "$rs0, $rs1, $dst0", []>;

  def srr_s : Isr_r<opcode, NoSwap, PreserveFlags,
                   (outs GRPTR:$rd0), (ins stackin:$src0, GR256:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GRPTR:$rd0, (node (load_stack stackaddr:$src0), GR256:$rs1))]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def rrs_s : Irr_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins GRPTR:$rs0, GR256:$rs1, stackout:$dst0),
                   "$rs0, $rs1, $dst0",
                   [(store_stack (node GRPTR:$rs0, GR256:$rs1), stackaddr:$dst0)]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def srs_s : Isr_s<opcode, NoSwap, PreserveFlags,
                   (outs), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node (load_stack stackaddr:$src0), GR256:$rs1), stackaddr:$dst0)]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def xrr_s : Iir_r<opcode, Swap, PreserveFlags,
                   (outs GRPTR:$rd0), (ins imm16:$imm, GRPTR:$rs1),
                   "$imm, $rs1, $rd0",
                   [(set GRPTR:$rd0, (node GRPTR:$rs1, imm16:$imm))]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def yrr_s : Imr_r<opcode, Swap, PreserveFlags,
                   (outs GRPTR:$rd0), (ins memop:$src0, GRPTR:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GRPTR:$rd0, (node GRPTR:$rs1, (load_code memaddr:$src0)))]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def zrr_s : Isr_r<opcode, Swap, PreserveFlags,
                   (outs GRPTR:$rd0), (ins stackin:$src0, GRPTR:$rs1),
                   "$src0, $rs1, $rd0",
                   [(set GRPTR:$rd0, (node GRPTR:$rs1, (load_stack stackaddr:$src0)))]>,
                   Swappable, RetAddrModeRel;

  def xrs_s : Iir_s<opcode, Swap, PreserveFlags,
                   (outs), (ins imm16:$imm, GRPTR:$rs1, stackout:$dst0),
                   "$imm, $rs1, $dst0",
                   [(store_stack (node GRPTR:$rs1, imm16:$imm), stackaddr:$dst0)]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def yrs_s : Imr_s<opcode, Swap, PreserveFlags,
                   (outs), (ins memop:$src0, GRPTR:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node GRPTR:$rs1, (load_code memaddr:$src0)), stackaddr:$dst0)]>,
                   Swappable, AddrModeRel, RetAddrModeRel;

  def zrs_s : Isr_s<opcode, Swap, PreserveFlags,
                   (outs), (ins stackin:$src0, GRPTR:$rs1, stackout:$dst0),
                   "$src0, $rs1, $dst0",
                   [(store_stack (node GRPTR:$rs1, (load_stack stackaddr:$src0)), stackaddr:$dst0)]>,
                   Swappable, RetAddrModeRel;
  }

  // xrr_s -> yrr_s
  def : Pat<(node GRPTR:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # yrr_s) (constant_pool imm:$imm), 0, GRPTR:$rs1, 0)>;

  // xrs_s -> yrs_s
  def : Pat<(store_stack (node GRPTR:$rs1, large_imm:$imm), stackaddr:$dst0),
            (!cast<Instruction>(NAME # yrs_s) (constant_pool imm:$imm), 0, GRPTR:$rs1, stackaddr:$dst0, 0)>;
}

// This is a bridge to convert GRPTR type to GR256 type. It will get eliminated in a post RA pass.
def PTR_TO_INT : Pseudo<(outs GR256:$rd0), (ins GRPTR:$rs0),
                        [(set GR256:$rd0, (EraVMptr_to_int GRPTR:$rs0))]>;

multiclass Arith2<EraVMOpcode opcode, SDPatternOperator node, bit commutes> {
  let BaseOpcode = opcode.Name, isCommutable = commutes in {
  def rrrr_s : Irr_rr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1),
                     "$rs0, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs0, GR256:$rs1))]>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def rrrr_v : Irr_rr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1),
                     "$rs0, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def crrr_s : Imr_rr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node (load_code memaddr:$src0), GR256:$rs1))]>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def crrr_v : Imr_rr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srrr_s : Isr_rr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackin:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node (load_stack stackaddr:$src0), GR256:$rs1))]>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srrr_v : Isr_rr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackin:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;

  def rrsr_s : Irr_sr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                     "$rs0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def rrsr_v : Irr_sr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                     "$rs0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def crsr_s : Imr_sr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def crsr_v : Imr_sr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srsr_s : Isr_sr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd1), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def srsr_v : Isr_sr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd1), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  }
}

multiclass Arith2ICommutable<EraVMOpcode opcode, SDPatternOperator node>
         : Arith2<opcode, node, 1> {
  let BaseOpcode = opcode.Name, isCommutable = 1 in {
  def irrr_s : Iir_rr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                     "$imm, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, imm16:$imm))]>,
                     FlagSetting, AddrModeRel, RetAddrModeRel;
  def irrr_v : Iir_rr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                     "$imm, $rs1, $rd0, $rd1", []>,
                     FlagSetting, AddrModeRel, RetAddrModeRel;
  def irsr_s : Iir_sr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                     "$imm, $rs1, $dst0, $rd1", []>,
                     FlagSetting, AddrModeRel, RetAddrModeRel;
  def irsr_v : Iir_sr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                     "$imm, $rs1, $dst0, $rd1", []>,
                     FlagSetting, AddrModeRel, RetAddrModeRel;
  }

  // commutative: rrrr_p -> crrr_p
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # crrr_s) (constant_pool imm:$imm), 0, GR256:$rs1, 0)>;
}

multiclass Arith2INonCommutable<EraVMOpcode opcode, SDPatternOperator node>
         : Arith2<opcode, node, 0> {
  let BaseOpcode = opcode.Name in {
  def irrr_s : Iir_rr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                     "$imm, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node imm16:$imm, GR256:$rs1))]>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrrr_s : Iir_rr<opcode, Swap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                     "$imm, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, imm16:$imm))]>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def irrr_v : Iir_rr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                     "$imm, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrrr_v : Iir_rr<opcode, Swap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins imm16:$imm, GR256:$rs1),
                     "$imm, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;

  // The four *_alias instructions below are defined to make asm parser handle
  // "mnemonic.s rs0, rs1, rd0, rd1" instructions for the sake of uniformity, that
  // are otherwise redundant. *Rel base classes are dropped, as these
  // instructions are not intended to participate in InstrMappings.
  def rrrr_s_alias : Irr_rr<opcode, Swap, PreserveFlags,
                           (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1),
                           "$rs0, $rs1, $rd0, $rd1", []>;
  def rrrr_v_alias : Irr_rr<opcode, Swap, SetFlags,
                           (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0, GR256:$rs1),
                           "$rs0, $rs1, $rd0, $rd1", []>;
  def rrsr_s_alias : Irr_sr<opcode, Swap, PreserveFlags,
                           (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                           "$rs0, $rs1, $dst0, $rd1", []>;
  def rrsr_v_alias : Irr_sr<opcode, Swap, SetFlags,
                           (outs GR256:$rd1), (ins GR256:$rs0, GR256:$rs1, stackout:$dst0),
                           "$rs0, $rs1, $dst0, $rd1", []>;

  def yrrr_s : Imr_rr<opcode, Swap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, (load_code memaddr:$src0)))]>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def yrrr_v : Imr_rr<opcode, Swap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins memop:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def zrrr_s : Isr_rr<opcode, Swap, PreserveFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackin:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1",
                     [(set GR256:$rd0, GR256:$rd1, (node GR256:$rs1, (load_stack stackaddr:$src0)))]>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def zrrr_v : Isr_rr<opcode, Swap, SetFlags,
                     (outs GR256:$rd0, GR256:$rd1), (ins stackin:$src0, GR256:$rs1),
                     "$src0, $rs1, $rd0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;

  def irsr_s : Iir_sr<opcode, NoSwap, PreserveFlags,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                     "$imm, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrsr_s : Iir_sr<opcode, Swap, PreserveFlags,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                     "$imm, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def irsr_v : Iir_sr<opcode, NoSwap, SetFlags,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                     "$imm, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, AddrModeRel, RetAddrModeRel;
  def xrsr_v : Iir_sr<opcode, Swap, SetFlags,
                     (outs GR256:$rd1), (ins imm16:$imm, GR256:$rs1, stackout:$dst0),
                     "$imm, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def yrsr_s : Imr_sr<opcode, Swap, PreserveFlags,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def yrsr_v : Imr_sr<opcode, Swap, SetFlags,
                     (outs GR256:$rd1), (ins memop:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def zrsr_s : Isr_sr<opcode, Swap, PreserveFlags,
                     (outs GR256:$rd1), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  def zrsr_v : Isr_sr<opcode, Swap, SetFlags,
                     (outs GR256:$rd1), (ins stackin:$src0, GR256:$rs1, stackout:$dst0),
                     "$src0, $rs1, $dst0, $rd1", []>,
                     FlagSetting, Swappable, RetAddrModeRel;
  }

  // non-commutative:
  def : Pat<(node large_imm:$imm, GR256:$rs1),
            (!cast<Instruction>(NAME # crrr_s) (constant_pool imm:$imm), 0, GR256:$rs1, 0)>;
  def : Pat<(node GR256:$rs1, large_imm:$imm),
            (!cast<Instruction>(NAME # yrrr_s) (constant_pool imm:$imm), 0, GR256:$rs1, 0)>;
}

defm ADD  : ArithICommutable<OpAdd, add>;
defm SUB  : ArithINonCommutable<OpSub, sub>;
defm AND  : ArithICommutable<OpAnd, and>;
defm OR   : ArithICommutable<OpOr, or>;
defm XOR  : ArithICommutable<OpXor, xor>;
defm SHL  : ArithINonCommutable<OpShl, shl>;
defm SHR  : ArithINonCommutable<OpShr, srl>;
defm ROL  : ArithINonCommutable<OpRol, rotl>;
defm ROR  : ArithINonCommutable<OpRor, rotr>;

defm MUL  : Arith2ICommutable<OpMul, umullohi>;
defm DIV  : Arith2INonCommutable<OpDiv, udivrem>;

// pointer arithmetic
defm PTR_ADD    : PtrInstr<OpPtrAdd, EraVMptr_add>;
defm PTR_SUB    : PtrInstr<OpPtrSub, EraVMptr_sub>;
defm PTR_PACK   : PtrInstr<OpPtrPack, EraVMptr_pack>;
defm PTR_SHRINK : PtrInstr<OpPtrShrink, EraVMptr_shrink>;

// MOV patterns
def : Pat<(store_stack (load_code memaddr:$src), stackaddr:$dst),  (ADDcrs_s memaddr:$src, R0, stackaddr:$dst, 0)>;
def : Pat<(store_stack (load_stack stackaddr:$src), stackaddr:$dst),  (ADDsrs_s stackaddr:$src, R0, stackaddr:$dst, 0)>;
def : Pat<(load_code memaddr:$addr),  (ADDcrr_s memaddr:$addr, R0, 0)>;
def : Pat<(load_stack stackaddr:$addr),  (ADDsrr_s stackaddr:$addr, R0, 0)>;
def : Pat<(store_stack GR256:$src, stackaddr:$dst),  (ADDrrs_s GR256:$src, R0, stackaddr:$dst, 0)>;
def : Pat<(store_stack imm16:$src, stackaddr:$dst),  (ADDirs_s imm16:$src, R0, stackaddr:$dst, 0)>;

// fat pointer move patterns
def : Pat<(fatptr (load_stack stackaddr:$addr)),  (PTR_ADDsrr_s stackaddr:$addr, R0, 0)>;
def : Pat<(store_stack GRPTR:$src, stackaddr:$dst),  (PTR_ADDrrs_s GRPTR:$src, R0, stackaddr:$dst, 0)>;

// fat pointer load/store
def : Pat<(store_stack GRPTR:$src, stackaddr:$dst), (PTR_ADDrrs_s GRPTR:$src, R0, stackaddr:$dst, 0)>;

def : Pat<(EraVMcopy_from_ptrreg GRPTR:$src), (PTR_ADDrrr_s GRPTR:$src, R0, 0)>;

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

//===----------------------------------------------------------------------===//
// Memory operations
//===----------------------------------------------------------------------===//

let mayLoad = 1 in {
multiclass HeapLoad<EraVMOpcode opcode, SDPatternOperator node> {
  def r : IUMAr_r<opcode, (outs GR256:$rd0), (ins GR256:$rs0),
                 "$rs0, $rd0",
                 [(set GR256:$rd0, (node GR256:$rs0))]>;
  def i : IUMAi_r<opcode, (outs GR256:$rd0), (ins imm16:$addr),
                 "$addr, $rd0",
                 [(set GR256:$rd0, (node imm16:$addr))]>;
}

multiclass HeapLoadInc<EraVMOpcode opcode> {
  def r : IUMAr_rr<opcode, (outs GR256:$rd0, GR256:$rd1), (ins GR256:$rs0),
                 "$rs0, $rd0, $rd1", []>;
  def i : IUMAi_rr<opcode, (outs GR256:$rd0, GR256:$rd1), (ins imm16:$addr),
                  "$addr, $rd0, $rd1", []>;
}
}

defm LDMh   : HeapLoad<OpLoadHeap, load_heap>;
defm LDMah  : HeapLoad<OpLoadAuxHeap, load_heapaux>;
defm LDMIh  : HeapLoadInc<OpLoadHeapInc>;
defm LDMIah : HeapLoadInc<OpLoadAuxHeapInc>;

defm LDMst  : HeapLoad<OpStaticRead, load_static>;
defm LDMIst : HeapLoadInc<OpStaticReadInc>;

let mayLoad = 1 in {
def LDP      : IUMAr_r<OpLoadPtr, (outs GR256:$rd0), (ins GRPTR:$rs0),
                     "$rs0, $rd0", []>;
def LDPI   : IUMAr_rr<OpLoadPtrInc, (outs GR256:$rd0, GRPTR:$rd1), (ins GRPTR:$rs0),
                      "$rs0, $rd0, $rd1", []>;
}

let mayStore = 1 in {
multiclass HeapStore<EraVMOpcode opcode, SDPatternOperator node> {
  def r : IUMArr_<opcode, (outs), (ins GR256:$rs0, GR256:$rs1),
                  "$rs0, $rs1",
                  [(node GR256:$rs1, GR256:$rs0)]>;
  def i : IUMAir_<opcode, (outs), (ins imm16:$addr, GR256:$rs1),
                  "$addr, $rs1",
                  [(node GR256:$rs1, imm16:$addr)]>;
}

multiclass HeapStoreInc<EraVMOpcode opcode> {
  def r : IUMArr_r<opcode, (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                  "$rs0, $rs1, $rd0", []>;
  def i : IUMAir_r<opcode, (outs GR256:$rd0), (ins imm16:$addr, GR256:$rs1),
                  "$addr, $rs1, $rd0", []>;
}
}

defm STMh   : HeapStore<OpStoreHeap, store_heap>;
defm STMah  : HeapStore<OpStoreAuxHeap, store_heapaux>;
defm STMIh  : HeapStoreInc<OpStoreHeapInc>;
defm STMIah : HeapStoreInc<OpStoreAuxHeapInc>;

defm STMst  : HeapStore<OpStaticWrite, store_static>;
defm STMIst : HeapStoreInc<OpStaticWriteInc>;

//===----------------------------------------------------------------------===//
// Control flow instructions
//===----------------------------------------------------------------------===//
// Conditional jump

// Mix-ins: has return address operand or not.
class JumpWithoutRetAddr {
  bit isTerminator = 1;
  bit isBranch = 1;
}
class JumpWithRetAddr {
  bit isTerminator = 1; // May be relaxed in the future.
  bit isBranch = 1;

  bits<4> ret_addr;
  bits<4> Dst0 = ret_addr;
}
// Mix-ins: specifying jump targets via different kinds of operands.
class JumpViaReg {
  bits<4> dest;
  bits<4> Src0 = dest;
}
class JumpViaLabel {
  bits<16> dest;
  bits<16> Imm0 = dest;
}
class JumpViaConst {
  bits<20> dest;
  bits<4> Src0 = dest{3-0};
  bits<16> Imm0 = dest{19-4};
}
class JumpViaStack : JumpViaConst;

// Below, two instructions are defined for each input operand kind: one with
// a return address output operand and one without. This is because "jump" with
// a return address operand can be used as a cheaper replacement for "call",
// so it may be worth making it non-terminator in the future.

let Uses = [Flags] in // TODO Do not set implicit use unconditionally.
def JCl  : IJump<OpJump, SrcImm, (outs), (ins jmptarget:$dest), "$dest",
                 [(EraVMbrcc bb:$dest, imm:$cc)]>,
           JumpWithoutRetAddr, JumpViaLabel;
def JClr : IJump<OpJump, SrcImm, (outs GR256:$ret_addr), (ins jmptarget:$dest), "$dest, $ret_addr", []>,
           JumpWithRetAddr, JumpViaLabel;

def JCr  : IJump<OpJump, SrcReg, (outs), (ins GR256:$dest), "$dest", []>,
           JumpWithoutRetAddr, JumpViaReg;
def JCrr : IJump<OpJump, SrcReg, (outs GR256:$ret_addr), (ins GR256:$dest), "$dest, $ret_addr", []>,
           JumpWithRetAddr, JumpViaReg;

def JCc  : IJump<OpJump, SrcCodeAddr, (outs), (ins memop:$dest), "$dest", []>,
           JumpWithoutRetAddr, JumpViaConst;
def JCcr : IJump<OpJump, SrcCodeAddr, (outs GR256:$ret_addr), (ins memop:$dest), "$dest, $ret_addr", []>,
           JumpWithRetAddr, JumpViaConst;

def JCs  : IJump<OpJump, SrcStackAbsolute, (outs), (ins stackin:$dest), "$dest", []>,
           JumpWithoutRetAddr, JumpViaStack;
def JCsr : IJump<OpJump, SrcStackAbsolute, (outs GR256:$ret_addr), (ins stackin:$dest), "$dest, $ret_addr", []>,
           JumpWithRetAddr, JumpViaStack;

// Unconditional jump
let isTerminator = 1, isBranch = 1, isBarrier = 1 in {
def J: Pseudo<(outs), (ins jmptarget:$dest), [(EraVMbrcc bb:$dest, 0)]>,
       PseudoInstExpansion<(JCl jmptarget:$dest, 0)>;
def J_s: Pseudo<(outs), (ins stackin:$dest), []>;
}
def : Pat<(br bb:$dst), (J bb:$dst)>;

let isBranch=1, isIndirectBranch=1, isTerminator=1, isBarrier = 1, mayLoad = 1 in
def J_jt: Pseudo<(outs), (ins memop:$dest, pred:$cc), []>,
          PseudoInstExpansion<(JCc memop:$dest, pred:$cc)>;

// Dynamic jumps
let isCall = 1 in
def JCALL: Pseudo<(outs), (ins jmptarget:$dest), []>,
           PseudoInstExpansion<(JCl jmptarget:$dest, 0)>;

def RETr : IRet<OpRet, (ins GR256:$rs0), "$rs0", []>;
def RETrl : IRetToLabel<OpRetToLabel, (ins GR256:$rs0, jmptarget:$dest), "$rs0, $dest", []>;

def REVERTr : IRet<OpRevert, (ins GR256:$rs0), "$rs0", []>;
def REVERTrl : IRetToLabel<OpRevertToLabel, (ins GR256:$rs0, jmptarget:$dest), "$rs0, $dest", []>;

let rs0 = 0 in {
def PANIC  : IRet<OpPanic, (ins), "", []>;
def PANICl : IRetToLabel<OpPanicToLabel, (ins jmptarget:$dest), "$dest", []>;
}

let isReturn = 1, isTerminator = 1, isBarrier = 1  in {
def RET  : Pseudo<(outs), (ins pred:$cc), [(EraVMret)]>,
           PseudoInstExpansion<(RETr R1, pred:$cc)>;
}

// FIXME Check if isTrap is handled properly.
let isTerminator = 1, isBarrier = 1, isTrap = 1 in
def THROW : Pseudo<(outs), (ins GR256:$rs0, pred:$cc), [(EraVMthrow GR256:$rs0)]>,
            PseudoInstExpansion<(REVERTr GR256:$rs0, pred:$cc)>;

def : InstAlias<"ret${cc}", (RETr R1, pred:$cc)>;
def : InstAlias<"revert${cc}", (REVERTr R1, pred:$cc)>;

// TODO Make these aliases preferred for printing after switching to the new syntax.
let EmitPriority = 0 in {
def : InstAlias<"ret.ok.to_label${cc} $dest", (RETrl R1, jmptarget:$dest, pred:$cc)>;
def : InstAlias<"ret.revert.to_label${cc} $dest", (REVERTrl R1, jmptarget:$dest, pred:$cc)>;
}

let isTerminator = 1, isBarrier = 1, isReturn = 1 in {
def DEFAULT_FAR_RETURN : Pseudo<(outs), (ins GR256:$rs0, pred:$cc), [(EraVMreturn GR256:$rs0)]>;
def DEFAULT_FAR_REVERT : Pseudo<(outs), (ins GR256:$rs0, pred:$cc), [(EraVMrevert GR256:$rs0)]>;
}

//===----------------------------------------------------------------------===//
// Constants materialization
//===----------------------------------------------------------------------===//

let isAsCheapAsAMove = 1, mayLoad = 1, isReMaterializable = 1,
    hasSideEffects = 0 in {
// FIXME: expand pseudo
def LOADCONST : Pseudo<(outs GR256:$val), (ins i256imm:$addr),
                       [(set GR256:$val, (load tconstpool:$addr))]>;
}

let isReMaterializable = 1, hasSideEffects = 0 in {
def MOVEIMM : Pseudo<(outs GR256:$out), (ins i256imm:$val),
                      [(set GR256:$out, imm:$val)]>;
}

//===----------------------------------------------------------------------===//
// Intrinsics lowering
//===----------------------------------------------------------------------===//

let isReMaterializable = 1, hasSideEffects = 0 in {
def CTXThis : IContext_r<OpContextThis, (outs GR256:$rd0), (ins),
                         "$rd0",
                         [(set GR256:$rd0, (int_eravm_this))]>;
def CTXCaller : IContext_r<OpContextCaller, (outs GR256:$rd0), (ins),
                           "$rd0",
                           [(set GR256:$rd0, (int_eravm_caller))]>;
def CTXCodeSource : IContext_r<OpContextCodeAddress, (outs GR256:$rd0), (ins),
                               "$rd0",
                               [(set GR256:$rd0, (int_eravm_codesource))]>;
def CTXGetU128 : IContext_r<OpContextGetContextU128, (outs GR256:$rd0), (ins),
                            "$rd0",
                            [(set GR256:$rd0, (int_eravm_getu128))]>;
}

def CTXMeta : IContext_r<OpContextMeta, (outs GR256:$rd0), (ins),
                         "$rd0",
                         [(set GR256:$rd0, (int_eravm_meta))]>;

let hasSideEffects = 1 in {
def CTXSetU128 : IContextr_<OpContextSetContextU128, (outs), (ins GR256:$rs0),
                            "$rs0",
                            [(int_eravm_setu128 GR256:$rs0)]>;
def CTXSetPubDP : IContextr_<OpContextSetErgsPerPubdataByte, (outs), (ins GR256:$rs0),
                             "$rs0",
                             [(int_eravm_setpubdataprice GR256:$rs0)]>;
let hasPostISelHook = 1 in
def CTXGasLeft : IContext_r<OpContextErgsLeft, (outs GR256:$rd0), (ins),
                            "$rd0",
                            [(set GR256:$rd0, (int_eravm_gasleft))]>;
def CTXGetSp   : IContext_r<OpContextSp, (outs GR256:$rd0), (ins),
                            "$rd0",
                            [(set GR256:$rd0, (EraVMget_sp))]>;
def CTXIncTx   : IContext_<OpContextIncrementTxNumber, (outs), (ins), "", [(int_eravm_inctx)]>;
}

//===----------------------------------------------------------------------===//
// Fat Pointer
//===----------------------------------------------------------------------===//
def LDS : ILogRr_r<OpSload, (outs GR256:$rd0), (ins GR256:$rs0),
                     "$rs0, $rd0",
                     [(set GR256:$rd0, (load_storage GR256:$rs0))]>;
def LDT : ILogRr_r<OpTransientLoad, (outs GR256:$rd0), (ins GR256:$rs0),
                     "$rs0, $rd0",
                     [(set GR256:$rd0, (load_transient GR256:$rs0))]>;

let hasSideEffects = 1 in {
  def STS : ILogRrr_<OpSstore, (outs), (ins GR256:$rs0, GR256:$rs1),
                      "$rs0, $rs1",
                      [(store_storage GR256:$rs1, GR256:$rs0)]>;
  def STT : ILogRrr_<OpTransientStore, (outs), (ins GR256:$rs0, GR256:$rs1),
                      "$rs0, $rs1",
                      [(store_transient GR256:$rs1, GR256:$rs0)]>;
}

let hasSideEffects = 1 in {
def LOGL1 : ILogRrr_<OpLogToL1, (outs), (ins GR256:$rs0, GR256:$rs1),
                        "$rs0, $rs1",
                        [(int_eravm_tol1 GR256:$rs0, GR256:$rs1, 0)]>;
def LOGL1I : ILogRrr_<OpLogToL1Initial, (outs), (ins GR256:$rs0, GR256:$rs1),
                             "$rs0, $rs1",
                             [(int_eravm_tol1 GR256:$rs0, GR256:$rs1, 1)]>;

def LOG : ILogRrr_<OpLogEvent, (outs), (ins GR256:$rs0, GR256:$rs1),
                         "$rs0, $rs1",
                         [(int_eravm_event GR256:$rs0, GR256:$rs1, 0)]>;

def LOGI : ILogRrr_<OpLogEventInitial, (outs), (ins GR256:$rs0, GR256:$rs1),
                              "$rs0, $rs1",
                              [(int_eravm_event GR256:$rs0, GR256:$rs1, 1)]>;

def CALLP : ILogRrr_r<OpLogPrecompile, (outs GR256:$rd0), (ins GR256:$rs0, GR256:$rs1),
                       "$rs0, $rs1, $rd0",
                       [(set GR256:$rd0, (int_eravm_precompile GR256:$rs0, GR256:$rs1))]>;
}

def DCMT : ILogRrr_r<OpDecommit, (outs GRPTR:$rd0), (ins GR256:$rs0, GR256:$rs1),
                          "$rs0, $rs1, $rd0",
                          [(set GRPTR:$rd0, (EraVMlog_decommit GR256:$rs0, GR256:$rs1))]>;

multiclass FarCallInst<EraVMOpcode opcode> {
  def rrl : IFarCall<opcode, 0, 0, (ins GR256:$rs0, GR256:$rs1, jmptarget:$unwind),
                    "$rs0, $rs1, $unwind", []>;
  def _STATICrrl : IFarCall<opcode, 0, 1, (ins GR256:$rs0, GR256:$rs1, jmptarget:$unwind),
                           "$rs0, $rs1, $unwind", []>;
  def _SHARDrrl  : IFarCall<opcode, 1, 0, (ins GR256:$rs0, GR256:$rs1, jmptarget:$unwind),
                           "$rs0, $rs1, $unwind", []>;
  def _STATIC_SHARDrrl : IFarCall<opcode, 1, 1, (ins GR256:$rs0, GR256:$rs1, jmptarget:$unwind),
                                 "$rs0, $rs1, $unwind", []>;
}

let Defs = [R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, Flags],
    Uses = [SP], isCall = 1 in {
  def CALL : Pseudo<(outs), (ins GR256:$in1, imm16:$callee),
                     [(EraVMcall GR256:$in1, (EraVMGAStack tglobaladdr:$callee))]>;
  def INVOKE : Pseudo<(outs), (ins GR256:$in1, imm16:$callee, jmptarget:$unwind),
                      [(EraVMinvoke GR256:$in1, (EraVMGAStack tglobaladdr:$callee), bb:$unwind)]>;
  def NEAR_CALL : ICall<OpCall, (ins GR256:$in1, jmptarget:$callee, jmptarget:$unwind),
                        "$in1, $callee, $unwind", []>;

  // Unfortunately, we cannot encode @DEFAULT_UNWIND_DEST here, so we have to alias to pseudo
  def NEAR_CALL_default_unwind : AsmParserPseudo<(outs), (ins GR256:$in1, jmptarget:$callee),
                                                 "near_call", "${in1} ${callee}">;

  defm FAR_CALL      : FarCallInst<OpFarcall>;
  defm DELEGATE_CALL : FarCallInst<OpDelegate>;
  defm MIMIC_CALL    : FarCallInst<OpMimic>;
}

// Static and shard modifiers are technically part of far call's mnemonic,
// so using `MnemonicAlias`es to accept them in both orders.
// Note: using foreach, otherwise typos would be very hard to spot.
foreach Op = [OpFarcall, OpDelegate, OpMimic] in {
  def : MnemonicAlias<!strconcat(Op.Name, ".sh.st"),
                      !strconcat(Op.Name, ".st.sh")>;
}

def : InstAlias<"near_call${cc} ${callee}",
                (NEAR_CALL_default_unwind R0, jmptarget:$callee, pred:$cc)>;

def : Pat<(EraVMfarcall bb:$unwind), (FAR_CALLrrl R1, R2, bb:$unwind)>;
def : Pat<(EraVMstaticcall bb:$unwind), (FAR_CALL_STATICrrl R1, R2, bb:$unwind)>;
def : Pat<(EraVMdelegatecall bb:$unwind), (DELEGATE_CALLrrl R1, R2, bb:$unwind)>;
def : Pat<(EraVMmimiccall bb:$unwind), (MIMIC_CALLrrl R1, R2, bb:$unwind)>;

def : Pat<(store_stack (add GR256:$rs0, neg_imm16:$imm), stackaddr:$dst0),
          (SUBxrs_s (negate_imm imm:$imm), GR256:$rs0, stackaddr:$dst0, 0)>;
def : Pat<(sub GR256:$rs0, neg_imm16:$imm), (ADDirr_s (negate_imm imm:$imm), GR256:$rs0, 0)>;
def : Pat<(add GR256:$rs0, neg_imm16:$imm), (SUBxrr_s (negate_imm imm:$imm), GR256:$rs0, 0)>;

//===----------------------------------------------------------------------===//
// Non-instruction patterns
//===----------------------------------------------------------------------===//

// GlobalAddress, ExternalSymbol
def : Pat<(i256 (EraVMGAStack tglobaladdr:$dst)), (ADDsrr_s tglobaladdr:$dst, 0, 0, R0, 0)>;
def : Pat<(i256 (EraVMGACode tglobaladdr:$dst)), (ADDcrr_s tglobaladdr:$dst, 0, R0, 0)>;
def : Pat<(i256 (EraVMGACode texternalsym:$dst)), (ADDcrr_s texternalsym:$dst, 0, R0, 0)>;
def : Pat<(i256 (EraVMGACode tblockaddress:$dst)), (ADDcrr_s tblockaddress:$dst, 0, R0, 0)>;

// Exceptions
def : Pat<(EraVMTrap), (PANIC 0)>;

def : Pat<(EraVMselectcc GRPTR:$rs0, GRPTR:$rs1, imm:$cc),
          (FATPTR_SELrrr GRPTR:$rs0, GRPTR:$rs1, imm:$cc)>;

// ============================ Overflow ADDs =====================================
// to register
def : Pat<(EraVMAdd_v GR256:$rs0, GR256:$rs1),
          (ADDrrr_v GR256:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMAdd_v GR256:$rs0, imm16:$rs1),
          (ADDirr_v imm16:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMAdd_v GR256:$rs0, large_imm:$rs1),
          (ADDcrr_v (constant_pool imm:$rs1), 0, GR256:$rs0, 0)>;
def : Pat<(EraVMAdd_v GR256:$rs0, (load_code memaddr:$rs1)),
          (ADDcrr_v memop:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMAdd_v (load_code memaddr:$rs0), GR256:$rs1),
          (ADDcrr_v memop:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMAdd_v GR256:$rs0, (load_stack stackaddr:$rs1)),
          (ADDsrr_v stackin:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMAdd_v (load_stack stackaddr:$rs0), GR256:$rs1),
          (ADDsrr_v stackin:$rs0, GR256:$rs1, 0)>;

// to stack
def : Pat<(store_stack (EraVMAdd_v GR256:$rs0, GR256:$rs1), stackaddr:$stack),
          (ADDrrs_v GR256:$rs0, GR256:$rs1, stackaddr:$stack, 0)>;
def : Pat<(store_stack (EraVMAdd_v GR256:$rs0, imm16:$rs1), stackaddr:$stack),
          (ADDirs_v imm16:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMAdd_v GR256:$rs0, large_imm:$rs1), stackaddr:$stack),
          (ADDcrs_v (constant_pool imm:$rs1), 0, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMAdd_v GR256:$rs0, (load_code memaddr:$rs1)), stackaddr:$stack),
          (ADDcrs_v memop:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMAdd_v (load_code memaddr:$rs0), GR256:$rs1), stackaddr:$stack),
          (ADDcrs_v memop:$rs0, GR256:$rs1, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMAdd_v (load_stack stackaddr:$rs0), GR256:$rs1), stackaddr:$stack),
          (ADDsrs_v stackin:$rs0, GR256:$rs1, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMAdd_v GR256:$rs0, (load_stack stackaddr:$rs1)), stackaddr:$stack),
          (ADDsrs_v stackin:$rs1, GR256:$rs0, stackin:$stack, 0)>;

// ============================ Overflow SUBs =====================================
// to register
def : Pat<(EraVMSub_v GR256:$rs0, GR256:$rs1), (SUBrrr_v GR256:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMSub_v imm16:$rs0, GR256:$rs1), (SUBirr_v imm16:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMSub_v large_imm:$rs0, GR256:$rs1),
          (SUBcrr_v (constant_pool imm:$rs0), 0, GR256:$rs1, 0)>;
def : Pat<(EraVMSub_v (load_code memaddr:$rs0), GR256:$rs1),
          (SUBcrr_v memop:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMSub_v (load_stack stackaddr:$rs0), GR256:$rs1),
          (SUBsrr_v stackin:$rs0, GR256:$rs1, 0)>;

def : Pat<(EraVMSub_v GR256:$rs0, imm16:$rs1),
          (SUBxrr_v imm16:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMSub_v GR256:$rs0, large_imm:$rs1),
          (SUByrr_v (constant_pool imm:$rs1), 0, GR256:$rs0, 0)>;
def : Pat<(EraVMSub_v GR256:$rs0, (load_code memaddr:$rs1)),
          (SUByrr_v memop:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMSub_v GR256:$rs0, (load_stack stackaddr:$rs1)),
          (SUBzrr_v stackin:$rs1, GR256:$rs0, 0)>;

// to stack
def : Pat<(store_stack (EraVMSub_v GR256:$rs0, GR256:$rs1), stackaddr:$stack),
          (SUBrrs_v GR256:$rs0, GR256:$rs1, stackin:$stack, 0)>;

def : Pat<(store_stack (EraVMSub_v imm16:$rs0, GR256:$rs1), stackaddr:$stack),
          (SUBirs_v imm16:$rs0, GR256:$rs1, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMSub_v large_imm:$rs0, GR256:$rs1), stackaddr:$stack),
          (SUBcrs_v (constant_pool imm:$rs0), 0, GR256:$rs1, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMSub_v (load_code memaddr:$rs0), GR256:$rs1), stackaddr:$stack),
          (SUBcrs_v memop:$rs0, GR256:$rs1, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMSub_v (load_stack stackaddr:$rs0), GR256:$rs1), stackaddr:$stack),
          (SUBsrs_v stackin:$rs0, GR256:$rs1, stackin:$stack, 0)>;

def : Pat<(store_stack (EraVMSub_v GR256:$rs0, imm16:$rs1), stackaddr:$stack),
          (SUBxrs_v imm16:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMSub_v GR256:$rs0, large_imm:$rs1), stackaddr:$stack),
          (SUByrs_v (constant_pool imm:$rs1), 0, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMSub_v GR256:$rs0, (load_code memaddr:$rs1)), stackaddr:$stack),
          (SUByrs_v memop:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMSub_v GR256:$rs0, (load_stack stackaddr:$rs1)), stackaddr:$stack),
          (SUBzrs_v stackin:$rs1, GR256:$rs0, stackin:$stack, 0)>;

// ============================ Overflow MULs =====================================
// to register
def : Pat<(EraVMMul_v GR256:$rs0, GR256:$rs1),
          (MULrrrr_v GR256:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMMul_v GR256:$rs0, imm16:$rs1),
          (MULirrr_v imm16:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMMul_v GR256:$rs0, large_imm:$rs1),
          (MULcrrr_v (constant_pool imm:$rs1), 0, GR256:$rs0, 0)>;
def : Pat<(EraVMMul_v (load_code memaddr:$rs0), GR256:$rs1),
          (MULcrrr_v memaddr:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMMul_v GR256:$rs0, (load_code memaddr:$rs1)),
          (MULcrrr_v memaddr:$rs1, GR256:$rs0, 0)>;
def : Pat<(EraVMMul_v (load_stack stackaddr:$rs0), GR256:$rs1),
          (MULsrrr_v stackaddr:$rs0, GR256:$rs1, 0)>;
def : Pat<(EraVMMul_v GR256:$rs0, (load_stack stackaddr:$rs1)),
          (MULsrrr_v stackaddr:$rs1, GR256:$rs0, 0)>;

// to stack
def : Pat<(store_stack (EraVMMul_v GR256:$rs0, GR256:$rs1), stackaddr:$stack),
          (MULrrsr_v GR256:$rs0, GR256:$rs1, stackaddr:$stack, 0)>;
def : Pat<(store_stack (EraVMMul_v GR256:$rs0, imm16:$rs1), stackaddr:$stack),
          (MULirsr_v imm16:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMMul_v GR256:$rs0, large_imm:$rs1), stackaddr:$stack),
          (MULcrsr_v (constant_pool imm:$rs1), 0, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMMul_v GR256:$rs0, (load_code memaddr:$rs1)), stackaddr:$stack),
          (MULcrsr_v memop:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMMul_v (load_code memaddr:$rs0) ,GR256:$rs1), stackaddr:$stack),
          (MULcrsr_v memop:$rs0, GR256:$rs1, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMMul_v GR256:$rs0, (load_stack stackaddr:$rs1)), stackaddr:$stack),
          (MULsrsr_v stackin:$rs1, GR256:$rs0, stackin:$stack, 0)>;
def : Pat<(store_stack (EraVMMul_v (load_stack stackaddr:$rs0) ,GR256:$rs1), stackaddr:$stack),
          (MULsrsr_v stackin:$rs0, GR256:$rs1, stackin:$stack, 0)>;
