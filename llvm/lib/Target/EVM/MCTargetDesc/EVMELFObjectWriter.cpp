//===-------- EVMELFObjectWriter.cpp - EVM ELF Writer ---------------------===//
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
  EVMELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_NONE,
                                /*HasRelocationAddend*/ true) {}

  ~EVMELFObjectWriter() override {}

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
