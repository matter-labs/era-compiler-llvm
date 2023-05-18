//===------ EVMMCAsmInfo.h - EVM asm properties --------------*- C++ -*----===//
//
// This file contains the declaration of the EVMMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class EVMMCAsmInfo final : public MCAsmInfoELF {
public:
  explicit EVMMCAsmInfo(const Triple &TT);
  bool shouldOmitSectionDirective(StringRef) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCASMINFO_H
