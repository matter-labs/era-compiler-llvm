//===-- SyncVMInstPrinter.cpp - Convert SyncVM MCInst to assembly syntax --===//
//
// This class prints an SyncVM MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "SyncVMInstPrinter.h"
#include "SyncVM.h"
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
#include "SyncVMGenAsmWriter.inc"

void SyncVMInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                  StringRef Annot, const MCSubtargetInfo &STI,
                                  raw_ostream &O) {
  if (!printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void SyncVMInstPrinter::printPCRelImmOperand(const MCInst *MI, unsigned OpNo,
                                             raw_ostream &O) {
  llvm_unreachable("Not implemented yet!");
}

void SyncVMInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
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

void SyncVMInstPrinter::printCCOperand(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &O) {
  unsigned CC = MI->getOperand(OpNo).getImm();

  switch (CC) {
  default:
    llvm_unreachable("Unsupported CC code");
  case SyncVMCC::COND_E:
    O << ".eq";
    break;
  case SyncVMCC::COND_LT:
    O << ".lt";
    break;
  case SyncVMCC::COND_GT:
    O << ".gt";
    break;
  case SyncVMCC::COND_NE:
    O << ".ne";
    break;
  case SyncVMCC::COND_GE:
    O << ".ge";
    break;
  case SyncVMCC::COND_LE:
    O << ".le";
    break;
  case SyncVMCC::COND_NONE:
    break;
  }
}

void SyncVMInstPrinter::printContextOperand(const MCInst *MI, unsigned OpNo,
                                            raw_ostream &O) {
  unsigned COp = MI->getOperand(OpNo).getImm();

  switch (COp) {
  default:
    llvm_unreachable("Unsupported Context parameter");
  case SyncVMCTX::SELF_ADDR:
    O << ".self_address";
    break;
  case SyncVMCTX::CALLER:
    O << ".caller";
    break;
  case SyncVMCTX::CODE_ADDR:
    O << ".code_address";
    break;
  case SyncVMCTX::META:
    O << ".meta";
    break;
  case SyncVMCTX::TX_ORIGIN:
    O << ".tx_origin";
    break;
  case SyncVMCTX::ERGS_LEFT:
    O << ".ergs_left";
    break;
  case SyncVMCTX::SP:
    O << ".sp";
    break;
  case SyncVMCTX::COINBASE:
    O << ".coinbase";
    break;
  }
}

void SyncVMInstPrinter::printEAFOperand(const MCInst *MI, unsigned OpNo,
                                        raw_ostream &O) {
  const MCOperand &EAF = MI->getOperand(OpNo);
  assert(EAF.isImm() &&
         "Expected immediate in exteranal address storage field");
  if (EAF.getImm() == 1) {
    O << ".e";
  }
}

void SyncVMInstPrinter::printFirstOperand(const MCInst *MI, unsigned OpNo,
                                          raw_ostream &O) {
  const MCOperand &EAF = MI->getOperand(OpNo);
  assert(EAF.isImm() && "Expected immediate in init field");
  if (EAF.getImm() == 1) {
    O << ".first";
  }
}

void SyncVMInstPrinter::printMemOperand(const MCInst *MI, unsigned OpNo,
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

void SyncVMInstPrinter::printStackOperand(const MCInst *MI, unsigned OpNo,
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

void SyncVMInstPrinter::printSPAdvanceOperand(const MCInst *MI, unsigned OpNo,
                                              raw_ostream &O) {
  const MCOperand &Val = MI->getOperand(OpNo);
  int Advance = Val.getImm();
  assert(Advance != 0);

  O << "stack";
  O << ((Advance < 0) ? "-" : "+");
  O << "=[" << std::abs(Advance) << "]";
}
