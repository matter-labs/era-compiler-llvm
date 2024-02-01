//===-------- EVMAsmBackend.cpp - EVM Assembler Backend -----*- C++ -*-----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVMAsmBackend class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMFixupKinds.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#include <limits>

using namespace llvm;

#define DEBUG_TYPE "evm-asm-backend"

namespace {
class EVMAsmBackend final : public MCAsmBackend {
  uint8_t OSABI;
  std::unique_ptr<const MCInstrInfo> MCII;

public:
  EVMAsmBackend(const Target &T, const MCSubtargetInfo &STI, uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::big), OSABI(OSABI), MCII(T.createMCInstrInfo()) {}
  EVMAsmBackend(const EVMAsmBackend &) = delete;
  EVMAsmBackend(EVMAsmBackend &&) = delete;
  EVMAsmBackend &operator=(const EVMAsmBackend &) = delete;
  EVMAsmBackend &operator=(EVMAsmBackend &&) = delete;
  ~EVMAsmBackend() override = default;

  void applyFixup(const MCFragment &, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved) override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createEVMELFObjectWriter(OSABI);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  std::optional<bool> evaluateFixup(const MCFragment &, MCFixup &, MCValue &,
                                    uint64_t &) override;

  bool fixupNeedsRelaxation(const MCFixup &Fixup,
                            uint64_t Value) const override {
    llvm_unreachable("Handled by fixupNeedsRelaxationAdvanced");
  }

  bool fixupNeedsRelaxationAdvanced(const MCFixup &, const MCValue &, uint64_t,
                                    bool Resolved) const override;

  bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                         const MCSubtargetInfo &STI) const override;

  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;
};
} // end anonymous namespace

bool EVMAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                 const MCSubtargetInfo *STI) const {
  for (uint64_t I = 0; I < Count; ++I)
    OS << char(EVM::INVALID);

  return true;
}

MCFixupKindInfo EVMAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static std::array<MCFixupKindInfo, EVM::NumTargetFixupKinds> Infos = {
      {// This table *must* be in the order that the fixup_* kinds are defined
       // in EVMFixupKinds.h.
       //
       // Name          Offset  Size  Flags
       //               (bits) (bits)
       {"fixup_SecRel_i32", 0, 8 * 4, 0},
       {"fixup_SecRel_i24", 0, 8 * 3, 0},
       {"fixup_SecRel_i16", 0, 8 * 2, 0},
       {"fixup_SecRel_i8",  0, 8 * 1, 0},
       {"fixup_Data_i32",   0, 8 * 4, 0}
      }};

  if (Kind < FirstTargetFixupKind)
    llvm_unreachable("Unexpected fixup kind");

  assert(static_cast<unsigned>(Kind - FirstTargetFixupKind) <
             EVM::NumTargetFixupKinds &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

std::optional<bool> EVMAsmBackend::evaluateFixup(const MCFragment &F,
                                                 MCFixup &Fixup,
                                                 MCValue &Target,
                                                 uint64_t &Value) {
  unsigned FixUpKind = Fixup.getKind();
  assert(static_cast<unsigned>(FixUpKind - FirstTargetFixupKind) <
             EVM::NumTargetFixupKinds &&
         "Invalid kind!");

  // The following fixups should be emited as relocations,
  // as they can only be resolved at link time.
  if (FixUpKind == EVM::fixup_Data_i32)
    return false;

  Value = Target.getConstant();
  if (Value > std::numeric_limits<uint32_t>::max())
    report_fatal_error("Fixup value exceeds the displacement 2^32");

  const MCSymbol *Sym = Target.getAddSym();
  assert(Sym->isDefined());
  Value += Asm->getSymbolOffset(*Sym);
  return true;
}

void EVMAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                               const MCValue &Target,
                               MutableArrayRef<char> Data, uint64_t Value,
                               bool IsResolved) {
  if (!IsResolved)
    return;

  // Doesn't change encoding.
  if (Value == 0)
    return;

  const MCFixupKindInfo &Info = getFixupKindInfo(Fixup.getKind());
  unsigned NumBytes = alignTo(Info.TargetSize, 8) / 8;
  unsigned Offset = Fixup.getOffset();
  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  LLVM_DEBUG(dbgs() << "applyFixup: value: " << Value
                    << ", nbytes: " << NumBytes << ", sym: ");
  LLVM_DEBUG(Target.getAddSym()->print(dbgs(), nullptr));
  LLVM_DEBUG(dbgs() << '\n');

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  for (unsigned I = 0; I != NumBytes; ++I)
    Data[Offset + I] |= uint8_t((Value >> ((NumBytes - I - 1) * 8)) & 0xff);
}

void EVMAsmBackend::relaxInstruction(MCInst &Inst,
                                     const MCSubtargetInfo &STI) const {
  // On each iteration of the relaxation process we try to decrease on one the
  // byte width of the value to be pushed.
  switch (Inst.getOpcode()) {
  default:
    llvm_unreachable("Unexpected instruction for relaxation");
  case EVM::PUSH4_S:
    Inst.setOpcode(EVM::PUSH3_S);
    break;
  case EVM::PUSH3_S:
    Inst.setOpcode(EVM::PUSH2_S);
    break;
  case EVM::PUSH2_S:
    Inst.setOpcode(EVM::PUSH1_S);
    break;
  }
}

bool EVMAsmBackend::fixupNeedsRelaxationAdvanced(const MCFixup &Fixup,
                                                 const MCValue &, uint64_t Value,
                                                 bool Resolved) const {
  unsigned FixUpKind = Fixup.getKind();
  // The following fixups shouls always be emited as relocations,
  // as they can only be resolved at linking time.
  if (FixUpKind == EVM::fixup_Data_i32)
    return false;

  assert(Resolved);
  unsigned Opcode = EVM::getPUSHOpcode(APInt(256, Value));
  // The first byte of an instruction is an opcode, so
  // subtract it from the total size to get size of an immediate.
  unsigned OffsetByteWidth = MCII->get(Opcode).getSize() - 1;

  LLVM_DEBUG(dbgs() << "fixupNeedsRelaxationAdvanced : value: " << Value
                    << ", OffsetByteWidth: " << OffsetByteWidth << ", sym: ");
  LLVM_DEBUG(getContext().getAsmInfo()->printExpr(dbgs(), *Fixup.getValue()));
  LLVM_DEBUG(dbgs() << '\n');

  switch (FixUpKind) {
  default:
    llvm_unreachable("Unexpected target fixup kind");
  case EVM::fixup_SecRel_i32:
    return OffsetByteWidth < 4;
  case EVM::fixup_SecRel_i24:
    return OffsetByteWidth < 3;
  case EVM::fixup_SecRel_i16:
    return OffsetByteWidth < 2;
  }
}

bool EVMAsmBackend::mayNeedRelaxation(unsigned Opcode,
                                      ArrayRef<MCOperand> Operands,
                                      const MCSubtargetInfo &STI) const {
  switch (Opcode) {
  default:
    return false;
  case EVM::PUSH4_S:
  case EVM::PUSH3_S:
  case EVM::PUSH2_S:
    return Operands[0].isExpr();
  }
}

MCAsmBackend *llvm::createEVMMCAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  return new EVMAsmBackend(T, STI, ELF::ELFOSABI_STANDALONE);
}
