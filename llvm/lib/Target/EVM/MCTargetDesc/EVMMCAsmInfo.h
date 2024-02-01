//===------ EVMMCAsmInfo.h - EVM asm properties --------------*- C++ -*----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the EVMMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"
#include "llvm/MC/MCFixup.h"

namespace llvm {
class Triple;

class EVMMCAsmInfo final : public MCAsmInfoELF {
public:
  explicit EVMMCAsmInfo(const Triple &TT);
  bool shouldOmitSectionDirective(StringRef) const override;
  void printSpecifierExpr(raw_ostream &OS,
                          const MCSpecifierExpr &Expr) const override;
};

namespace EVM {
using Specifier = uint16_t;

enum {
  S_None,
  S_DATA = FirstTargetFixupKind,
};

Specifier parseSpecifierName(StringRef name);
StringRef getSpecifierName(Specifier S);
} // namespace EVM

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H
