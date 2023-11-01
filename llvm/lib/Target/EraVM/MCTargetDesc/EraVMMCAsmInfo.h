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

#include "llvm/MC/MCAsmInfoCOFF.h"
#include "llvm/MC/MCAsmInfoDarwin.h"
#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class EraVMMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit EraVMMCAsmInfo(const Triple &TT);
  bool shouldOmitSectionDirective(StringRef) const override { return true; }
};

} // namespace llvm

#endif
