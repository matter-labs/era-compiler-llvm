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
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCSymbol.h"

using namespace llvm;

// EVMTargetStreamer implementations

EVMTargetStreamer::EVMTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

EVMTargetStreamer::~EVMTargetStreamer() = default;

void EVMTargetStreamer::finish() {
  MCContext &Ctx = Streamer.getContext();
  MCSection *TextSection = Ctx.getObjectFileInfo()->getTextSection();
  Streamer.switchSection /*NoChange*/ (TextSection);
  MCSymbol *TextEndSym =
      getStreamer().getCurrentSectionOnly()->getEndSymbol(Ctx);
  if (!TextEndSym->isInSection())
    Streamer.emitLabel(TextEndSym);

  const MCExpr *TextEndExp =
      MCSymbolRefExpr::create(TextEndSym, MCSymbolRefExpr::VK_None, Ctx);
  const MCExpr *TextStartExp = MCSymbolRefExpr::create(
      TextSection->getBeginSymbol(), MCSymbolRefExpr::VK_None, Ctx);

  const MCExpr *Size = MCBinaryExpr::createSub(TextEndExp, TextStartExp, Ctx);
  MCSymbol *TextSizeSym = Ctx.getOrCreateSymbol("__text_size__");
  TextSizeSym->setVariableValue(Size);
  Streamer.emitELFSize(TextSizeSym, Size);
}

EVMTargetObjStreamer::EVMTargetObjStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetObjStreamer::~EVMTargetObjStreamer() = default;

EVMTargetAsmStreamer::EVMTargetAsmStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetAsmStreamer::~EVMTargetAsmStreamer() = default;
