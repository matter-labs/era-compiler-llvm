//===-- EraVMMCAsmInfo.h - EraVM asm properties ----------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the EraVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMMCASMINFO_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMMCASMINFO_H

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAsmInfoCOFF.h"
#include "llvm/MC/MCAsmInfoDarwin.h"

namespace llvm {
class Triple;

class EraVMMCAsmInfo : public MCAsmInfo {
public:
  explicit EraVMMCAsmInfo(const Triple &TheTriple);
  bool shouldOmitSectionDirective(StringRef Name) const override;
  MCSection *getNonexecutableStackSection(MCContext &Ctx) const override {
    return nullptr;
  }
};

} // namespace llvm

#endif
