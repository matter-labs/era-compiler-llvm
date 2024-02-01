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
#include "MCTargetDesc/EVMMCAsmInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

#define DEBUG_TYPE "evm-mcexpr"

const EVMCImmMCExpr *EVMCImmMCExpr::create(const StringRef &Data,
                                           MCContext &Ctx) {
  return new (Ctx) EVMCImmMCExpr(Data);
}

void EVMCImmMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  OS << Data;
}

EVM::Specifier EVM::parseSpecifierName(StringRef name) {
  return StringSwitch<EVM::Specifier>(name)
      .Case("evm_data", EVM::S_DATA)
      .Default(EVM::S_None);
}

StringRef EVM::getSpecifierName(Specifier S) {
  switch (S) {
  default:
    llvm_unreachable("not used as %specifier()");
  case EVM::S_DATA:
    return "evm_data";
  }
}
      
