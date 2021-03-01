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
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    int64_t Imm = Op.getImm() * 2 + 2;
    O << "$";
    if (Imm >= 0)
      O << '+';
    O << Imm;
  } else {
    assert(Op.isExpr() && "unknown pcrel immediate operand");
    Op.getExpr()->print(O, &MAI);
  }
}

void SyncVMInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O, const char *Modifier) {
  assert((Modifier == nullptr || Modifier[0] == 0) && "No modifiers supported");
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    O << '#' << Op.getImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    O << '#';
    Op.getExpr()->print(O, &MAI);
  }
}

void SyncVMInstPrinter::printSrcMemOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &O,
                                           const char *Modifier) {
  const MCOperand &Base = MI->getOperand(OpNo);
  const MCOperand &Disp = MI->getOperand(OpNo+1);

  // Print displacement first

  // If the global address expression is a part of displacement field with a
  // register base, we should not emit any prefix symbol here, e.g.
  //   mov.w &foo, r1
  // vs
  //   mov.w glb(r1), r2
  // Otherwise (!) syncvm-as will silently miscompile the output :(
  if (Base.getReg() == SyncVM::SR)
    O << '&';

  if (Disp.isExpr())
    Disp.getExpr()->print(O, &MAI);
  else {
    assert(Disp.isImm() && "Expected immediate in displacement field");
    O << Disp.getImm();
  }

  // Print register base field
  if ((Base.getReg() != SyncVM::SR) &&
      (Base.getReg() != SyncVM::PC))
    O << '(' << getRegisterName(Base.getReg()) << ')';
}

void SyncVMInstPrinter::printIndRegOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &O) {
  const MCOperand &Base = MI->getOperand(OpNo);
  O << "@" << getRegisterName(Base.getReg());
}

void SyncVMInstPrinter::printPostIndRegOperand(const MCInst *MI, unsigned OpNo,
                                               raw_ostream &O) {
  const MCOperand &Base = MI->getOperand(OpNo);
  O << "@" << getRegisterName(Base.getReg()) << "+";
}

void SyncVMInstPrinter::printCCOperand(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &O) {
  unsigned CC = MI->getOperand(OpNo).getImm();

  switch (CC) {
  default:
    llvm_unreachable("Unsupported CC code");
  case SyncVMCC::COND_E:
    O << "eq";
    break;
  case SyncVMCC::COND_NE:
    O << "ne";
    break;
  case SyncVMCC::COND_HS:
    O << "hs";
    break;
  case SyncVMCC::COND_LO:
    O << "lo";
    break;
  case SyncVMCC::COND_GE:
    O << "ge";
    break;
  case SyncVMCC::COND_L:
    O << 'l';
    break;
  case SyncVMCC::COND_N:
    O << 'n';
    break;
  }
}
