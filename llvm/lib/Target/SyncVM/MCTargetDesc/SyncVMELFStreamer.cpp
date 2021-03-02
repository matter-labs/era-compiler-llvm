//===-- SyncVMELFStreamer.cpp - SyncVM ELF Target Streamer Methods --------===//
//
// This file provides SyncVM specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "SyncVMMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

namespace llvm {

class SyncVMTargetELFStreamer : public MCTargetStreamer {
public:
  MCELFStreamer &getStreamer();
  SyncVMTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
};

// This part is for ELF object output.
SyncVMTargetELFStreamer::SyncVMTargetELFStreamer(MCStreamer &S,
                                                 const MCSubtargetInfo &STI)
    : MCTargetStreamer(S) {

}

MCELFStreamer &SyncVMTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}

MCTargetStreamer *
createSyncVMObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new SyncVMTargetELFStreamer(S, STI);
  return nullptr;
}

} // namespace llvm
