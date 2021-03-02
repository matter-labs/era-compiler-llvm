//===-- SyncVMDisassembler.cpp - Disassembler for SyncVM ------------------===//
//
// This file implements the SyncVMDisassembler class.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "TargetInfo/SyncVMTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCFixedLenDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class SyncVMDisassembler : public MCDisassembler {

public:
  SyncVMDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &MI, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createSyncVMDisassembler(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                MCContext &Ctx) {
  return new SyncVMDisassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheSyncVMTarget(),
                                         createSyncVMDisassembler);
}

DecodeStatus SyncVMDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CStream) const {
  MI.setOpcode(SyncVM::ADD);

  Size = 1;

  return DecodeStatus::Success;
}
