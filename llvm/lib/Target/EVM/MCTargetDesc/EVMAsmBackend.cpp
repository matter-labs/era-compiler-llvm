//===-------- EVMAsmBackend.cpp - EVM Assembler Backend -------------------===//
//
// This file implements the EVMAsmBackend class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"

using namespace llvm;

namespace {
class EVMAsmBackend final : public MCAsmBackend {
  uint8_t OSABI;

public:
  EVMAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI)
      : MCAsmBackend(support::little), OSABI(OSABI) {}
  ~EVMAsmBackend() override {}

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createEVMELFObjectWriter(OSABI);
  }

  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    return false;
  }

  bool fixupNeedsRelaxationAdvanced(const MCFixup &Fixup, bool Resolved,
                                    uint64_t Value,
                                    const MCRelaxableFragment *DF,
                                    const MCAsmLayout &Layout,
                                    const bool WasForced) const override {
    return false;
  }

  unsigned getNumFixupKinds() const override { return 0; }

  bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override {
    return false;
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    return false;
  }
};

void EVMAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                               const MCValue &Target,
                               MutableArrayRef<char> Data, uint64_t Value,
                               bool IsResolved,
                               const MCSubtargetInfo *STI) const {
  return;
}

} // end anonymous namespace

MCAsmBackend *llvm::createEVMMCAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  return new EVMAsmBackend(STI, ELF::ELFOSABI_STANDALONE);
}
