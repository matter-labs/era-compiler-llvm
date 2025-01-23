//==----------- EVMDisassembler.cpp - Disassembler for EVM -*- C++ -------*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is part of the EVM Disassembler.
//
// It contains code to translate the data produced by the decoder into
// MCInsts.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMMCExpr.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "evm-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {

class EVMDisassembler final : public MCDisassembler {
  std::unique_ptr<const MCInstrInfo> MCII;

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;

public:
  EVMDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx,
                  std::unique_ptr<const MCInstrInfo> MCII)
      : MCDisassembler(STI, Ctx), MCII(std::move(MCII)) {}
};

} // end anonymous namespace

// Decode PUSHN instructions, where N > 8, because target independent part
// of the Decoder can handle only 64-bit types.
template <unsigned ImmByteWidth>
static DecodeStatus decodePUSH(MCInst &Inst, APInt &Insn, uint64_t Address,
                               const MCDisassembler *Decoder) {
  assert(ImmByteWidth > sizeof(uint64_t));
  assert(alignTo(Insn.getActiveBits(), 8) == (ImmByteWidth + 1) * 8);
  MCContext &Ctx = Decoder->getContext();
  auto *Str = new (Ctx) SmallString<80>();
  Insn.extractBits(ImmByteWidth * 8, 0).toStringUnsigned(*Str);
  Inst.addOperand(MCOperand::createExpr(EVMCImmMCExpr::create(*Str, Ctx)));
  return MCDisassembler::Success;
}

#include "EVMGenDisassemblerTables.inc"

static const uint8_t *getDecoderTable(unsigned Size) {
  switch (Size) {
  case 1:
    return static_cast<const uint8_t *>(DecoderTable8);
  case 2:
    return static_cast<const uint8_t *>(DecoderTable16);
  case 3:
    return static_cast<const uint8_t *>(DecoderTable24);
  case 4:
    return static_cast<const uint8_t *>(DecoderTable32);
  case 5:
    return static_cast<const uint8_t *>(DecoderTable40);
  case 6:
    return static_cast<const uint8_t *>(DecoderTable48);
  case 7:
    return static_cast<const uint8_t *>(DecoderTable56);
  case 8:
    return static_cast<const uint8_t *>(DecoderTable64);
  case 9:
    return static_cast<const uint8_t *>(DecoderTable72);
  case 10:
    return static_cast<const uint8_t *>(DecoderTable80);
  case 11:
    return static_cast<const uint8_t *>(DecoderTable88);
  case 12:
    return static_cast<const uint8_t *>(DecoderTable96);
  case 13:
    return static_cast<const uint8_t *>(DecoderTable104);
  case 14:
    return static_cast<const uint8_t *>(DecoderTable112);
  case 15:
    return static_cast<const uint8_t *>(DecoderTable120);
  case 16:
    return static_cast<const uint8_t *>(DecoderTable128);
  case 17:
    return static_cast<const uint8_t *>(DecoderTable136);
  case 18:
    return static_cast<const uint8_t *>(DecoderTable144);
  case 19:
    return static_cast<const uint8_t *>(DecoderTable152);
  case 20:
    return static_cast<const uint8_t *>(DecoderTable160);
  case 21:
    return static_cast<const uint8_t *>(DecoderTable168);
  case 22:
    return static_cast<const uint8_t *>(DecoderTable176);
  case 23:
    return static_cast<const uint8_t *>(DecoderTable184);
  case 24:
    return static_cast<const uint8_t *>(DecoderTable192);
  case 25:
    return static_cast<const uint8_t *>(DecoderTable200);
  case 26:
    return static_cast<const uint8_t *>(DecoderTable208);
  case 27:
    return static_cast<const uint8_t *>(DecoderTable216);
  case 28:
    return static_cast<const uint8_t *>(DecoderTable224);
  case 29:
    return static_cast<const uint8_t *>(DecoderTable232);
  case 30:
    return static_cast<const uint8_t *>(DecoderTable240);
  case 31:
    return static_cast<const uint8_t *>(DecoderTable248);
  case 32:
    return static_cast<const uint8_t *>(DecoderTable256);
  case 33:
    return static_cast<const uint8_t *>(DecoderTable264);
  default:
    llvm_unreachable("Instructions must be from 1 to 33-bytes");
  }
}

static MCDisassembler *createEVMDisassembler(const Target &T,
                                             const MCSubtargetInfo &STI,
                                             MCContext &Ctx) {
  std::unique_ptr<const MCInstrInfo> MCII(T.createMCInstrInfo());
  return new EVMDisassembler(STI, Ctx, std::move(MCII));
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMDisassembler() {
  // Register the disassembler.
  TargetRegistry::RegisterMCDisassembler(getTheEVMTarget(),
                                         createEVMDisassembler);
}

MCDisassembler::DecodeStatus
EVMDisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                ArrayRef<uint8_t> Bytes, uint64_t Address,
                                raw_ostream &CStream) const {
  Size = 0;
  if (Bytes.empty())
    return Fail;

  const size_t BytesNum = Bytes.size();
  for (Size = 1; Size <= 33; ++Size) {
    if (Size > BytesNum)
      break;

    APInt Insn(33 * 8, toHex(ArrayRef(Bytes.begin(), Bytes.begin() + Size)),
               16);
    DecodeStatus Result = decodeInstruction(getDecoderTable(Size), Instr, Insn,
                                            Address, this, STI);
    if (Result != Fail)
      return Result;
  }

  // Need to decrement after the loop.
  --Size;
  return Fail;
}
