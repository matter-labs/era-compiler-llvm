//===-- SyncVMMCAsmInfo.h - SyncVM asm properties --------------*- C++ -*--===//
//
// This file contains the declaration of the SyncVMMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_MCTARGETDESC_SYNCVMMCASMINFO_H
#define LLVM_LIB_TARGET_SYNCVM_MCTARGETDESC_SYNCVMMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class SyncVMMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit SyncVMMCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
};

} // namespace llvm

#endif
