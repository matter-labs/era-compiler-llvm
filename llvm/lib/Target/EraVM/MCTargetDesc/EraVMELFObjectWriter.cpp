//===-- EraVMELFObjectWriter.cpp - EraVM ELF Writer -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM ELF Writer.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EraVMFixupKinds.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"

#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class EraVMELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit EraVMELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_ERAVM,
                                /*HasRelocationAddend*/ true) {}

  ~EraVMELFObjectWriter() override = default;
  EraVMELFObjectWriter(const EraVMELFObjectWriter &) = delete;
  EraVMELFObjectWriter &operator=(EraVMELFObjectWriter &) = delete;
  EraVMELFObjectWriter(EraVMELFObjectWriter &&) = delete;
  EraVMELFObjectWriter &&operator=(EraVMELFObjectWriter &&) = delete;

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override {
    // Translate fixup kind to ELF relocation type.
    switch (Fixup.getTargetKind()) {
    case EraVM::fixup_16_scale_32:
      return ELF::R_ERAVM_16_SCALE_32;
    case EraVM::fixup_16_scale_8:
      return ELF::R_ERAVM_16_SCALE_8;
    case FK_Data_4:
      return ELF::R_ERAVM_32;
    default:
      llvm_unreachable("Invalid fixup kind");
    }
  }
};
} // end of anonymous namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createEraVMELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<EraVMELFObjectWriter>(OSABI);
}
