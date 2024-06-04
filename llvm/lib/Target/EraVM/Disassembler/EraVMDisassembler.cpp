//===-- EraVMDisassembler.cpp - Disassembler for EraVM ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
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
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

#include <memory>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "eravm-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {
class EraVMDisassembler : public MCDisassembler {
  std::unique_ptr<MCInstrInfo> MCII;

public:
  EraVMDisassembler(const Target &T, const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx), MCII(T.createMCInstrInfo()) {}

  DecodeStatus getInstruction(MCInst &MI, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};

Register decodeGR256(const MCDisassembler *D, unsigned EncodedReg) {
  const auto *MCRI = D->getContext().getRegisterInfo();
  const auto &RC = MCRI->getRegClass(EraVM::GR256RegClassID);
  assert(RC.getNumRegs() == 16 && "Unexpected change to register class");
  return RC.getRegister(EncodedReg);
}

int64_t decodeCC(unsigned EncodedCC) {
  // EraVMCC::CondCodes are defined so that enumerator values are actual
  // predicate's binary encodings.
  assert(EncodedCC < 8 && "Too wide field to decode");
  return EncodedCC & 0x7;
}

DecodeStatus DecodeGR256RegisterClass(MCInst &Inst, unsigned EncodedReg,
                                      uint64_t, const MCDisassembler *D) {
  Inst.addOperand(MCOperand::createReg(decodeGR256(D, EncodedReg)));
  return DecodeStatus::Success;
}

DecodeStatus DecodeCCOperand(MCInst &Inst, unsigned EncodedCC, uint64_t,
                             const MCDisassembler *) {
  Inst.addOperand(MCOperand::createImm(decodeCC(EncodedCC)));
  return DecodeStatus::Success;
}

DecodeStatus DecodeStackOperand(MCInst &Inst, unsigned EncodedStackOp, uint64_t,
                                const MCDisassembler *D) {
  unsigned EncodedReg = EncodedStackOp & 0xf;
  unsigned EncodedAddend = (EncodedStackOp >> 4) & 0xffff;
  Inst.addOperand(MCOperand::createImm(0)); // Will be replaced later
  Inst.addOperand(MCOperand::createReg(decodeGR256(D, EncodedReg)));
  Inst.addOperand(MCOperand::createImm(EncodedAddend));
  return DecodeStatus::Success;
}

DecodeStatus DecodeCodeOperand(MCInst &Inst, unsigned EncodedStackOp, uint64_t,
                               const MCDisassembler *D) {
  unsigned EncodedReg = EncodedStackOp & 0xf;
  unsigned EncodedAddend = (EncodedStackOp >> 4) & 0xffff;
  Inst.addOperand(MCOperand::createReg(decodeGR256(D, EncodedReg)));
  Inst.addOperand(MCOperand::createImm(EncodedAddend));
  return DecodeStatus::Success;
}

} // end anonymous namespace

#include "EraVMGenDisassemblerTables.inc"

static MCDisassembler *createEraVMDisassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new EraVMDisassembler(T, STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheEraVMTarget(),
                                         createEraVMDisassembler);
}

static std::optional<MCOperand> createStackMarker(int Mode) {
  switch (Mode) {
  case EraVM::ModeSpMod:
    return MCOperand::createReg(EraVM::R0);
  case EraVM::ModeSpRel:
    return MCOperand::createReg(EraVM::SP);
  case EraVM::ModeStackAbs:
    return MCOperand::createImm(0);
  default:
    return std::nullopt;
  }
}

DecodeStatus EraVMDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CStream) const {
  // The instruction is always 8 bytes.
  Size = 0;
  if (Bytes.size() < 8)
    return Fail;
  Size = 8;

  uint64_t Insn = support::endian::read64be(Bytes.begin());
  const uint64_t OldOpcode = Insn & EraVM::EncodedOpcodeMask;
  uint64_t NewOpcode = OldOpcode;
  Insn &= ~EraVM::EncodedOpcodeMask;

  EraVM::EncodedOperandMode SrcMode = EraVM::ModeNotApplicable;
  EraVM::EncodedOperandMode DstMode = EraVM::ModeNotApplicable;
  const EraVMOpcodeInfo *Info =
      EraVM::analyzeEncodedOpcode(OldOpcode, SrcMode, DstMode);
  if (!Info)
    return Fail;

  std::optional<MCOperand> StackSrcMarker = createStackMarker(SrcMode);
  std::optional<MCOperand> StackDstMarker = createStackMarker(DstMode);
  if (StackSrcMarker)
    NewOpcode += (EraVM::ModeStackAbs - SrcMode) * Info->SrcMultiplier;
  if (StackDstMarker)
    NewOpcode += (EraVM::ModeStackAbs - DstMode) * Info->DstMultiplier;

  Insn |= NewOpcode;

  DecodeStatus Result =
      decodeInstruction(DecoderTableEraVM64, MI, Insn, Address, this, STI);
  if (Result == DecodeStatus::Success) {
    const MCInstrDesc &Desc = MCII->get(MI.getOpcode());
    // TODO Make use of appendMCOperands(...).
    if (StackSrcMarker)
      MI.getOperand(Info->getMCOperandIndexOfStackSrc(Desc)) = *StackSrcMarker;
    if (StackDstMarker)
      MI.getOperand(Info->getMCOperandIndexOfStackDst(Desc)) = *StackDstMarker;
  }
  return Result;
}
