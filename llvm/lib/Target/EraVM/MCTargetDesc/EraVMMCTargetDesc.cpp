//===-- EraVMMCTargetDesc.cpp - EraVM Target Descriptions -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides EraVM specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "EraVMMCTargetDesc.h"
#include "EraVMInstPrinter.h"
#include "EraVMMCAsmInfo.h"
#include "TargetInfo/EraVMTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "EraVMGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "EraVMGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "EraVMGenSubtargetInfo.inc"

static MCInstrInfo *createEraVMMCInstrInfo() {
  auto *X = new MCInstrInfo();
  InitEraVMMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createEraVMMCRegisterInfo(const Triple &TT) {
  auto *X = new MCRegisterInfo();
  InitEraVMMCRegisterInfo(X, EraVM::PC);
  return X;
}

static MCInstPrinter *createEraVMMCInstPrinter(const Triple &T,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new EraVMInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCSubtargetInfo *
createEraVMMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  return createEraVMMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCAsmInfo *createARMMCAsmInfo(const MCRegisterInfo &MRI,
                                     const Triple &TheTriple,
                                     const MCTargetOptions &Options) {
  MCAsmInfo *MAI{};
  MAI = new EraVMMCAsmInfo(TheTriple);
  return MAI;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMTargetMC() {
  Target &T = getTheEraVMTarget();

  RegisterMCAsmInfoFn X(T, createARMMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createEraVMMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createEraVMMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createEraVMMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createEraVMMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createEraVMMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createEraVMMCAsmBackend);
  TargetRegistry::RegisterObjectTargetStreamer(T,
                                               createEraVMObjectTargetStreamer);
  TargetRegistry::RegisterAsmTargetStreamer(T, createEraVMTargetAsmStreamer);
}

#define GET_EraVMOpcodesList_IMPL
#include "EraVMGenSearchableTables.inc"

#define GET_INSTRINFO_LOGICAL_OPERAND_SIZE_MAP
#include "EraVMGenInstrInfo.inc"

unsigned EraVMOpcodeInfo::getMCOperandIndexForUseIndex(const MCInstrDesc &Desc,
                                                       unsigned UseIndex) {
  return EraVM::getLogicalOperandIdx(Desc.getOpcode(),
                                     Desc.getNumDefs() + UseIndex);
}

const EraVMOpcodeInfo *llvm::EraVM::findOpcodeInfo(unsigned Opcode) {
  auto Table = ArrayRef(EraVMOpcodesList);
  const auto *It =
      std::upper_bound(Table.begin(), Table.end(), Opcode,
                       [](unsigned LHS, const EraVMOpcodeInfo &RHS) {
                         return LHS < RHS.BaseOpcode;
                       });
  assert(It != Table.begin() && "Was \"<invalid>\" sentinel removed?");
  --It;

  return It;
}

const EraVMOpcodeInfo *
EraVM::analyzeEncodedOpcode(unsigned EncodedOpcode, EncodedOperandMode &SrcMode,
                            EncodedOperandMode &DstMode) {
  const EraVMOpcodeInfo *Info = findOpcodeInfo(EncodedOpcode);
  int OpcodeDelta = EncodedOpcode - Info->BaseOpcode;

  SrcMode = ModeNotApplicable;
  DstMode = ModeNotApplicable;

  if (Info->SrcMultiplier)
    SrcMode =
        EncodedOperandMode((OpcodeDelta / Info->SrcMultiplier) % NumSrcModes);
  if (Info->DstMultiplier)
    DstMode =
        EncodedOperandMode((OpcodeDelta / Info->DstMultiplier) % NumDstModes);

  return Info;
}
