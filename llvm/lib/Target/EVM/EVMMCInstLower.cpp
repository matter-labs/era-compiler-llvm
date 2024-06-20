//===----- EVMMCInstLower.cpp - Convert EVM MachineInstr to an MCInst -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains code to lower EVM MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "EVMMCInstLower.h"
#include "EVMInstrInfo.h"
#include "MCTargetDesc/EVMMCExpr.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"

#include <limits>

using namespace llvm;

extern cl::opt<bool> EVMKeepRegisters;

static bool IsLinkerOpcode(unsigned Opc) {
  return Opc == EVM::DATASIZE || Opc == EVM::DATAOFFSET;
}

static void removeRegisterOperands(const MachineInstr *MI, MCInst &OutMI) {
  // Remove all uses of stackified registers to bring the instruction format
  // into its final stack form used throughout MC, and transition opcodes to
  // their _S variant.
  if (MI->isDebugInstr() || MI->isLabel() || MI->isInlineAsm())
    return;

  // Transform 'register' instruction to 'stack' one.
  unsigned RegOpcode = OutMI.getOpcode();
  if (RegOpcode == EVM::PUSH_LABEL || IsLinkerOpcode(RegOpcode)) {
    // Replace PUSH_LABEL with PUSH4_S opcode.
    OutMI.setOpcode(EVM::PUSH4_S);
  } else {
    unsigned StackOpcode = EVM::getStackOpcode(RegOpcode);
    OutMI.setOpcode(StackOpcode);
  }

  // Remove register operands.
  for (auto I = OutMI.getNumOperands(); I; --I) {
    auto &MO = OutMI.getOperand(I - 1);
    if (MO.isReg()) {
      OutMI.erase(&MO);
    }
  }
}

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

MCOperand
EVMMCInstLower::LowerLinkerInstrOperand(const MachineOperand &MO) const {
  assert(MO.getCImm()->getValue().getSignificantBits() <= 32);
  MCSymbol *TextSizeSym = Ctx.getOrCreateSymbol("__text_size__");
  const MCExpr *TextSizeExp = MCSymbolRefExpr::create(TextSizeSym, Ctx);
  const MCExpr *OffExp = MCBinaryExpr::createAdd(
      TextSizeExp, MCConstantExpr::create(MO.getCImm()->getZExtValue(), Ctx),
      Ctx);
  return MCOperand::createExpr(OffExp);
}

void EVMMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) {
  OutMI.setOpcode(MI->getOpcode());
  const MCInstrDesc &Desc = MI->getDesc();
  unsigned Opc = MI->getOpcode();
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
      assert(!IsLinkerOpcode(Opc));
      MCOp = MCOperand::createImm(MO.getImm());
      break;
    case MachineOperand::MO_CImmediate: {
      const APInt &CImmVal = MO.getCImm()->getValue();
      // Check for the max number of significant bits - 64, otherwise
      // the assertion in getZExtValue() is failed.
      if (CImmVal.getSignificantBits() <= 64 && CImmVal.isNonNegative()) {
        if (IsLinkerOpcode(Opc))
          MCOp = LowerLinkerInstrOperand(MO);
        else
          MCOp = MCOperand::createImm(MO.getCImm()->getZExtValue());
      } else {
        assert(!IsLinkerOpcode(Opc));
        // To avoid a memory leak, initial size of the SmallString should be
        // chosen enough for the entire string. Otherwise, its internal memory
        // will be reallocated into the generic heap but not into the Ctx
        // arena and thus never deallocated.
        auto *Str = new (Ctx) SmallString<80>();
        CImmVal.toStringUnsigned(*Str);
        MCOp = MCOperand::createExpr(EVMCImmMCExpr::create(*Str, Ctx));
      }
    } break;
    case MachineOperand::MO_MCSymbol: {
      MCOp =
          MCOperand::createExpr(MCSymbolRefExpr::create(MO.getMCSymbol(), Ctx));
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
  if (!EVMKeepRegisters)
    removeRegisterOperands(MI, OutMI);
  else if (Desc.variadicOpsAreDefs())
    OutMI.insert(OutMI.begin(), MCOperand::createImm(MI->getNumExplicitDefs()));
}

unsigned EVMMCInstLower::EncodeVReg(unsigned Reg) {
  if (Register::isVirtualRegister(Reg)) {
    const TargetRegisterClass *RC = MRI.getRegClass(Reg);
    const VRegRCMap::const_iterator I = VRegMapping.find(RC);
    assert(I != VRegMapping.end() && "Bad register class");
    const DenseMap<unsigned, unsigned> &RegMap = I->second;

    const VRegMap::const_iterator VI = RegMap.find(Reg);
    assert(VI != RegMap.end() && "Bad virtual register");
    const unsigned RegNum = VI->second;
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
