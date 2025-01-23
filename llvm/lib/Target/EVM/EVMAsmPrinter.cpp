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
#include "llvm/ADT/StringSet.h"
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

#define DEBUG_TYPE "asm-printer"

namespace {
class EVMAsmPrinter : public AsmPrinter {
  // For each register class we need the mapping from a virtual register
  // to its number.
  // TODO: Once stackification is implemented this should be removed.
  using VRegMap = DenseMap<unsigned, unsigned>;
  using VRegRCMap = DenseMap<const TargetRegisterClass *, VRegMap>;
  VRegRCMap VRegMapping;

  // Maps a linker symbol name to corresponding MCSymbol.
  StringSet<> WideRelocSymbolsSet;

public:
  EVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EVM Assembly "; }

  void SetupMachineFunction(MachineFunction &MF) override;

  void emitInstruction(const MachineInstr *MI) override;

  void emitFunctionEntryLabel() override;

  /// Return true if the basic block has exactly one predecessor and the control
  /// transfer mechanism between the predecessor and this block is a
  /// fall-through.
  bool isBlockOnlyReachableByFallthrough(
      const MachineBasicBlock *MBB) const override;

  void emitEndOfAsmFile(Module &) override;

private:
  void emitAssemblySymbol(const MachineInstr *MI);
  void emitWideRelocatableSymbol(const MachineInstr *MI);
};
} // end of anonymous namespace

void EVMAsmPrinter::SetupMachineFunction(MachineFunction &MF) {
  // Unbundle <push_label, jump> bundles.
  for (MachineBasicBlock &MBB : MF) {
    MachineBasicBlock::instr_iterator I = MBB.instr_begin(),
                                      E = MBB.instr_end();
    for (; I != E; ++I) {
      if (I->isBundledWithPred()) {
        assert(I->isConditionalBranch() || I->isUnconditionalBranch());
        I->unbundleFromPred();
      }
    }
  }

  AsmPrinter::SetupMachineFunction(MF);
}

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

void EVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EVMMCInstLower MCInstLowering(OutContext, *this, VRegMapping,
                                MF->getRegInfo());
  unsigned Opc = MI->getOpcode();
  if (Opc == EVM::DATASIZE_S || Opc == EVM::DATAOFFSET_S) {
    emitAssemblySymbol(MI);
    return;
  }
  if (Opc == EVM::LINKERSYMBOL_S) {
    emitWideRelocatableSymbol(MI);
    return;
  }

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

bool EVMAsmPrinter::isBlockOnlyReachableByFallthrough(
    const MachineBasicBlock *MBB) const {
  // For simplicity, always emit BB labels.
  return false;
}

void EVMAsmPrinter::emitAssemblySymbol(const MachineInstr *MI) {
  MCSymbol *LinkerSymbol = MI->getOperand(0).getMCSymbol();
  StringRef LinkerSymbolName = LinkerSymbol->getName();
  unsigned Opc = MI->getOpcode();
  assert(Opc == EVM::DATASIZE_S || Opc == EVM::DATAOFFSET_S);

  std::string NameHash = EVM::getLinkerSymbolHash(LinkerSymbolName);
  std::string SymbolNameHash = (Opc == EVM::DATASIZE_S)
                                   ? EVM::getDataSizeSymbol(NameHash)
                                   : EVM::getDataOffsetSymbol(NameHash);

  MCInst MCI;
  MCI.setOpcode(EVM::PUSH4_S);
  MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
      SymbolNameHash, MCSymbolRefExpr::VariantKind::VK_EVM_DATA, OutContext));
  MCI.addOperand(MCOp);
  EmitToStreamer(*OutStreamer, MCI);
}

// Lowers LINKERSYMBOL_S instruction as shown below:
//
//   LINKERSYMBOL_S @BaseSymbol
//     ->
//   PUSH20_S @"__linker_symbol__$KECCAK256(BaseSymbol)$__"
//
//   .section ".symbol_name__$KECCAK256(BaseName)$__","S",@progbits
//          .ascii "~ \".%%^ [];,<.>?  .sol:GreaterHelper"

void EVMAsmPrinter::emitWideRelocatableSymbol(const MachineInstr *MI) {
  constexpr unsigned LinkerSymbolSize = 20;
  MCSymbol *BaseSymbol = MI->getOperand(0).getMCSymbol();
  StringRef BaseSymbolName = BaseSymbol->getName();
  std::string BaseSymbolNameHash = EVM::getLinkerSymbolHash(BaseSymbolName);
  std::string SymbolName = EVM::getLinkerSymbolName(BaseSymbolNameHash);

  MCInst MCI;
  // This represents a library address symbol which is 20-bytes wide.
  MCI.setOpcode(EVM::PUSH20_S);
  MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
      BaseSymbolNameHash, MCSymbolRefExpr::VariantKind::VK_EVM_DATA,
      OutContext));
  MCI.addOperand(MCOp);

  if (!WideRelocSymbolsSet.contains(SymbolName)) {
    for (unsigned Idx = 0; Idx < LinkerSymbolSize / sizeof(uint32_t); ++Idx) {
      std::string SubSymName = EVM::getSymbolIndexedName(SymbolName, Idx);
      if (OutContext.lookupSymbol(SubSymName))
        report_fatal_error(Twine("MC: duplicating reference sub-symbol ") +
                           SubSymName);
    }
  }

  auto *TS = static_cast<EVMTargetStreamer *>(OutStreamer->getTargetStreamer());
  TS->emitWideRelocatableSymbol(MCI, SymbolName, LinkerSymbolSize);

  // The linker symbol and the related section already exist, so just exit.
  if (WideRelocSymbolsSet.contains(SymbolName))
    return;

  WideRelocSymbolsSet.insert(SymbolName);

  MCSection *CurrentSection = OutStreamer->getCurrentSectionOnly();

  // Emit the .symbol_name section that contains the actual symbol
  // name.
  std::string SymbolSectionName = EVM::getSymbolSectionName(SymbolName);
  MCSection *SymbolSection = OutContext.getELFSection(
      SymbolSectionName, ELF::SHT_PROGBITS, ELF::SHF_STRINGS);
  OutStreamer->switchSection(SymbolSection);
  OutStreamer->emitBytes(BaseSymbolName);
  OutStreamer->switchSection(CurrentSection);
}

void EVMAsmPrinter::emitEndOfAsmFile(Module &) { WideRelocSymbolsSet.clear(); }

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMAsmPrinter() {
  const RegisterAsmPrinter<EVMAsmPrinter> X(getTheEVMTarget());
}
