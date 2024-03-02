//===-- EraVMAsmBackend.cpp - EraVM Assembler Backend -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM Assembler Backend.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EraVMFixupKinds.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class EraVMAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

  uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                            MCContext &Ctx) const;

public:
  EraVMAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI)
      : MCAsmBackend(support::little), OSABI(OSABI) {}
  ~EraVMAsmBackend() override = default;
  EraVMAsmBackend(const EraVMAsmBackend &) = delete;
  EraVMAsmBackend &operator=(const EraVMAsmBackend &) = delete;
  EraVMAsmBackend(EraVMAsmBackend &&) = delete;
  EraVMAsmBackend &&operator=(EraVMAsmBackend &&) = delete;

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createEraVMELFObjectWriter(OSABI);
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

  unsigned getNumFixupKinds() const override {
    return EraVM::NumTargetFixupKinds;
  }

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override {
    const static MCFixupKindInfo Infos[EraVM::NumTargetFixupKinds] = {
        // This table must be in the same order of enum in EraVMFixupKinds.h.
        //
        // name            offset bits flags
        {"fixup_32", 0, 32, 0},
        {"fixup_10_pcrel", 0, 10, MCFixupKindInfo::FKF_IsPCRel},
        {"fixup_16", 0, 16, 0},
        {"fixup_16_pcrel", 0, 16, MCFixupKindInfo::FKF_IsPCRel},
        {"fixup_16_byte", 0, 16, 0},
        {"fixup_16_pcrel_byte", 0, 16, MCFixupKindInfo::FKF_IsPCRel},
        {"fixup_2x_pcrel", 0, 10, MCFixupKindInfo::FKF_IsPCRel},
        {"fixup_rl_pcrel", 0, 16, MCFixupKindInfo::FKF_IsPCRel},
        {"fixup_8", 0, 8, 0},
        {"fixup_sym_diff", 0, 32, 0},
    };
    static_assert((std::size(Infos)) == EraVM::NumTargetFixupKinds,
                  "Not all fixup kinds added to Infos array");

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);

    return Infos[Kind - FirstTargetFixupKind];
  }

  bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override {
    return false;
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;
};

uint64_t EraVMAsmBackend::adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                           MCContext &Ctx) const {
  unsigned Kind = Fixup.getKind();
  if (Kind == EraVM::fixup_10_pcrel) {
    if (Value & 0x1)
      Ctx.reportError(Fixup.getLoc(), "fixup value must be 2-byte aligned");

    // Offset is signed
    int16_t Offset = Value;
    // Jumps are in words
    Offset >>= 1;
    // PC points to the next instruction so decrement by one
    --Offset;

    if (Offset < -512 || Offset > 511)
      Ctx.reportError(Fixup.getLoc(), "fixup value out of range");

    // Mask 10 bits
    Offset &= 0x3ff;

    return Offset;
  }
  return Value;
}

void EraVMAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                 const MCValue &Target,
                                 MutableArrayRef<char> Data, uint64_t Value,
                                 bool IsResolved,
                                 const MCSubtargetInfo *STI) const {
  Value = adjustFixupValue(Fixup, Value, Asm.getContext());
  MCFixupKindInfo Info = getFixupKindInfo(Fixup.getKind());
  if (!Value)
    return; // Doesn't change encoding.

  // Shift the value into position.
  Value <<= Info.TargetOffset;

  unsigned Offset = Fixup.getOffset();
  unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;

  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  for (unsigned i = 0; i != NumBytes; ++i) {
    Data[Offset + i] |= uint8_t((Value >> (i * 8)) & 0xff);
  }
}

bool EraVMAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                   const MCSubtargetInfo *STI) const {
  (void)STI;
  if ((Count % 2) != 0)
    return false;

  // The canonical nop on EraVM is mov #0, r3
  uint64_t NopCount = Count / 2;
  while (NopCount--)
    OS.write("\x03\x43", 2);

  return true;
}

} // end anonymous namespace

MCAsmBackend *llvm::createEraVMMCAsmBackend(const Target &T,
                                            const MCSubtargetInfo &STI,
                                            const MCRegisterInfo &MRI,
                                            const MCTargetOptions &Options) {
  return new EraVMAsmBackend(STI, ELF::ELFOSABI_STANDALONE);
}

static MCOperand createStackOperandMarker(EraVM::MemOperandKind Kind) {
  switch (Kind) {
  default:
    llvm_unreachable("Stack operand kind expected");
  case EraVM::OperandStackAbsolute:
    return MCOperand::createImm(0);
  case EraVM::OperandStackSPRelative:
    return MCOperand::createReg(EraVM::SP);
  case EraVM::OperandStackSPModifying:
    return MCOperand::createReg(EraVM::R0);
  }
}

static MCOperand createSymbolWithAddend(MCContext &Ctx, const MCSymbol *Symbol,
                                        int Addend) {
  // Use just an immediate operand, if possible.
  if (!Symbol)
    return MCOperand::createImm(Addend);

  // Fallback to constructing Symbol + Addend expression.
  const MCExpr *AddendExpr = MCConstantExpr::create(Addend, Ctx);
  const MCExpr *SymbolExpr = MCSymbolRefExpr::create(Symbol, Ctx);
  const MCExpr *Expr = MCBinaryExpr::createAdd(SymbolExpr, AddendExpr, Ctx);
  return MCOperand::createExpr(Expr);
}

static void appendCodeMCOperands(MCContext &Ctx, MCInst &MI, unsigned Reg,
                                 const MCSymbol *Symbol, int Addend) {
  // If address is a compile-time constant (possibly after relocations),
  // use r0 as a base register.
  if (Reg == 0)
    Reg = EraVM::R0;

  // First sub-operand: base register.
  MI.addOperand(MCOperand::createReg(Reg));

  // Second sub-operand: Symbol+Addend (effectively an immediate integer operand
  // after all relocations).
  MI.addOperand(createSymbolWithAddend(Ctx, Symbol, Addend));
}

static void appendStackMCOperands(MCContext &Ctx, MCInst &MI,
                                  EraVM::MemOperandKind Kind, unsigned Reg,
                                  const MCSymbol *Symbol, int Addend) {
  // TODO Refactor internal encoding used by the backend
  if (Reg == 0 && !Symbol)
    Addend *= -1;

  // First sub-operand: marker (whether this stack reference is absolute,
  // SP-relative or SP-modifying).
  MI.addOperand(createStackOperandMarker(Kind));

  // Second sub-operand: base register (marker immediate operand, if none).
  if (Reg == 0)
    MI.addOperand(MCOperand::createImm(0));
  else
    MI.addOperand(MCOperand::createReg(Reg));

  // Third sub-operand: Symbol+Addend (effectively an immediate integer operand
  // after all relocations).
  MI.addOperand(createSymbolWithAddend(Ctx, Symbol, Addend));
}

void EraVM::appendMCOperands(MCContext &Ctx, MCInst &MI, MemOperandKind Kind,
                             unsigned Reg, const MCSymbol *Symbol, int Addend) {
  if (Kind == OperandCode)
    appendCodeMCOperands(Ctx, MI, Reg, Symbol, Addend);
  else
    appendStackMCOperands(Ctx, MI, Kind, Reg, Symbol, Addend);
}
