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
  const MCInstrInfo &MCII;

  /// TableGen'erated function for getting the binary encoding for an
  /// instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

public:
  EraVMMCCodeEmitter(MCContext &Ctx, MCInstrInfo const &MCII)
      : Ctx(Ctx), MCII(MCII) {}

  uint64_t getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  uint64_t getMemOpValue(const MCInst &MI, unsigned Idx,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  template <bool IsSrc>
  uint64_t getStackOpValue(const MCInst &MI, unsigned Idx,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const;

  uint64_t getJumpTargetValue(const MCInst &MI, unsigned Idx,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;

  uint64_t getCCOpValue(const MCInst &MI, unsigned Idx,
                        SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI) const;

  // Update the encoded instruction to account for three different possible
  // variants of stack operands by updating its 11-bit "opcode" field (not to
  // be confused with MI.Opcode used internally by the backend).
  //
  // While EraVM has 6 modes of in_any and 4 modes of out_any operands (with 3
  // different stack modes on each "side") which are encoded into 11-bit opcode
  // field of 64-bit instruction, the LLVM backend for EraVM uses the same
  // instruction (corresponding to MI.Opcode) for all kinds of stack operands
  // of the particular direction (input/output), so the encoded opcode field
  // has to be adjusted based on MachineOperands corresponding to stack operands
  // of the instruction.
  uint64_t adjustForStackOperands(const MCInst &MI, uint64_t EncodedInstr,
                                  const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  uint64_t getRegWithAddend(const MCInst &MI, unsigned BaseReg,
                            const MCSymbol *Symbol, int Addend, bool IsSrc,
                            SmallVectorImpl<MCFixup> &Fixups) const;
};

static int getStackModeEncoding(const MCInst &MI, unsigned Idx, bool IsSrc) {
  EraVM::MemOperandKind Kind = EraVM::getStackOperandKind(MI, Idx, IsSrc);
  return EraVM::getModeEncodingForOperandKind(Kind);
}

uint64_t EraVMMCCodeEmitter::adjustForStackOperands(
    const MCInst &MI, uint64_t EncodedInstr, const MCSubtargetInfo &STI) const {
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());

  EraVM::EncodedOperandMode OldSrcMode = EraVM::ModeNotApplicable;
  EraVM::EncodedOperandMode OldDstMode = EraVM::ModeNotApplicable;
  // To not reason about possible signed-to-unsigned widening in the code below,
  // let's just use int type for EncodedOpcode as it essentially holds an
  // 11-bit value anyway.
  int EncodedOpcode = EncodedInstr & EraVM::EncodedOpcodeMask;
  const EraVMOpcodeInfo *Info =
      EraVM::analyzeEncodedOpcode(EncodedOpcode, OldSrcMode, OldDstMode);
  assert(Info && "Incorrect EncodedOpcode produced by the encoder");

  if (OldSrcMode == EraVM::ModeStackAbs) {
    unsigned SrcIdx = Info->getMCOperandIndexOfStackSrc(Desc);
    int NewSrcMode = getStackModeEncoding(MI, SrcIdx, /*IsSrc=*/true);
    EncodedOpcode += (NewSrcMode - (int)OldSrcMode) * (int)Info->SrcMultiplier;
  }
  if (OldDstMode == EraVM::ModeStackAbs) {
    unsigned DstIdx = Info->getMCOperandIndexOfStackDst(Desc);
    int NewDstMode = getStackModeEncoding(MI, DstIdx, /*IsSrc=*/false);
    EncodedOpcode += (NewDstMode - (int)OldDstMode) * (int)Info->DstMultiplier;
  }

  EncodedInstr &= ~EraVM::EncodedOpcodeMask;
  EncodedInstr |= EncodedOpcode;

  return EncodedInstr;
}

void EraVMMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  MCInst ExpandedMI(MI);

  if (ExpandedMI.getOpcode() == EraVM::NEAR_CALL_default_unwind) {
    ExpandedMI.setOpcode(EraVM::NEAR_CALL);
    MCSymbol *Sym = Ctx.getOrCreateSymbol("DEFAULT_UNWIND_DEST");
    const MCExpr *Expr = MCSymbolRefExpr::create(Sym, Ctx);
    auto *OptionalCC = std::next(ExpandedMI.begin(), 2);
    ExpandedMI.insert(OptionalCC, MCOperand::createExpr(Expr));
  }

  uint64_t EncodedInstr = getBinaryCodeForInstr(ExpandedMI, Fixups, STI);
  EncodedInstr = adjustForStackOperands(ExpandedMI, EncodedInstr, STI);
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

uint64_t EraVMMCCodeEmitter::getRegWithAddend(
    const MCInst &MI, unsigned BaseReg, const MCSymbol *Symbol, int Addend,
    bool IsSrc, SmallVectorImpl<MCFixup> &Fixups) const {
  if (Symbol) {
    unsigned Offset = IsSrc ? 2 : 0;
    const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, Ctx);
    auto FK = static_cast<MCFixupKind>(EraVM::fixup_16_scale_32);
    Fixups.push_back(MCFixup::create(Offset, Expr, FK, MI.getLoc()));
  }

  uint64_t Result = 0;
  Result |= Ctx.getRegisterInfo()->getEncodingValue(BaseReg);
  Result |= Addend << 4;

  return Result;
}

template <bool IsSrc>
uint64_t EraVMMCCodeEmitter::getStackOpValue(const MCInst &MI, unsigned Idx,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  unsigned BaseReg = 0;
  const MCSymbol *Symbol = nullptr;
  int Addend = 0;
  EraVM::MemOperandKind Kind = EraVM::OperandInvalid;
  EraVM::analyzeMCOperandsStack(MI, Idx, IsSrc, BaseReg, Kind, Symbol, Addend);

  return getRegWithAddend(MI, BaseReg, Symbol, Addend, IsSrc, Fixups);
}

uint64_t EraVMMCCodeEmitter::getMemOpValue(const MCInst &MI, unsigned Idx,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  unsigned BaseReg = 0;
  const MCSymbol *Symbol = nullptr;
  int Addend = 0;
  EraVM::analyzeMCOperandsCode(MI, Idx, BaseReg, Symbol, Addend);

  return getRegWithAddend(MI, BaseReg, Symbol, Addend, /*IsSrc=*/true, Fixups);
}

uint64_t
EraVMMCCodeEmitter::getJumpTargetValue(const MCInst &MI, unsigned Idx,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  const MCOperand &Op = MI.getOperand(Idx);
  assert(Op.isExpr());

  unsigned Offset = 2;
  if (MI.getOpcode() == EraVM::NEAR_CALL && Idx == 2) {
    // NEAR_CALL has two jmptarget operands and this is the second.
    Offset = 0;
  }
  auto FK = static_cast<MCFixupKind>(EraVM::fixup_16_scale_8);
  Fixups.push_back(MCFixup::create(Offset, Op.getExpr(), FK, MI.getLoc()));
  return 0;
}

uint64_t EraVMMCCodeEmitter::getCCOpValue(const MCInst &MI, unsigned Idx,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  uint64_t CC = MI.getOperand(Idx).getImm();
  // When comes to encoding, the COND_OF equals COND_LT;
  if (CC == EraVMCC::COND_OF)
    CC = EraVMCC::COND_LT;
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
