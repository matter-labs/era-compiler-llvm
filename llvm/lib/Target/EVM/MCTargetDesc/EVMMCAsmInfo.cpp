//===-------- EVMMCAsmInfo.cpp - EVM asm properties ------*- C++ -*--------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the EVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "EVMMCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

#define DEBUG_TYPE "evm-mc-asm-info"

const MCAsmInfo::AtSpecifier atSpecifiers[] = {
    {EVM::S_DATA, "DATA"},
};

EVMMCAsmInfo::EVMMCAsmInfo(const Triple &TT) {
  IsLittleEndian = false;
  HasFunctionAlignment = false;
  HasDotTypeDotSizeDirective = false;
  PrivateGlobalPrefix = ".";
  PrivateLabelPrefix = ".";
  AlignmentIsInBytes = true;
  PrependSymbolRefWithAt = true;
  CommentString = ";";
  SupportsDebugInformation = true;

  initializeAtSpecifiers(atSpecifiers);
}

bool EVMMCAsmInfo::shouldOmitSectionDirective(StringRef) const { return true; }

void EVMMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                      const MCSpecifierExpr &Expr) const {
  StringRef S = EVM::getSpecifierName(Expr.getSpecifier());
  if (!S.empty())
    OS << '%' << S << '(';
  printExpr(OS, *Expr.getSubExpr());
  if (!S.empty())
    OS << ')';
}
