//===- EraVM.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class EraVM final : public TargetInfo {
public:
  EraVM();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

EraVM::EraVM() {
  defaultImageBase = 0;

  // Prevent unnecessary over-alignment when switching between .code and .rodata
  // sections (as the former is executable and the latter is non-executable).
  defaultCommonPageSize = 32;
  defaultMaxPageSize = 32;
}

RelExpr EraVM::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  return R_ABS;
}

void EraVM::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  auto add16scaled = [loc, &rel](uint64_t value, uint64_t scale) {
    uint64_t scaledValue = value / scale;
    checkAlignment(loc, value, scale, rel);
    checkIntUInt(loc, scaledValue, 16, rel);
    write16be(loc, read16be(loc) + scaledValue);
  };

  uint64_t scale = 0;
  switch (rel.type) {
  case R_ERAVM_16_SCALE_32:
    scale = 32;
    break;
  case R_ERAVM_16_SCALE_8:
    scale = 8;
    break;
  default: {
    error(getErrorLocation(loc) + "unrecognized relocation " +
          toString(rel.type));
    return;
  }
  }

  // HACK: there may be cases where the relocation represents the code offset
  // (measured in 8-byte units), for example:
  //
  //     add   @.OUTLINED_FUNCTION_RET0[0], r0, stack-[1]
  //     jump  @OUTLINED_FUNCTION_0
  //   .OUTLINED_FUNCTION_RET0:
  //     jump.ge @.BB1_16
  //     jump  @.BB1_23
  //
  // In such cases the actual relocation type should be R_ERAVM_16_SCALE_8.
  // TODO: check if this can be done properly done on the LLVM MC layer?
  auto *sec = cast<InputSection>(cast<Defined>(rel.sym)->section);
  if (sec->name == ".text" && rel.type == R_ERAVM_16_SCALE_32)
    scale = 8;

  add16scaled(val, scale);
}

TargetInfo *elf::getEraVMTargetInfo() {
  static EraVM target;
  return &target;
}
