//===--------- EVMTargetStreamer.h - EVMTargetStreamer class --*- C++ -*---===//
//
// This file declares the EVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMTARGETSTREAMER_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMTARGETSTREAMER_H
#include "llvm/MC/MCStreamer.h"

namespace llvm {

// EVM streamer interface to support EVM assembly directives
class EVMTargetStreamer : public MCTargetStreamer {
public:
  EVMTargetStreamer(MCStreamer &S);
  ~EVMTargetStreamer() override;
};

/// This part is for ASCII assembly output
class EVMTargetAsmStreamer final : public EVMTargetStreamer {
public:
  EVMTargetAsmStreamer(MCStreamer &S);
  ~EVMTargetAsmStreamer() override;
};

// This part is for EVM object output
class EVMTargetObjStreamer final : public EVMTargetStreamer {
public:
  EVMTargetObjStreamer(MCStreamer &S);
  ~EVMTargetObjStreamer() override;
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMTARGETSTREAMER_H
