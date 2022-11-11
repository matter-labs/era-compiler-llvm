//===-------- EVMFixupKinds.h - EVM Specific Fixup Entries ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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
