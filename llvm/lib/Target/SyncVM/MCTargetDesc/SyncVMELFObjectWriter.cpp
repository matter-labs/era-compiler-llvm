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
      : MCELFObjectTargetWriter(false, OSABI, ELF::EM_SYNCVM,
      /*HasRelocationAddend*/ true) {}

  ~SyncVMELFObjectWriter() override {}

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override {
    // Translate fixup kind to ELF relocation type.
    switch (Fixup.getTargetKind()) {
    case FK_Data_1:                   return ELF::R_SYNCVM_8;
    case FK_Data_2:                   return ELF::R_SYNCVM_16_BYTE;
    case FK_Data_4:                   return ELF::R_SYNCVM_32;
    case SyncVM::fixup_32:            return ELF::R_SYNCVM_32;
    case SyncVM::fixup_10_pcrel:      return ELF::R_SYNCVM_10_PCREL;
    case SyncVM::fixup_16:            return ELF::R_SYNCVM_16;
    case SyncVM::fixup_16_pcrel:      return ELF::R_SYNCVM_16_PCREL;
    case SyncVM::fixup_16_byte:       return ELF::R_SYNCVM_16_BYTE;
    case SyncVM::fixup_16_pcrel_byte: return ELF::R_SYNCVM_16_PCREL_BYTE;
    case SyncVM::fixup_2x_pcrel:      return ELF::R_SYNCVM_2X_PCREL;
    case SyncVM::fixup_rl_pcrel:      return ELF::R_SYNCVM_RL_PCREL;
    case SyncVM::fixup_8:             return ELF::R_SYNCVM_8;
    case SyncVM::fixup_sym_diff:      return ELF::R_SYNCVM_SYM_DIFF;
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
