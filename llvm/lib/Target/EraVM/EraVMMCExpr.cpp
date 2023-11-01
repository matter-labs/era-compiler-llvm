//===-- EraVMMCExpr.cpp - EraVM specific MC expression classes --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements EraVM-specific MC classes.
//
//===----------------------------------------------------------------------===//

#include "EraVMMCExpr.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
using namespace llvm;

#define DEBUG_TYPE "tvm-mcexpr"

const EraVMCImmMCExpr *EraVMCImmMCExpr::create(const StringRef &Data,
                                               MCContext &Ctx) {
  return new (Ctx) EraVMCImmMCExpr(Data);
}

void EraVMCImmMCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  OS << Data;
}
