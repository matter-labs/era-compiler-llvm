//===-------- EVMFixupKinds.h - EVM Specific Fixup Entries ------*- C++ -*-===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMFIXUPKINDS_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

#undef EVM

namespace llvm {
namespace EVM {

// This table must be in the same order of
// MCFixupKindInfo Infos[EVM::NumTargetFixupKinds]
// in EVMAsmBackend.cpp.
//
enum Fixups {
  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // end namespace EVM
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMFIXUPKINDS_H
