//===- EraVMDisassembler.cpp - Disassembler for EraVM ---------------------===//
//
// This file implements the EraVMDisassembler class.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "TargetInfo/EraVMTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class EraVMDisassembler : public MCDisassembler {

public:
  EraVMDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &MI, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createEraVMDisassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new EraVMDisassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheEraVMTarget(),
                                         createEraVMDisassembler);
}

DecodeStatus EraVMDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CStream) const {
  MI.setOpcode(EraVM::NOPSP);

  Size = 32;

  return DecodeStatus::Success;
}
