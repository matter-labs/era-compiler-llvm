//===-- EraVMFixupKinds.h - EraVM Specific Fixup Entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EraVM specific fixup entries.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMFIXUPKINDS_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

#undef EraVM

namespace llvm {
namespace EraVM {

// At now, two fixups are defined as stack and const operands operate in 32-byte
// units and jump target operands point to an 8-byte instruction.
// The semantics of fixup_16_scale_N is "take the big-endian value P in place
// and overwrite it with P + AbsAddr(@sym) / N. If AbsAddr(@sym) is not
// multiple of N, error should be emitted".

// This table must be in the same order of
// MCFixupKindInfo Infos[EraVM::NumTargetFixupKinds]
// in EraVMAsmBackend.cpp.
//
enum Fixups {
  // A 16 bit absolute fixup for stack and const operands.
  fixup_16_scale_32 = FirstTargetFixupKind,
  // A 16 bit absolute fixup for jump target operands.
  fixup_16_scale_8,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // end namespace EraVM
} // end namespace llvm

#endif
