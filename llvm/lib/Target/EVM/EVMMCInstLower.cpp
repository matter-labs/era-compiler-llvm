//===----- EVMMCInstLower.cpp - Convert EVM MachineInstr to an MCInst -----===//
//
// This file contains code to lower EVM MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "EVMMCInstLower.h"
#include "MCTargetDesc/EVMMCExpr.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

#define GET_REGINFO_HEADER
#include "EVMGenRegisterInfo.inc"

MCSymbol *
EVMMCInstLower::GetGlobalAddressSymbol(const MachineOperand &MO) const {
  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  return Printer.getSymbol(MO.getGlobal());
}

MCSymbol *
EVMMCInstLower::GetExternalSymbolSymbol(const MachineOperand &MO) const {
  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  return Printer.GetExternalSymbolSymbol(MO.getSymbolName());
}

MCOperand EVMMCInstLower::LowerSymbolOperand(const MachineOperand &MO,
                                             MCSymbol *Sym) const {
  const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);

  switch (MO.getTargetFlags()) {
  default:
    llvm_unreachable("Unknown target flag on GV operand");
  case 0:
    break;
  }

  if (MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
  return MCOperand::createExpr(Expr);
}

void EVMMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) {
  OutMI.setOpcode(MI->getOpcode());

  for (unsigned I = 0, E = MI->getNumOperands(); I != E; ++I) {
    const MachineOperand &MO = MI->getOperand(I);
    MCOperand MCOp;
    switch (MO.getType()) {
    default:
      MI->print(errs());
      llvm_unreachable("Unknown operand type");
    case MachineOperand::MO_Register:
      // Ignore all implicit register operands.
      if (MO.isImplicit())
        continue;

      MCOp = MCOperand::createReg(EncodeVReg(MO.getReg()));
      break;
    case MachineOperand::MO_Immediate:
      MCOp = MCOperand::createImm(MO.getImm());
      break;
    case MachineOperand::MO_CImmediate: {
      const APInt &CImmVal = MO.getCImm()->getValue();
      // Check for the max number of significant bits - 64, otherwise
      // the assertion in getZExtValue() is failed.
      if (CImmVal.getSignificantBits() <= 64 && CImmVal.isNonNegative()) {
        MCOp = MCOperand::createImm(MO.getCImm()->getZExtValue());
      } else {
        // To avoid a memory leak, initial size of the SmallString should be
        // chosen enough for the entire string. Otherwise, its internal memory
        // will be reallocated into the generic heap but not into the Ctx
        // arena and thus never deallocated.
        auto Str = new (Ctx) SmallString<80>();
        CImmVal.toStringUnsigned(*Str);
        MCOp = MCOperand::createExpr(EVMCImmMCExpr::create(*Str, Ctx));
      }
    } break;
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
    }

    OutMI.addOperand(MCOp);
  }
}

unsigned EVMMCInstLower::EncodeVReg(unsigned Reg) {
  if (Register::isVirtualRegister(Reg)) {
    const TargetRegisterClass *RC = MRI.getRegClass(Reg);
    VRegRCMap::const_iterator I = VRegMapping.find(RC);
    assert(I != VRegMapping.end() && "Bad register class");
    const DenseMap<unsigned, unsigned> &RegMap = I->second;

    VRegMap::const_iterator VI = RegMap.find(Reg);
    assert(VI != RegMap.end() && "Bad virtual register");
    unsigned RegNum = VI->second;
    unsigned Ret = 0;
    if (RC == &EVM::GPRRegClass)
      Ret = (1 << 28);
    else
      report_fatal_error("Unexpected register class");

    // Insert the vreg number
    Ret |= (RegNum & 0x0FFFFFFF);
    return Ret;
  }

  // Some special-use registers (at least SP) are actually physical registers.
  // Encode this as the register class ID of 0 and the real register ID.
  return Reg & 0x0FFFFFFF;
}
