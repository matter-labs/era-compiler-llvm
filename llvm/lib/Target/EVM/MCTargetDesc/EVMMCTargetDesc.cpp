//===----- EVMMCTargetDesc.cpp - EVM Target Descriptions -*- C++ -*--------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
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
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/KECCAK.h"
#include "llvm/Support/Regex.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "EVMGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "EVMGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "EVMGenRegisterInfo.inc"

static MCInstrInfo *createEVMMCInstrInfo() {
  auto *X = new MCInstrInfo();
  InitEVMMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createEVMMCRegisterInfo(const Triple &TT) {
  auto *X = new MCRegisterInfo();
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
  const RegisterMCAsmInfoFn X(T, createEVMMCAsmInfo);

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

// Returs a string of the following format:
//   '__$KECCAK256(SymName)$__'
std::string EVM::getLinkerSymbolHash(StringRef SymName) {
  std::array<uint8_t, 32> Hash = KECCAK::KECCAK_256(SymName);
  SmallString<72> HexHash;
  toHex(Hash, /*LowerCase*/ true, HexHash);
  return (Twine("__$") + HexHash + "$__").str();
}

// Returns concatenation of the \p Name with the \p SubIdx.
std::string EVM::getSymbolIndexedName(StringRef Name, unsigned SubIdx) {
  return (Twine(Name) + std::to_string(SubIdx)).str();
}

// Returns concatenation of '.symbol_name' with the \p Name.
std::string EVM::getSymbolSectionName(StringRef Name) {
  return (Twine(".symbol_name") + Name).str();
}

// Strips index from the \p Name.
std::string EVM::getNonIndexedSymbolName(StringRef Name) {
  Regex suffixRegex(R"(.*[0-4]$)");
  if (!suffixRegex.match(Name))
    report_fatal_error("Unexpected indexed symbol name");

  return Name.drop_back().str();
}

std::string EVM::getLinkerSymbolName(StringRef Name) {
  return (Twine("__linker_symbol") + Name).str();
}

std::string EVM::getDataSizeSymbol(StringRef Name) {
  return (Twine("__datasize") + Name).str();
}

bool EVM::isDataSizeSymbolName(StringRef SymbolName) {
  return SymbolName.find("__datasize") == 0;
}

std::string EVM::extractDataSizeName(StringRef SymbolName) {
  if (!SymbolName.consume_front("__datasize"))
    report_fatal_error("Unexpected datasize symbol format");

  return SymbolName.str();
}

std::string EVM::getDataOffsetSymbol(StringRef Name) {
  return (Twine("__dataoffset") + Name).str();
}

bool EVM::isDataOffsetSymbolName(StringRef Name) {
  return Name.find("__dataoffset") == 0;
}

std::string EVM::extractDataOffseteName(StringRef SymbolName) {
  if (!SymbolName.consume_front("__dataoffset"))
    report_fatal_error("Unexpected dataoffset symbol format");

  return SymbolName.str();
}

std::string EVM::getLoadImmutableSymbol(StringRef Name, unsigned Idx) {
  return (Twine("__load_immutable__") + Name + "." + std::to_string(Idx)).str();
}

bool EVM::isLoadImmutableSymbolName(StringRef Name) {
  return Name.find("__load_immutable__") == 0;
}

// extract immutable ID from the load immutable symbol name.
// '__load_immutable__ID.N' -> 'ID'.
std::string EVM::getImmutableId(StringRef Name) {
  SmallVector<StringRef, 2> Matches;
  Regex suffixRegex(R"(\.[0-9]+$)");
  if (!suffixRegex.match(Name, &Matches))
    report_fatal_error("No immutable symbol index");
  assert(Matches.size() == 1);

  // Strip suffix
  Name.consume_back(Matches[0]);

  // Strip prefix
  if (!Name.consume_front("__load_immutable__"))
    report_fatal_error("Unexpected load immutable symbol format");

  return Name.str();
}
