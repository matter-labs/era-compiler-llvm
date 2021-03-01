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
  MCAssembler &MCA = getStreamer().getAssembler();
  unsigned EFlags = MCA.getELFHeaderEFlags();
  MCA.setELFHeaderEFlags(EFlags);

  // Emit build attributes section according to
  // SyncVM EABI (slaa534.pdf, part 13).
  MCSection *AttributeSection = getStreamer().getContext().getELFSection(
      ".SyncVM.attributes", ELF::SHT_SYNCVM_ATTRIBUTES, 0);
  Streamer.SwitchSection(AttributeSection);

  // Format version.
  Streamer.emitInt8(0x41);
  // Subsection length.
  Streamer.emitInt32(22);
  // Vendor name string, zero-terminated.
  Streamer.emitBytes("mspabi");
  Streamer.emitInt8(0);

  // Attribute vector scope tag. 1 stands for the entire file.
  Streamer.emitInt8(1);
  // Attribute vector length.
  Streamer.emitInt32(11);
  // OFBA_MSPABI_Tag_ISA(4) = 1, SyncVM
  Streamer.emitInt8(4);
  Streamer.emitInt8(1);
  // OFBA_MSPABI_Tag_Code_Model(6) = 1, Small
  Streamer.emitInt8(6);
  Streamer.emitInt8(1);
  // OFBA_MSPABI_Tag_Data_Model(8) = 1, Small
  Streamer.emitInt8(8);
  Streamer.emitInt8(1);
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
