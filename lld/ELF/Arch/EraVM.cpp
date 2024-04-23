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
#include "llvm/Support/Debug.h"
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

  // "panic" is 8 bytes as any other EraVM instruction, but 4 bytes required.
  // trapInstr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31};
}

RelExpr EraVM::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  return R_ABS;
}

void EraVM::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  auto *definedSym = dyn_cast<Defined>(rel.sym);
  assert(definedSym && "Defined rel.sym expected");
  uint64_t addressInSection =
      definedSym->getVA(rel.addend) - definedSym->section->getVA();

  auto add16scaled = [loc, &rel](uint64_t value, uint64_t scale) {
    uint64_t scaledValue = value / scale;
    checkAlignment(loc, value, scale, rel);
    checkIntUInt(loc, scaledValue, 16, rel);
    write16be(loc, read16be(loc) + scaledValue);
  };

  switch (rel.type) {
  case R_ERAVM_16_SCALE_32:
    add16scaled(addressInSection, /*scale=*/32);
    break;
  case R_ERAVM_16_SCALE_8:
    add16scaled(addressInSection, /*scale=*/8);
    break;
  default:
    error(getErrorLocation(loc) + "unrecognized relocation " +
          toString(rel.type));
  }
}

TargetInfo *elf::getEraVMTargetInfo() {
  static EraVM target;
  return &target;
}
