//===----- EVMMCExpr.cpp - EVM specific MC expression classes -*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Define EVM-specific MC classes.
//
//===----------------------------------------------------------------------===//

#include "EVMMCExpr.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

#define DEBUG_TYPE "evm-mcexpr"

const EVMCImmMCExpr *EVMCImmMCExpr::create(const StringRef &Data,
                                           MCContext &Ctx) {
  return new (Ctx) EVMCImmMCExpr(Data);
}

void EVMCImmMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  OS << "0x" << Data;
}
