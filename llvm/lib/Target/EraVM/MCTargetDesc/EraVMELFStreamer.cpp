//===-- EraVMELFStreamer.cpp - EraVM ELF Streamer ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "EraVMMCTargetDesc.h"
#include "EraVMTargetStreamer.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

namespace llvm {

class EraVMTargetELFStreamer : public EraVMTargetStreamer {
public:
  MCELFStreamer &getStreamer();
  EraVMTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
};

// This part is for ELF object output.
EraVMTargetELFStreamer::EraVMTargetELFStreamer(MCStreamer &S,
                                               const MCSubtargetInfo &STI)
    : EraVMTargetStreamer(S) {}

class EraVMTargetAsmStreamer : public EraVMTargetStreamer {
public:
  EraVMTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                         MCInstPrinter &InstPrinter, bool VerboseAsm);
};

EraVMTargetAsmStreamer::EraVMTargetAsmStreamer(MCStreamer &S,
                                               formatted_raw_ostream &OS,
                                               MCInstPrinter &InstPrinter,
                                               bool VerboseAsm)
    : EraVMTargetStreamer(S) {}

MCELFStreamer &EraVMTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}
MCTargetStreamer *createEraVMTargetAsmStreamer(MCStreamer &S,
                                               formatted_raw_ostream &OS,
                                               MCInstPrinter *InstPrint,
                                               bool isVerboseAsm) {
  return new EraVMTargetAsmStreamer(S, OS, *InstPrint, isVerboseAsm);
}

MCTargetStreamer *createEraVMNullTargetStreamer(MCStreamer &S) {
  return new EraVMTargetStreamer(S);
}

MCTargetStreamer *createEraVMObjectTargetStreamer(MCStreamer &S,
                                                  const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new EraVMTargetELFStreamer(S, STI);
  return new EraVMTargetStreamer(S);
}
} // namespace llvm
