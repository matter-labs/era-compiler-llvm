//===-------- EVMMCExpr.h - EVM specific MC expression classes --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Define EVM specific MC expression classes
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCEXPR_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCEXPR_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCExpr.h"

namespace llvm {

class EVMCImmMCExpr final : public MCTargetExpr {
private:
  StringRef Data;

  explicit EVMCImmMCExpr(const StringRef &Data) : Data(Data) {}

public:
  static const EVMCImmMCExpr *create(const StringRef &Data, MCContext &Ctx);

  StringRef getString() const { return Data; }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;

  bool evaluateAsRelocatableImpl(MCValue &Res, const MCAssembler *Asm,
                                 const MCFixup *Fixup) const override {
    return false;
  }

  void visitUsedExpr(MCStreamer &Streamer) const override{};

  MCFragment *findAssociatedFragment() const override { return nullptr; }

  void fixELFSymbolsInTLSFixups(MCAssembler &Asm) const override {}
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCEXPR_H
