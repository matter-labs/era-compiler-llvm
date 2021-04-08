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
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    if (isa<MCSymbolRefExpr>(Op.getExpr()))
      O << "@";
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
    O << "e";
    break;
  case SyncVMCC::COND_NE:
    O << "ne";
    break;
  case SyncVMCC::COND_LT:
    O << "lt";
    break;
  case SyncVMCC::COND_LE:
    O << "le";
    break;
  case SyncVMCC::COND_GT:
    O << "gt";
    break;
  case SyncVMCC::COND_GE:
    O << "ge";
    break;
  case SyncVMCC::COND_NONE:
    break;
  }
}

void SyncVMInstPrinter::printMemOperand(const MCInst *MI, unsigned OpNo,
                                        raw_ostream &O) {
  const MCOperand &Base = MI->getOperand(OpNo);
  const MCOperand &Disp = MI->getOperand(OpNo + 1);

  if (!Base.isReg())
    O << "@";

  // Print displacement first
  if (Disp.isExpr()) {
    Disp.getExpr()->print(O, &MAI);
  } else {
    assert(Disp.isImm() && "Expected immediate in displacement field");
    O << Disp.getImm();
  }

  // Print register base field
  if (Base.isReg())
    O << '(' << getRegisterName(Base.getReg()) << ')';
}
