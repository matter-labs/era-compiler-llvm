//===-- EraVMMCCodeEmitter.cpp - EraVM Code Emitter -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVMMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "MCTargetDesc/EraVMFixupKinds.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "mccodeemitter"

namespace llvm {

class EraVMMCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;

  /// TableGen'erated function for getting the binary encoding for an
  /// instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

public:
  EraVMMCCodeEmitter(MCContext &Ctx, MCInstrInfo const &MCII) : Ctx(Ctx) {}

  uint64_t getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  uint64_t getMemOpValue(const MCInst &MI, unsigned Idx,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  uint64_t getStackOpValue(const MCInst &MI, unsigned Idx,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const;

  uint64_t getCCOpValue(const MCInst &MI, unsigned Idx,
                        SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
};

void EraVMMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  uint64_t EncodedInstr = getBinaryCodeForInstr(MI, Fixups, STI);
  support::endian::write(OS, EncodedInstr, support::big);
}

uint64_t
EraVMMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                      SmallVectorImpl<MCFixup> &Fixups,
                                      const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return MO.getImm();

  llvm_unreachable("Unexpected generic operand type");
}

uint64_t EraVMMCCodeEmitter::getStackOpValue(const MCInst &MI, unsigned Idx,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  llvm_unreachable("Unable to encode stack operand!");
}

uint64_t EraVMMCCodeEmitter::getMemOpValue(const MCInst &MI, unsigned Idx,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  llvm_unreachable("Unable to encode mem operand!");
}

uint64_t EraVMMCCodeEmitter::getCCOpValue(const MCInst &MI, unsigned Idx,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  uint64_t CC = MI.getOperand(Idx).getImm();
  assert(CC < 8 && "Invalid condition code");
  assert(CC != (uint64_t)EraVMCC::COND_INVALID && "Cannot encode COND_INVALID");
  // EraVMCC::CondCodes are defined so that enumerator values are actual
  // predicate's binary encodings.
  return CC & 0x7;
}

MCCodeEmitter *createEraVMMCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx) {
  return new EraVMMCCodeEmitter(Ctx, MCII);
}

#include "EraVMGenMCCodeEmitter.inc"

} // end of namespace llvm
