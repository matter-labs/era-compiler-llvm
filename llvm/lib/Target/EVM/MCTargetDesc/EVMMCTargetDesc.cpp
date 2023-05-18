//===-------- EVMMCTargetDesc.cpp - EVM Target Descriptions ---------------===//
//
// This file provides EVM specific target descriptions.
//
//===----------------------------------------------------------------------===//
//
#include "EVMMCTargetDesc.h"
#include "EVMInstPrinter.h"
#include "EVMMCAsmInfo.h"
#include "EVMTargetStreamer.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "EVMGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "EVMGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "EVMGenRegisterInfo.inc"

static MCInstrInfo *createEVMMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitEVMMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createEVMMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitEVMMCRegisterInfo(X, 0);
  return X;
}

static MCInstPrinter *createEVMMCInstPrinter(const Triple &T,
                                             unsigned SyntaxVariant,
                                             const MCAsmInfo &MAI,
                                             const MCInstrInfo &MII,
                                             const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new EVMInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCAsmInfo *createEVMMCAsmInfo(const MCRegisterInfo & /*MRI*/,
                                     const Triple &TT,
                                     const MCTargetOptions & /*Options*/) {
  return new EVMMCAsmInfo(TT);
}

static MCSubtargetInfo *createEVMMCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU, StringRef FS) {
  return createEVMMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCTargetStreamer *
createEVMObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo & /*STI*/) {
  return new EVMTargetObjStreamer(S);
}

static MCTargetStreamer *
createEVMAsmTargetStreamer(MCStreamer &S, formatted_raw_ostream & /*OS*/,
                           MCInstPrinter * /*InstPrint*/,
                           bool /*isVerboseAsm*/) {
  return new EVMTargetAsmStreamer(S);
}

static MCTargetStreamer *createEVMNullTargetStreamer(MCStreamer &S) {
  return new EVMTargetStreamer(S);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMTargetMC() {
  Target &T = getTheEVMTarget();

  // Register the MC asm info.
  RegisterMCAsmInfoFn X(T, createEVMMCAsmInfo);

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(T, createEVMMCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(T, createEVMMCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(T, createEVMMCSubtargetInfo);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(T, createEVMMCInstPrinter);

  // Register the MC code emitter.
  TargetRegistry::RegisterMCCodeEmitter(T, createEVMMCCodeEmitter);

  // Register the ASM Backend.
  TargetRegistry::RegisterMCAsmBackend(T, createEVMMCAsmBackend);

  // Register the object target streamer.
  TargetRegistry::RegisterObjectTargetStreamer(T,
                                               createEVMObjectTargetStreamer);

  // Register the asm target streamer.
  TargetRegistry::RegisterAsmTargetStreamer(T, createEVMAsmTargetStreamer);

  // Register the null target streamer.
  TargetRegistry::RegisterNullTargetStreamer(T, createEVMNullTargetStreamer);
}
