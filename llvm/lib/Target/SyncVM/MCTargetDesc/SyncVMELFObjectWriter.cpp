//===-- SyncVMELFObjectWriter.cpp - SyncVM ELF Writer ---------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SyncVMFixupKinds.h"
#include "MCTargetDesc/SyncVMMCTargetDesc.h"

#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class SyncVMELFObjectWriter : public MCELFObjectTargetWriter {
public:
  SyncVMELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_MSP430,
      /*HasRelocationAddend*/ true) {}

  ~SyncVMELFObjectWriter() override {}

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
llvm::createSyncVMELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<SyncVMELFObjectWriter>(OSABI);
}
