//===-- EraVMMCInstLower.cpp - Convert EraVM MachineInstr to an MCInst ----===//
//
// This file contains code to lower EraVM MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "EraVMMCInstLower.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

#include "EraVM.h"
#include "EraVMMCExpr.h"

using namespace llvm;

MCSymbol *
EraVMMCInstLower::GetGlobalAddressSymbol(const MachineOperand &MO) const {
  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  return Printer.getSymbol(MO.getGlobal());
}

MCSymbol *
EraVMMCInstLower::GetExternalSymbolSymbol(const MachineOperand &MO) const {
  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  return Printer.GetExternalSymbolSymbol(MO.getSymbolName());
}

MCSymbol *EraVMMCInstLower::GetJumpTableSymbol(const MachineOperand &MO) const {
  const DataLayout &DL = Printer.getDataLayout();
  SmallString<256> Name;
  raw_svector_ostream(Name)
      << DL.getPrivateGlobalPrefix() << "JTI" << Printer.getFunctionNumber()
      << '_' << MO.getIndex();

  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  // Create a symbol for the name.
  return Ctx.getOrCreateSymbol(Name);
}

MCSymbol *
EraVMMCInstLower::GetConstantPoolIndexSymbol(const MachineOperand &MO) const {
  const DataLayout &DL = Printer.getDataLayout();
  SmallString<256> Name;
  raw_svector_ostream(Name)
      << DL.getPrivateGlobalPrefix() << "CPI" << Printer.getFunctionNumber()
      << '_' << MO.getIndex();

  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  // Create a symbol for the name.
  return Ctx.getOrCreateSymbol(Name);
}

MCSymbol *
EraVMMCInstLower::GetBlockAddressSymbol(const MachineOperand &MO) const {
  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  return Printer.GetBlockAddressSymbol(MO.getBlockAddress());
}

MCOperand EraVMMCInstLower::LowerSymbolOperand(const MachineOperand &MO,
                                               MCSymbol *Sym) const {
  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);
  if (MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
  return MCOperand::createExpr(Expr);
}

void EraVMMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);

    MCOperand MCOp;
    switch (MO.getType()) {
    default:
      MI->print(errs());
      llvm_unreachable("unknown operand type");
    case MachineOperand::MO_Register:
      // Ignore all implicit register operands.
      if (MO.isImplicit())
        continue;
      MCOp = MCOperand::createReg(MO.getReg());
      break;
    case MachineOperand::MO_Immediate:
      MCOp = MCOperand::createImm(MO.getImm());
      break;
    case MachineOperand::MO_CImmediate:
      if (MO.getCImm()->getValue().getMinSignedBits() <= 64) {
        MCOp = MCOperand::createImm(MO.getCImm()->getZExtValue());
      } else {
        // To avoid a memory leak, initial size of the SmallString should be
        // chosen enough for the entire string. Otherwise, its internal memory
        // will be reallocated into the generic heap but not into the Ctx
        // arena and thus never deallocated.
        auto *Str = new (Ctx) SmallString<80>();
        MO.getCImm()->getValue().toStringUnsigned(*Str);
        MCOp = MCOperand::createExpr(EraVMCImmMCExpr::create(*Str, Ctx));
      }
      break;

    case MachineOperand::MO_MachineBasicBlock:
      MCOp = MCOperand::createExpr(
          MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx));
      break;
    case MachineOperand::MO_GlobalAddress:
      MCOp = LowerSymbolOperand(MO, GetGlobalAddressSymbol(MO));
      break;
    case MachineOperand::MO_ExternalSymbol:
      MCOp = LowerSymbolOperand(MO, GetExternalSymbolSymbol(MO));
      break;
    case MachineOperand::MO_JumpTableIndex:
      MCOp = LowerSymbolOperand(MO, GetJumpTableSymbol(MO));
      break;
    case MachineOperand::MO_ConstantPoolIndex:
      MCOp = LowerSymbolOperand(MO, GetConstantPoolIndexSymbol(MO));
      break;
    case MachineOperand::MO_BlockAddress:
      MCOp = LowerSymbolOperand(MO, GetBlockAddressSymbol(MO));
      break;
    case MachineOperand::MO_MCSymbol:
      switch (MO.getTargetFlags()) {
      case EraVMII::MO_NO_FLAG:
      case EraVMII::MO_SYM_RET_ADDRESS:
        break;
      default:
        llvm_unreachable("Unknown target flag on MCSymbol");
      }
      MCOp = LowerSymbolOperand(MO, MO.getMCSymbol());
      break;
    case MachineOperand::MO_RegisterMask:
      continue;
    }

    OutMI.addOperand(MCOp);
  }
}
