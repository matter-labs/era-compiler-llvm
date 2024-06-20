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

  unsigned getTextSectionAlignment() const override;
};

} // end namespace llvm

#endif
