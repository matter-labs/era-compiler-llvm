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

#include "MCTargetDesc/EVMFixupKinds.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/MC/MCELFObjectWriter.h"

using namespace llvm;

namespace {
class EVMELFObjectWriter final : public MCELFObjectTargetWriter {
public:
  explicit EVMELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_EVM,
                                /*HasRelocationAddend*/ true) {}

  EVMELFObjectWriter(const EVMELFObjectWriter &) = delete;
  EVMELFObjectWriter(EVMELFObjectWriter &&) = delete;
  EVMELFObjectWriter &operator=(EVMELFObjectWriter &&) = delete;
  EVMELFObjectWriter &operator=(const EVMELFObjectWriter &) = delete;
  ~EVMELFObjectWriter() override = default;

protected:
  bool needsRelocateWithSymbol(const MCSymbol &Sym,
                               unsigned Type) const override;

  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;
};
} // end of anonymous namespace

bool EVMELFObjectWriter::needsRelocateWithSymbol(const MCSymbol &Sym,
                                                 unsigned Type) const {
  // We support only relocations with a symbol, not section.
  return true;
}

unsigned EVMELFObjectWriter::getRelocType(MCContext &Ctx, const MCValue &Target,
                                          const MCFixup &Fixup,
                                          bool IsPCRel) const {
  // Translate fixup kind to ELF relocation type.
  switch (Fixup.getTargetKind()) {
  default:
    llvm_unreachable("Unexpected EVM fixup");
  case EVM::fixup_Data_i32:
    return ELF::R_EVM_DATA;
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createEVMELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<EVMELFObjectWriter>(OSABI);
}
