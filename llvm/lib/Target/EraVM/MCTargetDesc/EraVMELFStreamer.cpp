//===-- EraVMELFStreamer.cpp - EraVM ELF Streamer ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "EraVMFixupKinds.h"
#include "EraVMMCTargetDesc.h"
#include "EraVMTargetStreamer.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

#include <functional>

using namespace llvm;

namespace llvm {

class EraVMTargetELFStreamer : public EraVMTargetStreamer {
public:
  MCELFStreamer &getStreamer();
  EraVMTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
  void emitCell(const APInt &Value) override;
  void emitJumpTarget(const MCExpr *Expr) override;
  void emitLinkerSymbol(StringRef Symbol) override;
  void emitFactoryDependencySymbol(StringRef SymbolName) override;

private:
  void emitWideRelocatableSymbol(
      StringRef SymbolName, unsigned SymbolSize,
      std::function<std::string(StringRef, unsigned)> SubName);
};

// This part is for ELF object output.
EraVMTargetELFStreamer::EraVMTargetELFStreamer(MCStreamer &S,
                                               const MCSubtargetInfo &STI)
    : EraVMTargetStreamer(S) {}

class EraVMTargetAsmStreamer : public EraVMTargetStreamer {
public:
  EraVMTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                         MCInstPrinter &InstPrinter, bool VerboseAsm);
  void emitCell(const APInt &Value) override;
  void emitJumpTarget(const MCExpr *Expr) override;
  void emitLinkerSymbol(StringRef Symbol) override;
  void emitFactoryDependencySymbol(StringRef SymbolName) override;
};

void EraVMTargetELFStreamer::emitCell(const APInt &Value) {
  assert(Value.getBitWidth() <= EraVM::CellBitWidth);
  // aligned by 256 bit
  Streamer.emitValueToAlignment(Align(EraVM::CellBitWidth / 8));
  Streamer.emitIntValue(Value.sext(EraVM::CellBitWidth));
}

void EraVMTargetELFStreamer::emitJumpTarget(const MCExpr *Expr) {
  // The code is similar to MCObjectStreamer::emitValueImpl, but takes the
  // specifics of code labels into account: the instruction index is actually
  // only 16 bits in size and is counted in 8-byte units.

  constexpr auto FK = static_cast<MCFixupKind>(EraVM::fixup_16_scale_8);
  auto &S = static_cast<MCObjectStreamer &>(Streamer);

  S.visitUsedExpr(*Expr);

  // Emit the placeholder.
  emitCell(APInt::getZero(EraVM::CellBitWidth));

  // Emit the fixup.
  auto *DF = cast<MCDataFragment>(S.getCurrentFragment());
  // Offset of the 16 least significant bits of 256-bit value.
  unsigned Offset = DF->getContents().size() - 2;
  DF->getFixups().push_back(MCFixup::create(Offset, Expr, FK));
}

// Emits a relocatable symbol of the size up to 32 bytes. The symbol
// value is represented as an array of 4-byte relocatable
// sub-symbols.
void EraVMTargetELFStreamer::emitWideRelocatableSymbol(
    StringRef SymbolName, unsigned SymbolSize,
    std::function<std::string(StringRef, unsigned)> SubNameFunc) {
  if (SymbolSize * 8 > EraVM::CellBitWidth)
    report_fatal_error("MC: relocatable symbol size exceeds the cell width");

  // Emit the placeholder.
  emitCell(APInt::getZero(EraVM::CellBitWidth));

  MCContext &Ctx = Streamer.getContext();
  auto &S = static_cast<MCObjectStreamer &>(Streamer);
  auto *DF = cast<MCDataFragment>(S.getCurrentFragment());

  // Emits 4-byte fixup to cover a part of the wide symbol value.
  auto EmitFixup = [&S, &Ctx, &SymbolName, &SubNameFunc, SymbolSize,
                    &DF](unsigned Idx) {
    std::string SubSymName = SubNameFunc(SymbolName, Idx);
    if (Ctx.lookupSymbol(SubSymName))
      report_fatal_error(Twine("MC: duplicating reference sub-symbol ") +
                         SubSymName);

    auto *Sym = cast<MCSymbolELF>(Ctx.getOrCreateSymbol(SubSymName));
    Sym->setOther(ELF::STO_ERAVM_REFERENCE_SYMBOL);
    const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);
    S.visitUsedExpr(*Expr);

    assert(DF->getContents().size() == 32 && SymbolSize > Idx * 4);
    unsigned Offset = DF->getContents().size() - (SymbolSize - Idx * 4);
    DF->getFixups().push_back(MCFixup::create(Offset, Expr, FK_Data_4));
  };

  for (unsigned Idx = 0; Idx < SymbolSize / sizeof(uint32_t); ++Idx)
    EmitFixup(Idx);
}

void EraVMTargetELFStreamer::emitFactoryDependencySymbol(StringRef SymbolName) {
  constexpr unsigned SymbolSize = 32;
  emitWideRelocatableSymbol(SymbolName, SymbolSize,
                            &EraVM::getSymbolIndexedName);
}

void EraVMTargetELFStreamer::emitLinkerSymbol(StringRef SymbolName) {
  constexpr unsigned SymbolSize = 20;
  emitWideRelocatableSymbol(SymbolName, SymbolSize,
                            &EraVM::getSymbolIndexedName);
}

void EraVMTargetAsmStreamer::emitCell(const APInt &Value) {
  assert(Value.getBitWidth() <= EraVM::CellBitWidth);

  SmallString<86> Str;
  raw_svector_ostream OS(Str);
  OS << "\t.cell\t" << Value;

  Streamer.emitRawText(OS.str());
}

void EraVMTargetAsmStreamer::emitJumpTarget(const MCExpr *Expr) {
  Streamer.emitValue(Expr, EraVM::CellBitWidth / 8);
}

void EraVMTargetAsmStreamer::emitLinkerSymbol(StringRef SymbolName) {
  // This is almost a copy of MCTargetStreamer::emitValue() implementation.
  MCContext &Ctx = Streamer.getContext();
  const MCExpr *Expr = MCSymbolRefExpr::create(
      SymbolName, MCSymbolRefExpr::VariantKind::VK_None, Ctx);
  SmallString<128> Str;
  raw_svector_ostream OS(Str);
  OS << "\t.reference_symbol_cell\t";
  Expr->print(OS, Ctx.getAsmInfo());
  Streamer.emitRawText(OS.str());
}

void EraVMTargetAsmStreamer::emitFactoryDependencySymbol(StringRef SymbolName) {
  // The asm syntax of both reference symbol types is the same.
  emitLinkerSymbol(SymbolName);
}

EraVMTargetAsmStreamer::EraVMTargetAsmStreamer(MCStreamer &S,
                                               formatted_raw_ostream &OS,
                                               MCInstPrinter &InstPrinter,
                                               bool VerboseAsm)
    : EraVMTargetStreamer(S) {}

MCELFStreamer &EraVMTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}
MCTargetStreamer *createEraVMTargetAsmStreamer(MCStreamer &S,
                                               formatted_raw_ostream &OS,
                                               MCInstPrinter *InstPrint,
                                               bool isVerboseAsm) {
  return new EraVMTargetAsmStreamer(S, OS, *InstPrint, isVerboseAsm);
}

MCTargetStreamer *createEraVMNullTargetStreamer(MCStreamer &S) {
  return new EraVMTargetStreamer(S);
}

MCTargetStreamer *createEraVMObjectTargetStreamer(MCStreamer &S,
                                                  const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new EraVMTargetELFStreamer(S, STI);
  return new EraVMTargetStreamer(S);
}
} // namespace llvm
