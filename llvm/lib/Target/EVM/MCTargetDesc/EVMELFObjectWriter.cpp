//===-------- EVMELFObjectWriter.cpp - EVM ELF Writer ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file handles EVM-specific object emission, converting LLVM's
// internal fixups into the appropriate relocations.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/MC/MCELFObjectWriter.h"

using namespace llvm;

namespace {
class EVMELFObjectWriter final : public MCELFObjectTargetWriter {
public:
  explicit EVMELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_NONE,
                                /*HasRelocationAddend*/ true){};

  EVMELFObjectWriter(const EVMELFObjectWriter &) = delete;
  EVMELFObjectWriter(EVMELFObjectWriter &&) = delete;
  EVMELFObjectWriter &operator=(EVMELFObjectWriter &&) = delete;
  EVMELFObjectWriter &operator=(const EVMELFObjectWriter &) = delete;
  ~EVMELFObjectWriter() override = default;

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override {
    // Translate fixup kind to ELF relocation type.
    switch (Fixup.getTargetKind()) {
    default:
      llvm_unreachable("Fixups are not supported for EVM");
    }
  }
};
} // end of anonymous namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createEVMELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<EVMELFObjectWriter>(OSABI);
}
