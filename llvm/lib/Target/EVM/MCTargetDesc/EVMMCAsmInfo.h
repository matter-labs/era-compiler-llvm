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

namespace llvm {
class Triple;

// Inherit from MCAsmInfo instead of MCAsmInfoELF to enable overriding
// getNonexecutableStackSection(), as the base class functionality is minimal
// in this context.
class EVMMCAsmInfo final : public MCAsmInfo {
public:
  explicit EVMMCAsmInfo(const Triple &TT);
  bool shouldOmitSectionDirective(StringRef) const override;
  MCSection *getNonexecutableStackSection(MCContext &Ctx) const override {
    return nullptr;
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H
