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
}

void SyncVMInstPrinter::printPCRelImmOperand(const MCInst *MI, unsigned OpNo,
                                             raw_ostream &O) {
}

void SyncVMInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O, const char *Modifier) {
}

void SyncVMInstPrinter::printSrcMemOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &O,
                                           const char *Modifier) {
}

void SyncVMInstPrinter::printIndRegOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &O) {
}

void SyncVMInstPrinter::printPostIndRegOperand(const MCInst *MI, unsigned OpNo,
                                               raw_ostream &O) {
}
