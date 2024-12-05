//===-------- EVMAsmPrinter.cpp - EVM LLVM assembly writer ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the EVM assembly language.
//
//===----------------------------------------------------------------------===//

#include "EVMMCInstLower.h"
#include "EVMTargetMachine.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "MCTargetDesc/EVMTargetStreamer.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

extern cl::opt<bool> EVMKeepRegisters;

#define DEBUG_TYPE "asm-printer"

namespace {
class EVMAsmPrinter : public AsmPrinter {
  // For each register class we need the mapping from a virtual register
  // to its number.
  // TODO: Once stackification is implemented this should be removed.
  using VRegMap = DenseMap<unsigned, unsigned>;
  using VRegRCMap = DenseMap<const TargetRegisterClass *, VRegMap>;
  VRegRCMap VRegMapping;

public:
  EVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EVM Assembly "; }

  void emitInstruction(const MachineInstr *MI) override;

  void emitBasicBlockStart(const MachineBasicBlock &MBB) override;

  void emitFunctionEntryLabel() override;

private:
  void emitLinkerSymbol(const MachineInstr *MI);
  void emitJumpDest();
};
} // end of anonymous namespace

void EVMAsmPrinter::emitFunctionEntryLabel() {
  AsmPrinter::emitFunctionEntryLabel();

  VRegMapping.clear();

  // Go through all virtual registers of the MF to establish the mapping
  // between the global virtual register number and the per-class virtual
  // register number (though we have only one GPR register class).
  // We use the per-class virtual register number in the asm output.
  // TODO: This is a temporary solution while EVM-backend outputs virtual
  // registers. Once stackification is implemented this should be removed.
  const MachineRegisterInfo &MRI = MF->getRegInfo();
  const unsigned NumVRs = MRI.getNumVirtRegs();
  for (unsigned I = 0; I < NumVRs; I++) {
    const Register Vr = Register::index2VirtReg(I);
    const TargetRegisterClass *RC = MRI.getRegClass(Vr);
    DenseMap<unsigned, unsigned> &VRegMap = VRegMapping[RC];
    const unsigned N = VRegMap.size();
    VRegMap.insert(std::make_pair(Vr, N + 1));
  }
}

void EVMAsmPrinter::emitBasicBlockStart(const MachineBasicBlock &MBB) {
  AsmPrinter::emitBasicBlockStart(MBB);

  // Emit JUMPDEST instruction at the beginning of the basic block, if
  // this is not a block that is only reachable by fallthrough.
  if (!EVMKeepRegisters && !AsmPrinter::isBlockOnlyReachableByFallthrough(&MBB))
    emitJumpDest();
}

void EVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EVMMCInstLower MCInstLowering(OutContext, *this, VRegMapping,
                                MF->getRegInfo());

  switch (MI->getOpcode()) {
  default:
    break;
  case EVM::PseudoCALL: {
    // Generate push instruction with the address of a function.
    MCInst Push;
    Push.setOpcode(EVM::PUSH4_S);
    assert(MI->getOperand(0).isGlobal() &&
           "The first operand of PseudoCALL should be a GlobalValue.");

    // TODO: #745: Refactor EVMMCInstLower::Lower so we could use lowerOperand
    // instead of creating a MCOperand directly.
    MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
        getSymbol(MI->getOperand(0).getGlobal()), OutContext));
    Push.addOperand(MCOp);
    EmitToStreamer(*OutStreamer, Push);

    // Jump to a function.
    MCInst Jump;
    Jump.setOpcode(EVM::JUMP_S);
    EmitToStreamer(*OutStreamer, Jump);

    // In case a function has a return label, emit it, and also
    // emit a JUMPDEST instruction.
    if (MI->getNumExplicitOperands() > 1) {
      // We need to emit ret label after JUMP instruction, so we couldn't
      // use setPostInstrSymbol since label would be created after
      // JUMPDEST instruction. To overcome this, we added MCSymbol operand
      // and we are emitting label manually here.
      assert(MI->getOperand(1).isMCSymbol() &&
             "The second operand of PseudoCALL should be a MCSymbol.");
      OutStreamer->emitLabel(MI->getOperand(1).getMCSymbol());
      emitJumpDest();
    }
    return;
  }
  case EVM::PseudoRET: {
    // TODO: #746: Use PseudoInstExpansion and do this expansion in tblgen.
    MCInst Jump;
    Jump.setOpcode(EVM::JUMP_S);
    EmitToStreamer(*OutStreamer, Jump);
    return;
  }
  case EVM::PseudoJUMP:
  case EVM::PseudoJUMPI: {
    MCInst Push;
    Push.setOpcode(EVM::PUSH4_S);

    // TODO: #745: Refactor EVMMCInstLower::Lower so we could use lowerOperand
    // instead of creating a MCOperand directly.
    MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
        MI->getOperand(0).getMBB()->getSymbol(), OutContext));
    Push.addOperand(MCOp);
    EmitToStreamer(*OutStreamer, Push);

    MCInst Jump;
    Jump.setOpcode(MI->getOpcode() == EVM::PseudoJUMP ? EVM::JUMP_S
                                                      : EVM::JUMPI_S);
    EmitToStreamer(*OutStreamer, Jump);
    return;
  }
  case EVM::DATASIZE_S:
  case EVM::DATAOFFSET_S:
    emitLinkerSymbol(MI);
    return;
  }

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

void EVMAsmPrinter::emitLinkerSymbol(const MachineInstr *MI) {
  MCSymbol *LinkerSymbol = MI->getOperand(0).getMCSymbol();
  StringRef LinkerSymbolName = LinkerSymbol->getName();
  unsigned Opc = MI->getOpcode();
  assert(Opc == EVM::DATASIZE_S || Opc == EVM::DATAOFFSET_S);

  std::string SymbolNameHash = EVM::getLinkerSymbolHash(LinkerSymbolName);
  std::string DataSymbolNameHash =
      (Opc == EVM::DATASIZE_S) ? EVM::getDataSizeSymbol(SymbolNameHash)
                               : EVM::getDataOffsetSymbol(SymbolNameHash);

  MCInst MCI;
  MCI.setOpcode(EVM::PUSH4_S);
  MCSymbolRefExpr::VariantKind Kind = MCSymbolRefExpr::VariantKind::VK_EVM_DATA;
  MCOperand MCOp = MCOperand::createExpr(
      MCSymbolRefExpr::create(DataSymbolNameHash, Kind, OutContext));
  MCI.addOperand(MCOp);
  EmitToStreamer(*OutStreamer, MCI);
}

void EVMAsmPrinter::emitJumpDest() {
  MCInst JumpDest;
  JumpDest.setOpcode(EVM::JUMPDEST_S);
  EmitToStreamer(*OutStreamer, JumpDest);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMAsmPrinter() {
  const RegisterAsmPrinter<EVMAsmPrinter> X(getTheEVMTarget());
}
