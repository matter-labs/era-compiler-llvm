//===-- EVMMCCodeEmitter.cpp - Convert EVM code to machine code -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVMMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMFixupKinds.h"
#include "MCTargetDesc/EVMMCExpr.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

namespace {
class EVMMCCodeEmitter final : public MCCodeEmitter {
  const MCInstrInfo &MCII;

  // Implementation generated by tablegen.
  void getBinaryCodeForInstr(const MCInst &MI, SmallVectorImpl<MCFixup> &Fixups,
                             APInt &Inst, APInt &Scratch,
                             const MCSubtargetInfo &STI) const;

  // Return binary encoding of operand.
  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO, APInt &Op,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

public:
  EVMMCCodeEmitter(MCContext &Ctx, MCInstrInfo const &MCII) : MCII(MCII) {}
};

EVM::Fixups getFixupForOpc(unsigned Opcode, MCSymbolRefExpr::VariantKind Kind) {
  if (Kind == MCSymbolRefExpr::VariantKind::VK_EVM_DATA)
    return EVM::fixup_Data_i32;

  switch (Opcode) {
  default:
    llvm_unreachable("Unexpected MI for the SymbolRef MO");
  case EVM::PUSH4_S:
    return EVM::fixup_SecRel_i32;
  case EVM::PUSH3_S:
    return EVM::fixup_SecRel_i24;
  case EVM::PUSH2_S:
    return EVM::fixup_SecRel_i16;
  case EVM::PUSH1_S:
    return EVM::fixup_SecRel_i8;
  }
}

} // end anonymous namespace

void EVMMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  APInt Inst, Scratch;
  getBinaryCodeForInstr(MI, Fixups, Inst, Scratch, STI);

  constexpr unsigned ByteSize = sizeof(std::byte) * 8;
  unsigned InstSize = MCII.get(MI.getOpcode()).getSize();

#ifndef NDEBUG
  const unsigned ActBitWidth =
      MI.getOpcode() != EVM::STOP_S ? Inst.getActiveBits() : 1;
  assert(ActBitWidth > 0 && ActBitWidth <= InstSize * ByteSize);
#endif // NDEBUG

  unsigned BitNum = InstSize * ByteSize;
  while (BitNum > 0) {
    std::byte ByteVal{0};
    for (unsigned I = 0; I < ByteSize; ++I, --BitNum)
      ByteVal |= std::byte(Inst[BitNum - 1]) << (ByteSize - I - 1);

    support::endian::write<std::byte>(OS, ByteVal, support::big);
  }
}

unsigned EVMMCCodeEmitter::getMachineOpValue(const MCInst &MI,
                                             const MCOperand &MO, APInt &Op,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  if (MO.isImm()) {
    Op = MO.getImm();
  } else if (MO.isExpr()) {
    auto Kind = MO.getExpr()->getKind();
    if (Kind == MCExpr::ExprKind::Target) {
      const auto *CImmExp = cast<EVMCImmMCExpr>(MO.getExpr());
      Op = APInt(Op.getBitWidth(), CImmExp->getString(), /*radix=*/10);
    } else if (Kind == MCExpr::ExprKind::SymbolRef) {
      const auto *RefExpr = cast<MCSymbolRefExpr>(MO.getExpr());
      MCSymbolRefExpr::VariantKind Kind = RefExpr->getKind();
      EVM::Fixups Fixup = getFixupForOpc(MI.getOpcode(), Kind);
      // The byte index of start of the relocation is always 1, as
      // we need to skip the instruction opcode which is always one byte.
      Fixups.push_back(
          MCFixup::create(1, MO.getExpr(), MCFixupKind(Fixup), MI.getLoc()));
    }
  } else {
    llvm_unreachable("Unexpected MC operand type");
  }

  return 0;
}

MCCodeEmitter *llvm::createEVMMCCodeEmitter(const MCInstrInfo &MCII,
                                            MCContext &Ctx) {
  return new EVMMCCodeEmitter(Ctx, MCII);
}

#include "EVMGenMCCodeEmitter.inc"
