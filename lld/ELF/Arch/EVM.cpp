//===- EVM.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// EVM is a stack-based virtual machine with a word size of 256 bits intendent
// for execution of smart contracts in Ethereum blockchain environment.
//
// Since it is a baremetal programming, there's usually no loader to load
// ELF files on EVMs. You are expected to link your program against address
// 0 and pull out a .text section from the result using objcopy, so that you
// can write the linked code to on-chip flush memory. You can do that with
// the following commands:
//
//   ld.lld -Ttext=0 -o foo foo.o
//   objcopy -O binary --only-section=.text foo output.bin
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
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
class EVM final : public TargetInfo {
public:
  uint32_t calcEFlags() const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

RelExpr EVM::getRelExpr(RelType type, const Symbol &s,
                        const uint8_t *loc) const {
  switch (type) {
  case R_EVM_DATASIZE:
  case R_EVM_DATASIZE_RUNTIME:
    return R_ABS;
  default:
    error(getErrorLocation(loc) + "unknown relocation (" + Twine(type) +
          ") against symbol " + toString(s));
    return R_NONE;
  }
}

void EVM::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_EVM_DATASIZE:
  case R_EVM_DATASIZE_RUNTIME:
    write64be(loc, val);
    break;
  default:
    llvm_unreachable("unknown relocation");
  }
}

TargetInfo *elf::getEVMTargetInfo() {
  static EVM target;
  return &target;
}

static uint32_t getEFlags(InputFile *file) {
  return cast<ObjFile<ELF32LE>>(file)->getObj().getHeader().e_flags;
}

uint32_t EVM::calcEFlags() const {
  assert(!ctx->objectFiles.empty());

  const uint32_t flags = getEFlags(ctx->objectFiles[0]);
  for (InputFile *f : makeArrayRef(ctx->objectFiles).slice(1)) {
    const uint32_t objFlags = getEFlags(f);
    if ((objFlags /* & EF_EVM_ARCH_MASK*/) != (flags /* & EF_EVM_ARCH_MASK*/))
      error(toString(f) +
            ": cannot link object files with incompatible target ISA");
  }

  return flags;
}
