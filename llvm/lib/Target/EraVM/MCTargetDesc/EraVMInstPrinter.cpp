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
#include "llvm/MC/MCSymbol.h"
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
    dbgs()<<*MI;
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
  case EraVMCTX::THIS:
    O << ".this";
    break;
  case EraVMCTX::CALLER:
    O << ".caller";
    break;
  case EraVMCTX::CODE_SOURCE:
    O << ".code_source";
    break;
  case EraVMCTX::META:
    O << ".meta";
    break;
  case EraVMCTX::TX_ORIGIN:
    O << ".tx_origin";
    break;
  case EraVMCTX::GAS_LEFT:
    O << ".gas_left";
    break;
  case EraVMCTX::SP:
    O << ".sp";
    break;
  case EraVMCTX::COINBASE:
    O << ".coinbase";
    break;
  case EraVMCTX::GET_U128:
    O << ".get_context_u128";
    break;
  case EraVMCTX::SET_U128:
    O << ".set_context_u128";
    break;
  case EraVMCTX::INC_CTX:
    O << ".inc_tx_num";
    break;
  case EraVMCTX::SET_PUBDATAPRICE:
    O << ".set_gas_per_pubdata";
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

  // print constant pool memory
  if (Base.isExpr()) {
    Base.getExpr()->print(O, &MAI);
    O << "[0]";
    return;
  }

  if (Base.isReg() && Disp.isImm()) {
    if (Disp.getImm() == 0)
      O << "code[" << getRegisterName(Base.getReg()) << "]";
    else
      O << "code[" << getRegisterName(Base.getReg()) << "+" << Disp.getImm()
        << "]";
    return;
  }

  // Print displacement first
  if (Disp.isExpr()) {
    const auto *expr = Disp.getExpr();
    // handle the case where symbol has an offset
    if (const auto *binExpr = dyn_cast<MCBinaryExpr>(expr)) {
      assert(binExpr->getOpcode() == MCBinaryExpr::Add &&
             "Unexpected binary expression type, check EraVMMCInstLower "
             "for reference.");
      const auto *sym = cast<MCSymbolRefExpr>(binExpr->getLHS());
      const auto *offset = cast<MCConstantExpr>(binExpr->getRHS());
      // print symbol
      O << '@' << sym->getSymbol().getName() << "[";
      // if there is a reg, print it before offset
      if (Base.isReg())
        O << getRegisterName(Base.getReg()) << "+";
      // finally, print offset
      O << offset->getValue() << "]";
    } else if (const auto *symExpr = dyn_cast<MCSymbolRefExpr>(expr)) {
      // handle the case where symbol has no imm offset but could have a reg
      // index
      if (Base.isReg())
        O << '@' << symExpr->getSymbol().getName() << "["
          << getRegisterName(Base.getReg()) << "]";
      else
        O << '@' << symExpr->getSymbol().getName() << "[0]";
    }
    return;
  }

  assert(Disp.isImm() && "Expected immediate in displacement field");
  O << Disp.getImm();
}

void EraVMInstPrinter::printStackOperand(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &O) {
  const MCOperand &Base1 = MI->getOperand(OpNo);
  const MCOperand &Base2 = MI->getOperand(OpNo + 1);
  const MCOperand &Disp = MI->getOperand(OpNo + 2);

  bool isRelative = Base1.isReg();

  O << "stack";
  if (isRelative)
    O << "-";
  O << "[";

  if (Base2.isReg()) {
    if (isRelative) {
      // stack-[disp + reg];
      assert(Disp.isImm() && Disp.getImm() < 0); // don't support expr yet
      O << std::abs(Disp.getImm()) << " - " << getRegisterName(Base2.getReg());
    } else {
      // print absolute address in format stack[disp + reg]:
      if (Disp.isImm()) {
        // skip the sign if disp is 0
        if (Disp.getImm() > 0)
          O << Disp.getImm();
        else if (Disp.getImm() < 0)
          O << " - " << std::abs(Disp.getImm());

        // if disp is 0, don't print the + sign
        if (Disp.getImm() != 0)
          O << " + ";
      } else {
        assert(Disp.isExpr());
        Disp.getExpr()->print(O, &MAI);
        O << " + ";
      }
      O << getRegisterName(Base2.getReg());
    }
  } else {
    // Print displacement first
    if (Disp.isExpr())
      Disp.getExpr()->print(O, &MAI);
    else {
      assert(Disp.isImm() && "Expected immediate in displacement field");
      O << std::abs(Disp.getImm());
    }
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
