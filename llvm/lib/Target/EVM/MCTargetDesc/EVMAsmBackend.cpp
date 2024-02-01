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
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "evm-asm-backend"

namespace {
class EVMAsmBackend final : public MCAsmBackend {
  uint8_t OSABI;
  std::unique_ptr<const MCInstrInfo> MCII;

public:
  EVMAsmBackend(const Target &T, const MCSubtargetInfo &STI, uint8_t OSABI)
      : MCAsmBackend(support::big), OSABI(OSABI), MCII(T.createMCInstrInfo()) {}
  EVMAsmBackend(const EVMAsmBackend &) = delete;
  EVMAsmBackend(EVMAsmBackend &&) = delete;
  EVMAsmBackend &operator=(const EVMAsmBackend &) = delete;
  EVMAsmBackend &operator=(EVMAsmBackend &&) = delete;
  ~EVMAsmBackend() override = default;

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createEVMELFObjectWriter(OSABI);
  }

  unsigned getNumFixupKinds() const override {
    return EVM::NumTargetFixupKinds;
  }

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;

  bool evaluateTargetFixup(const MCAssembler &Asm, const MCAsmLayout &Layout,
                           const MCFixup &Fixup, const MCFragment *DF,
                           const MCValue &Target, uint64_t &Value,
                           bool &WasForced) override;

  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    llvm_unreachable("Handled by fixupNeedsRelaxationAdvanced");
  }

  bool fixupNeedsRelaxationAdvanced(const MCFixup &Fixup, bool Resolved,
                                    uint64_t Value,
                                    const MCRelaxableFragment *DF,
                                    const MCAsmLayout &Layout,
                                    bool WasForced) const override;

  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override;

  bool mayNeedRelaxation(const MCInst &Inst,
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

const MCFixupKindInfo &EVMAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static std::array<MCFixupKindInfo, EVM::NumTargetFixupKinds> Infos = {
      {// This table *must* be in the order that the fixup_* kinds are defined
       // in EVMFixupKinds.h.
       //
       // Name             Offset (bits) Size (bits) Flags
       {"fixup_SecRel_i64", 0, 8 * 8, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i56", 0, 8 * 7, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i48", 0, 8 * 6, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i40", 0, 8 * 5, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i32", 0, 8 * 4, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i24", 0, 8 * 3, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i16", 0, 8 * 2, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_SecRel_i8", 0, 8 * 1, MCFixupKindInfo::FKF_IsTarget},
       {"fixup_Data_i32", 0, 8 * 4, MCFixupKindInfo::FKF_IsTarget}}};

  if (Kind < FirstTargetFixupKind)
    llvm_unreachable("Unexpected fixup kind");

  assert(static_cast<unsigned>(Kind - FirstTargetFixupKind) <
             getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

bool EVMAsmBackend::evaluateTargetFixup(const MCAssembler &Asm,
                                        const MCAsmLayout &Layout,
                                        const MCFixup &Fixup,
                                        const MCFragment *DF,
                                        const MCValue &Target, uint64_t &Value,
                                        bool &WasForced) {
  assert(unsigned(Fixup.getTargetKind() - FirstTargetFixupKind) <
             getNumFixupKinds() &&
         "Invalid kind!");

  // The following fixups should be emited as relocations,
  // as they can only be resolved at link time.
  unsigned FixUpKind = Fixup.getTargetKind();
  if (FixUpKind == EVM::fixup_Data_i32)
    return false;

  Value = Target.getConstant();
  if (const MCSymbolRefExpr *A = Target.getSymA()) {
    const MCSymbol &Sym = A->getSymbol();
    assert(Sym.isDefined());
    Value += Layout.getSymbolOffset(Sym);
    return true;
  }
  llvm_unreachable("Unexpect target MCValue");
}

void EVMAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                               const MCValue &Target,
                               MutableArrayRef<char> Data, uint64_t Value,
                               bool IsResolved,
                               const MCSubtargetInfo *STI) const {
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
  LLVM_DEBUG(Target.getSymA()->print(dbgs(), nullptr));
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
  case EVM::PUSH8_S:
    Inst.setOpcode(EVM::PUSH7_S);
    break;
  case EVM::PUSH7_S:
    Inst.setOpcode(EVM::PUSH6_S);
    break;
  case EVM::PUSH6_S:
    Inst.setOpcode(EVM::PUSH5_S);
    break;
  case EVM::PUSH5_S:
    Inst.setOpcode(EVM::PUSH4_S);
    break;
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
                                                 bool Resolved, uint64_t Value,
                                                 const MCRelaxableFragment *DF,
                                                 const MCAsmLayout &Layout,
                                                 const bool WasForced) const {
  unsigned FixUpKind = Fixup.getTargetKind();
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
  LLVM_DEBUG(Fixup.getValue()->print(dbgs(), nullptr));
  LLVM_DEBUG(dbgs() << '\n');

  switch (FixUpKind) {
  default:
    llvm_unreachable("Unexpected target fixup kind");
  case EVM::fixup_SecRel_i64:
    return OffsetByteWidth < 8;
  case EVM::fixup_SecRel_i56:
    return OffsetByteWidth < 7;
  case EVM::fixup_SecRel_i48:
    return OffsetByteWidth < 6;
  case EVM::fixup_SecRel_i40:
    return OffsetByteWidth < 5;
  case EVM::fixup_SecRel_i32:
    return OffsetByteWidth < 4;
  case EVM::fixup_SecRel_i24:
    return OffsetByteWidth < 3;
  case EVM::fixup_SecRel_i16:
    return OffsetByteWidth < 2;
  }
}

bool EVMAsmBackend::mayNeedRelaxation(const MCInst &Inst,
                                      const MCSubtargetInfo &STI) const {
  switch (Inst.getOpcode()) {
  default:
    return false;
  case EVM::PUSH8_S:
  case EVM::PUSH7_S:
  case EVM::PUSH6_S:
  case EVM::PUSH5_S:
  case EVM::PUSH4_S:
  case EVM::PUSH3_S:
  case EVM::PUSH2_S:
    return Inst.getOperand(0).isExpr();
  }
}

MCAsmBackend *llvm::createEVMMCAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  return new EVMAsmBackend(T, STI, ELF::ELFOSABI_STANDALONE);
}
