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
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
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
  // MCSymbol *BeginCodeSectionSym = nullptr;
  // MCSymbol *EndCodeSectionSym = nullptr;

public:
  EVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EVM Assembly "; }

  void emitInstruction(const MachineInstr *MI) override;

  void emitFunctionEntryLabel() override;
  void emitStartOfAsmFile(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;
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

  MCSection *Sec = OutStreamer->getCurrentSectionOnly();
  if (!Sec->getBeginSymbol()) {
    MCSymbol *SectionStartSym = OutContext.createTempSymbol();
    OutStreamer->emitLabel(SectionStartSym);
    Sec->setBeginSymbol(SectionStartSym);
  }
}

void EVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EVMMCInstLower MCInstLowering(OutContext, *this, VRegMapping,
                                MF->getRegInfo());

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

void EVMAsmPrinter::emitStartOfAsmFile(Module &M) {
  MCSymbol *RuntimeSizeSym =
      OutContext.getOrCreateSymbol("__datasize_D.D_runtime");
  OutStreamer->emitLabel(RuntimeSizeSym);

  // BeginCodeSectionSym = OutContext.createTempSymbol();
  // OutStreamer->emitLabel(BeginCodeSectionSym);
}

void EVMAsmPrinter::emitEndOfAsmFile(Module &M) {
  /*EndCodeSectionSym = OutContext.createTempSymbol();
  OutStreamer->emitLabel(EndCodeSectionSym);
  const MCExpr *CodeSizeExp = MCBinaryExpr::createSub(
       MCSymbolRefExpr::create(EndCodeSectionSym, OutContext),
       MCSymbolRefExpr::create(EndCodeSectionSym, OutContext),
       OutContext);
  MCSymbol *SectionSizeSym = OutContext.getOrCreateSymbol("__codesec_size__");
  OutStreamer->emitELFSize(SectionSizeSym, CodeSizeExp);
  */
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMAsmPrinter() {
  const RegisterAsmPrinter<EVMAsmPrinter> X(getTheEVMTarget());
}
