//===-- SyncVMMCTargetDesc.cpp - SyncVM Target Descriptions ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides SyncVM specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "SyncVMMCTargetDesc.h"
#include "SyncVMInstPrinter.h"
#include "SyncVMMCAsmInfo.h"
#include "TargetInfo/SyncVMTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "SyncVMGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "SyncVMGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "SyncVMGenSubtargetInfo.inc"

static MCInstrInfo *createSyncVMMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitSyncVMMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createSyncVMMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitSyncVMMCRegisterInfo(X, SyncVM::PC);
  return X;
}

static MCInstPrinter *createSyncVMMCInstPrinter(const Triple &T,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new SyncVMInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCSubtargetInfo *
createSyncVMMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  return createSyncVMMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCAsmInfo *createARMMCAsmInfo(const MCRegisterInfo &MRI,
                                     const Triple &TheTriple,
                                     const MCTargetOptions &Options) {
  MCAsmInfo *MAI;
  MAI = new SyncVMMCAsmInfo(TheTriple);
  return MAI;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMTargetMC() {
  Target &T = getTheSyncVMTarget();

  RegisterMCAsmInfoFn X(T, createARMMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createSyncVMMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createSyncVMMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createSyncVMMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createSyncVMMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createSyncVMMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createSyncVMMCAsmBackend);
  TargetRegistry::RegisterObjectTargetStreamer(
      T, createSyncVMObjectTargetStreamer);
  TargetRegistry::RegisterAsmTargetStreamer(T, createSyncVMTargetAsmStreamer);
}
