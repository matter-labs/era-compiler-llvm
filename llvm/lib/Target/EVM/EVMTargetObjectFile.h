//===---------- EVMTargetObjectFile.h - EVM Object Info -*- C++ ---------*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the EVM-specific subclass of
// TargetLoweringObjectFile.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMTARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_EVM_EVMTARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

class EVMELFTargetObjectFile final : public TargetLoweringObjectFileELF {
public:
  EVMELFTargetObjectFile() = default;

  // Code sections need to be aligned on 1, otherwise linker will add padding
  // between .text sections of the object files being linked.
  unsigned getTextSectionAlignment() const override { return 1; }
};

} // end namespace llvm

#endif
