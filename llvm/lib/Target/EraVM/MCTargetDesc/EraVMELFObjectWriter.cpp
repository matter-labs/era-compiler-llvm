//===-- EraVMELFObjectWriter.cpp - EraVM ELF Writer -----------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EraVMFixupKinds.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"

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
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_NONE,
                                /*HasRelocationAddend*/ true) {}

  ~EraVMELFObjectWriter() override = default;

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override {
    // Translate fixup kind to ELF relocation type.
    switch (Fixup.getTargetKind()) {
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
