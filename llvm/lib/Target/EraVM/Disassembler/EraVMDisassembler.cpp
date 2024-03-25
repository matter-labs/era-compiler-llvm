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
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {
class EraVMDisassembler : public MCDisassembler {

public:
  EraVMDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

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
  const unsigned EncodingForIfGTOrLT = 7;
  if (EncodedCC == EncodingForIfGTOrLT)
    return EraVMCC::COND_INVALID;

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
  // TODO Implement decoding stack operands
  return DecodeStatus::Fail;
}

DecodeStatus DecodeCodeOperand(MCInst &Inst, unsigned EncodedStackOp, uint64_t,
                               const MCDisassembler *D) {
  // TODO Implement decoding code operands
  return DecodeStatus::Fail;
}

} // end anonymous namespace

#include "EraVMGenDisassemblerTables.inc"

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
  // The instruction is always 8 bytes.
  Size = 0;
  if (Bytes.size() < 8)
    return Fail;
  Size = 8;

  uint64_t Insn = support::endian::read64be(Bytes.begin());

  return decodeInstruction(DecoderTableEraVM64, MI, Insn, Address, this, STI);
}
