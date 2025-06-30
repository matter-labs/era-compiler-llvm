//===------- EVMTargetStreamer.cpp - EVMTargetStreamer class --*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMTargetStreamer.h"
#include "EVMFixupKinds.h"
#include "EVMMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCFragment.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

// EVMTargetStreamer implementations

EVMTargetStreamer::EVMTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

void EVMTargetStreamer::emitLabel(MCSymbol *Symbol) {
  // This is mostly a workaround for the current linking scheme.
  // Mark all the symbols as local to their translation units.
  auto *ELFSymbol = cast<MCSymbolELF>(Symbol);
  ELFSymbol->setBinding(ELF::STB_LOCAL);
}

EVMTargetAsmStreamer::EVMTargetAsmStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

void EVMTargetAsmStreamer::emitWideRelocatableSymbol(const MCInst &PushInst,
                                                     StringRef SymbolName,
                                                     unsigned SymbolSize) {
  MCContext &Ctx = Streamer.getContext();
  const MCSubtargetInfo *STI = Ctx.getSubtargetInfo();
  Streamer.emitInstruction(PushInst, *STI);
}

EVMTargetObjStreamer::EVMTargetObjStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

// Emits a PUSH instruction with relocatable symbol of the size up to 32 bytes.
// The symbol value is represented as an array of 4-byte relocatable
// sub-symbols.
void EVMTargetObjStreamer::emitWideRelocatableSymbol(const MCInst &PushInst,
                                                     StringRef SymbolName,
                                                     unsigned SymbolSize) {
  if (SymbolSize > 32)
    report_fatal_error("MC: relocatable symbol size exceeds 32 bytes");

  // The code below is based on the MCObjectStreamer::emitInstToFragment()
  // implementation.
  MCContext &Ctx = Streamer.getContext();
  const MCSubtargetInfo *STI = Ctx.getSubtargetInfo();
  auto &S = static_cast<MCObjectStreamer &>(Streamer);

  auto *DF = Ctx.allocFragment<MCDataFragment>();
  S.insert(DF);
  SmallString<128> Code;
  S.getAssembler().getEmitter().encodeInstruction(PushInst, Code,
                                                  DF->getFixups(), *STI);
  // Remove a fixup corresponding to the initial symbol operand.
  DF->getFixups().clear();
  DF->getContents().append(Code.begin(), Code.end());

  // Emit 4-byte fixups to cover a wide symbol value.
  assert(DF->getContents().size() == SymbolSize + 1 /* opcode byte */);
  assert(!((DF->getContents().size() - 1) % 4));
  for (unsigned Idx = 0; Idx < SymbolSize / sizeof(uint32_t); ++Idx) {
    std::string SubSymName = EVM::getSymbolIndexedName(SymbolName, Idx);
    auto *Sym = cast<MCSymbolELF>(Ctx.getOrCreateSymbol(SubSymName));
    Sym->setOther(ELF::STO_ERAVM_REFERENCE_SYMBOL);
    const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);
    S.visitUsedExpr(*Expr);

    assert(SymbolSize > Idx * 4);
    // The byte index of start of the relocation is always 1, as
    // we need to skip the instruction opcode which is always one byte.
    constexpr auto FK = static_cast<MCFixupKind>(EVM::fixup_Data_i32);
    DF->getFixups().push_back(MCFixup::create((Idx * 4) + 1, Expr, FK));
  }
}
