//===--------- EVMTargetStreamer.h - EVMTargetStreamer class --*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the EVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMTARGETSTREAMER_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMTARGETSTREAMER_H
#include "llvm/MC/MCStreamer.h"

namespace llvm {

// EVM streamer interface to support EVM assembly directives
class EVMTargetStreamer : public MCTargetStreamer {
public:
  explicit EVMTargetStreamer(MCStreamer &S);
  EVMTargetStreamer(const EVMTargetStreamer &) = delete;
  EVMTargetStreamer(EVMTargetStreamer &&) = delete;
  EVMTargetStreamer &operator=(const EVMTargetStreamer &) = delete;
  EVMTargetStreamer &operator=(EVMTargetStreamer &&) = delete;

  void emitLabel(MCSymbol *Symbol) override;
};

/// This part is for ASCII assembly output
class EVMTargetAsmStreamer final : public EVMTargetStreamer {
public:
  explicit EVMTargetAsmStreamer(MCStreamer &S);
  EVMTargetAsmStreamer(const EVMTargetAsmStreamer &) = delete;
  EVMTargetAsmStreamer(EVMTargetAsmStreamer &&) = delete;
  EVMTargetAsmStreamer &operator=(const EVMTargetAsmStreamer &) = delete;
  EVMTargetAsmStreamer &operator=(EVMTargetAsmStreamer &&) = delete;
  ~EVMTargetAsmStreamer() override;
};

// This part is for EVM object output
class EVMTargetObjStreamer final : public EVMTargetStreamer {
public:
  explicit EVMTargetObjStreamer(MCStreamer &S);
  EVMTargetObjStreamer(const EVMTargetObjStreamer &) = delete;
  EVMTargetObjStreamer(EVMTargetObjStreamer &&) = delete;
  EVMTargetObjStreamer &operator=(const EVMTargetObjStreamer &) = delete;
  EVMTargetObjStreamer &operator=(EVMTargetObjStreamer &&) = delete;
  ~EVMTargetObjStreamer() override;
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMTARGETSTREAMER_H
