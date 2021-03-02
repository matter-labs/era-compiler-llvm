//===-- SyncVMMCCodeEmitter.cpp - Convert SyncVM code to machine code -----===//
//
// This file implements the SyncVMMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "MCTargetDesc/SyncVMFixupKinds.h"

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

class SyncVMMCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;
  MCInstrInfo const &MCII;

  // Offset keeps track of current word number being emitted
  // inside a particular instruction.
  mutable unsigned Offset;

  /// TableGen'erated function for getting the binary encoding for an
  /// instruction.
  void getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 APInt &Inst,
                                 APInt &Scratch,
                                 const MCSubtargetInfo &STI) const;

public:
  SyncVMMCCodeEmitter(MCContext &ctx, MCInstrInfo const &MCII)
      : Ctx(ctx), MCII(MCII) {}

  unsigned getMachineOpValue(const MCInst &MI,
                             const MCOperand &MO,
                             const APInt opcode,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
};

void SyncVMMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {

}

unsigned SyncVMMCCodeEmitter::getMachineOpValue(const MCInst &MI,
                                                const MCOperand &MO,
                                                const APInt opcode,
                                                SmallVectorImpl<MCFixup> &Fixups,
                                                const MCSubtargetInfo &STI) const {
  return 0;
}

MCCodeEmitter *createSyncVMMCCodeEmitter(const MCInstrInfo &MCII,
                                         const MCRegisterInfo &MRI,
                                         MCContext &Ctx) {
  return new SyncVMMCCodeEmitter(Ctx, MCII);
}

#include "SyncVMGenMCCodeEmitter.inc"

} // end of namespace llvm
