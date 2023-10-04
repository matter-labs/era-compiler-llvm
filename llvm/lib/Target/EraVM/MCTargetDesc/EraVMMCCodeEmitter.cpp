//===-- EraVMMCCodeEmitter.cpp - Convert EraVM code to machine code -----===//
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
  /// TableGen'erated function for getting the binary encoding for an
  /// instruction.
  void getBinaryCodeForInstr(const MCInst &MI, SmallVectorImpl<MCFixup> &Fixups,
                             APInt &Inst, APInt &Scratch,
                             const MCSubtargetInfo &STI) const;

public:
  EraVMMCCodeEmitter(MCContext &ctx, MCInstrInfo const &MCII) {}

  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             const APInt &opcode,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
};

void EraVMMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {}

unsigned EraVMMCCodeEmitter::getMachineOpValue(
    const MCInst &MI, const MCOperand &MO, const APInt &opcode,
    SmallVectorImpl<MCFixup> &Fixups, const MCSubtargetInfo &STI) const {
  return 0;
}

MCCodeEmitter *createEraVMMCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx) {
  return new EraVMMCCodeEmitter(Ctx, MCII);
}

#include "EraVMGenMCCodeEmitter.inc"

} // end of namespace llvm
