//===- SyncVMTargetStreamer.cpp - SyncVMTargetStreamer class --*- C++ -*---===//
//
// This file implements the SyncVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SyncVMTargetStreamer.h"
#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "llvm/MC/ConstantPools.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/TargetParser.h"

using namespace llvm;

//
// SyncVMTargetStreamer Implemenation
//

SyncVMTargetStreamer::SyncVMTargetStreamer(MCStreamer &S)
    : MCTargetStreamer(S), ConstantPools(new AssemblerConstantPools()) {}

SyncVMTargetStreamer::~SyncVMTargetStreamer() = default;

void SyncVMTargetStreamer::emitGlobalConst(APInt Value) {
  if (Value.getBitWidth() < 256) {
    // align by 256 bit
    Value = Value.sext(256);
  }
  SmallString<86> Str;
  raw_svector_ostream OS(Str);
  OS << "\t.cell " << Value;
  Streamer.emitRawText(OS.str());
}
