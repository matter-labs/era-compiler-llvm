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
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

namespace llvm {

class EraVMTargetELFStreamer : public EraVMTargetStreamer {
public:
  MCELFStreamer &getStreamer();
  EraVMTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
  void emitCell(const APInt &Value) override;
  void emitJumpTarget(const MCExpr *Expr) override;
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
