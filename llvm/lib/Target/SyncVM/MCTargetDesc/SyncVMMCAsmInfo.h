//===-- SyncVMMCAsmInfo.h - SyncVM asm properties --------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the SyncVMMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SyncVM_MCTARGETDESC_SyncVMMCASMINFO_H
#define LLVM_LIB_TARGET_SyncVM_MCTARGETDESC_SyncVMMCASMINFO_H

#include "llvm/MC/MCAsmInfoCOFF.h"
#include "llvm/MC/MCAsmInfoDarwin.h"
#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class SyncVMMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit SyncVMMCAsmInfo(const Triple &TT);
  bool shouldOmitSectionDirective(StringRef) const override { return true; }
};

} // namespace llvm

#endif
