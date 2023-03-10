//===-- SyncVMELFStreamer.cpp - SyncVM ELF Target Streamer Methods --------===//
//
// This file provides SyncVM specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "SyncVMMCTargetDesc.h"
#include "SyncVMTargetStreamer.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

namespace llvm {

class SyncVMTargetELFStreamer : public SyncVMTargetStreamer {
public:
  MCELFStreamer &getStreamer();
  SyncVMTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
};

// This part is for ELF object output.
SyncVMTargetELFStreamer::SyncVMTargetELFStreamer(MCStreamer &S,
                                                 const MCSubtargetInfo &STI)
    : SyncVMTargetStreamer(S) {}

class SyncVMTargetAsmStreamer : public SyncVMTargetStreamer {
public:
  SyncVMTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                          MCInstPrinter &InstPrinter, bool VerboseAsm);
};

SyncVMTargetAsmStreamer::SyncVMTargetAsmStreamer(MCStreamer &S,
                                                 formatted_raw_ostream &OS,
                                                 MCInstPrinter &InstPrinter,
                                                 bool VerboseAsm)
    : SyncVMTargetStreamer(S) {}

MCELFStreamer &SyncVMTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}
MCTargetStreamer *createSyncVMTargetAsmStreamer(MCStreamer &S,
                                                formatted_raw_ostream &OS,
                                                MCInstPrinter *InstPrint,
                                                bool isVerboseAsm) {
  return new SyncVMTargetAsmStreamer(S, OS, *InstPrint, isVerboseAsm);
}

MCTargetStreamer *createSyncVMNullTargetStreamer(MCStreamer &S) {
  return new SyncVMTargetStreamer(S);
}

MCTargetStreamer *createSyncVMObjectTargetStreamer(MCStreamer &S,
                                                   const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new SyncVMTargetELFStreamer(S, STI);
  return new SyncVMTargetStreamer(S);
}
} // namespace llvm
