//===-- EraVMInstPrinter.cpp - EraVM instr printer --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an EraVM MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "EraVMInstPrinter.h"
#include "EraVM.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "EraVMGenAsmWriter.inc"

void EraVMInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &O) {
  if (!printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void EraVMInstPrinter::printPCRelImmOperand(const MCInst *MI, unsigned OpNo,
                                            raw_ostream &O) {
  llvm_unreachable("Not implemented yet!");
}

void EraVMInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    raw_ostream &O, const char *Modifier) {
  assert((Modifier == nullptr || Modifier[0] == 0) && "No modifiers supported");
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    O << Op.getImm();
  } else {
    Op.getExpr()->print(O, &MAI);
  }
}

void EraVMInstPrinter::printCCOperand(const MCInst *MI, unsigned OpNo,
                                      raw_ostream &O) {
  unsigned CC = MI->getOperand(OpNo).getImm();

  switch (CC) {
  default:
    llvm_unreachable("Unsupported CC code");
  case EraVMCC::COND_E:
    O << ".eq";
    break;
  case EraVMCC::COND_LT:
    O << ".lt";
    break;
  case EraVMCC::COND_GT:
    O << ".gt";
    break;
  case EraVMCC::COND_NE:
    O << ".ne";
    break;
  case EraVMCC::COND_GE:
    O << ".ge";
    break;
  case EraVMCC::COND_LE:
    O << ".le";
    break;
  case EraVMCC::COND_NONE:
    break;
  }
}

void EraVMInstPrinter::printContextOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &O) {
  unsigned COp = MI->getOperand(OpNo).getImm();

  switch (COp) {
  default:
    llvm_unreachable("Unsupported Context parameter");
  case EraVMCTX::SELF_ADDR:
    O << ".self_address";
    break;
  case EraVMCTX::CALLER:
    O << ".caller";
    break;
  case EraVMCTX::CODE_ADDR:
    O << ".code_address";
    break;
  case EraVMCTX::META:
    O << ".meta";
    break;
  case EraVMCTX::TX_ORIGIN:
    O << ".tx_origin";
    break;
  case EraVMCTX::ERGS_LEFT:
    O << ".ergs_left";
    break;
  case EraVMCTX::SP:
    O << ".sp";
    break;
  case EraVMCTX::COINBASE:
    O << ".coinbase";
    break;
  }
}

void EraVMInstPrinter::printFirstOperand(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &O) {
  const MCOperand &EAF = MI->getOperand(OpNo);
  assert(EAF.isImm() && "Expected immediate in init field");
  if (EAF.getImm() == 1) {
    O << ".first";
  }
}

void EraVMInstPrinter::printMemOperand(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &O) {
  const MCOperand &Base = MI->getOperand(OpNo);
  const MCOperand &Disp = MI->getOperand(OpNo + 1);

  // Print displacement first
  if (Disp.isExpr()) {
    Disp.getExpr()->print(O, &MAI);
  } else {
    assert(Disp.isImm() && "Expected immediate in displacement field");
    O << Disp.getImm() / 32; // Displacement is in 8-bit bytes. The memory cells
                             // are 256 bits wide.
  }

  // Print register base field
  if (Base.isReg())
    O << '(' << getRegisterName(Base.getReg()) << ')';
}

void EraVMInstPrinter::printStackOperand(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &O) {
  const MCOperand &Base1 = MI->getOperand(OpNo);
  const MCOperand &Base2 = MI->getOperand(OpNo + 1);
  const MCOperand &Disp = MI->getOperand(OpNo + 2);

  O << "stack";
  if (Base1.isReg())
    O << "-";
  O << "[";

  if (Base2.isReg())
    O << getRegisterName(Base2.getReg()) << " + ";

  // Print displacement first
  if (Disp.isExpr()) {
    Disp.getExpr()->print(O, &MAI);
  } else {
    assert(Disp.isImm() && "Expected immediate in displacement field");
    O << Disp.getImm() / 32; // Displacement is in 8-bit bytes. The memory cells
                             // are 256 bits wide.
  }

  O << "]";
}

void EraVMInstPrinter::printSPAdvanceOperand(const MCInst *MI, unsigned OpNo,
                                             raw_ostream &O) {
  const MCOperand &Val = MI->getOperand(OpNo);
  int Advance = Val.getImm();
  assert(Advance != 0);

  O << "stack";
  O << ((Advance < 0) ? "-" : "+");
  O << "=[" << std::abs(Advance) << "]";
}
