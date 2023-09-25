//===-- EraVMFixupKinds.h - EraVM Specific Fixup Entries --------*- C++ -*-===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMFIXUPKINDS_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

#undef EraVM

namespace llvm {
namespace EraVM {

// This table must be in the same order of
// MCFixupKindInfo Infos[EraVM::NumTargetFixupKinds]
// in EraVMAsmBackend.cpp.
//
enum Fixups {
  // A 32 bit absolute fixup.
  fixup_32 = FirstTargetFixupKind,
  // A 10 bit PC relative fixup.
  fixup_10_pcrel,
  // A 16 bit absolute fixup.
  fixup_16,
  // A 16 bit PC relative fixup.
  fixup_16_pcrel,
  // A 16 bit absolute fixup for byte operations.
  fixup_16_byte,
  // A 16 bit PC relative fixup for command address.
  fixup_16_pcrel_byte,
  // A 10 bit PC relative fixup for complicated polymorphs.
  fixup_2x_pcrel,
  // A 16 bit relaxable fixup.
  fixup_rl_pcrel,
  // A 8 bit absolute fixup.
  fixup_8,
  // A 32 bit symbol difference fixup.
  fixup_sym_diff,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // end namespace EraVM
} // end namespace llvm

#endif
