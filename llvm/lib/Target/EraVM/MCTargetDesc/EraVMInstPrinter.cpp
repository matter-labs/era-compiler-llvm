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
#include "EraVMMCTargetDesc.h"
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
  unsigned Opcode = MI->getOpcode();

  // Print some default versions here and there
  if (Opcode == EraVM::NEAR_CALL_default_unwind) {
    const MCOperand &Reg = MI->getOperand(0);

    O << "\tcall";
    printCCOperand(MI, 2, O);
    O << '\t';
    if (Reg.getReg() != EraVM::R0) {
      printOperand(MI, 0, O);
      O << ",\t";
    }
    printOperand(MI, 1, O);
    O << ",\t@DEFAULT_UNWIND_DEST";

    printAnnotation(O, Annot);
    return;
  }

  if (Opcode == EraVM::NOPrrs || Opcode == EraVM::NOPsrr) {
    // TODO Refactor the internal encoding of stack operand in the backend and
    //      remove this fixup.
    MCInst NormalizedMI = *MI;
    unsigned StackOpIdx = Opcode == EraVM::NOPrrs ? 2 : 1;

    MCOperand &MarkerOp = NormalizedMI.getOperand(StackOpIdx);
    MCOperand &BaseOp = NormalizedMI.getOperand(StackOpIdx + 1);
    MCOperand &AddendOp = NormalizedMI.getOperand(StackOpIdx + 2);

    if (MarkerOp.isReg() && MarkerOp.getReg() == EraVM::R0 && BaseOp.isImm()) {
      // (stackop R0, 0, -addend) -> (stackop R0, R0, addend)
      assert(BaseOp.getImm() == 0 && "Unexpected marker immediate operand");
      BaseOp = MCOperand::createReg(EraVM::R0);
      AddendOp.setImm(-AddendOp.getImm());
    }

    if (!printAliasInstr(&NormalizedMI, Address, O))
      printInstruction(&NormalizedMI, Address, O);
    printAnnotation(O, Annot);
    return;
  }

  if (!printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
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
    assert(Op.isExpr() && "expected expression");
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
  case EraVMCC::COND_OF:
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
  case EraVMCC::COND_GTOrLT:
    O << ".gtlt";
    break;
  case EraVMCC::COND_NONE:
    break;
  }
}

void EraVMInstPrinter::printMemOperand(const MCInst *MI, unsigned OpNo,
                                       raw_ostream &O) {
  unsigned BaseReg = 0;
  const MCSymbol *Symbol = nullptr;
  int Addend = 0;

  EraVM::analyzeMCOperandsCode(*MI, OpNo, BaseReg, Symbol, Addend);
  if (BaseReg == EraVM::R0)
    BaseReg = 0;

  if (Symbol)
    O << "@" << Symbol->getName() << "[";
  else
    O << "code[";

  if (!BaseReg && !Addend)
    O << "0";

  if (BaseReg)
    O << getRegisterName(BaseReg);

  if (Addend) {
    if (Addend < 0)
      O << "-";
    else if (BaseReg)
      O << "+";
    O << std::abs(Addend);
  }

  O << "]";
}

template <bool IsInput>
void EraVMInstPrinter::printStackOperand(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &O) {
  using namespace EraVM;

  unsigned BaseReg = 0;
  MemOperandKind Kind = MemOperandKind::OperandInvalid;
  const MCSymbol *Symbol = nullptr;
  int Addend = 0;
  analyzeMCOperandsStack(*MI, OpNo, IsInput, BaseReg, Kind, Symbol, Addend);

  switch (Kind) {
  default:
    llvm_unreachable("Unexpected kind");
  case MemOperandKind::OperandStackAbsolute:
    O << "stack[";
    break;
  case MemOperandKind::OperandStackSPRelative:
    O << "stack-[";
    break;
  case MemOperandKind::OperandStackSPDecrement:
    O << "stack-=[";
    break;
  case MemOperandKind::OperandStackSPIncrement:
    O << "stack+=[";
    break;
  }

  if (!Symbol && !Addend && !BaseReg) {
    O << "0]";
    return;
  }

  bool PrintedSomething = false;

  if (Symbol) {
    O << "@" << Symbol->getName();
    PrintedSomething = true;
  }

  if (Addend == 0 && Kind != MemOperandKind::OperandStackAbsolute) {
    // Always print immediate addend in stack-[...], stack-=[...], stack+=[...].
    // TODO Remove this special case and update the tests.
    O << "0";
    PrintedSomething = true;
  }

  if (Addend) {
    if (Addend < 0)
      O << " - ";
    else if (PrintedSomething)
      O << " + ";
    O << std::abs(Addend);
    PrintedSomething = true;
  }

  if (BaseReg) {
    if (PrintedSomething)
      O << " + ";
    O << getRegisterName(BaseReg);
    PrintedSomething = true;
  }

  O << "]";
}
