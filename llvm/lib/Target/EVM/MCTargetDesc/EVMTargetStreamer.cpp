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
#include "llvm/BinaryFormat/ELF.h"
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

EVMTargetObjStreamer::EVMTargetObjStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetObjStreamer::~EVMTargetObjStreamer() = default;

EVMTargetAsmStreamer::EVMTargetAsmStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetAsmStreamer::~EVMTargetAsmStreamer() = default;
