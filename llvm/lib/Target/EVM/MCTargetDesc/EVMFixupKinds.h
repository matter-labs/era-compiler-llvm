//===------ EVMFixupKinds.h - EVM Specific Fixup Entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMFIXUPKINDS_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace EVM {
enum Fixups {
  fixup_SecRel_i64 = FirstTargetFixupKind, // 64-bit unsigned
  fixup_SecRel_i56,                        // 56-bit unsigned
  fixup_SecRel_i48,                        // 48-bit unsigned
  fixup_SecRel_i40,                        // 40-bit unsigned
  fixup_SecRel_i32,                        // 32-bit unsigned
  fixup_SecRel_i24,                        // 24-bit unsigned
  fixup_SecRel_i16,                        // 16-bit unsigned
  fixup_SecRel_i8,                         // 8-bit unsigned
  fixup_Data_i32,                          // 32-bit unsigned

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // end namespace EVM
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMFIXUPKINDS_H
