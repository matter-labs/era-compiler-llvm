//===-- EraVMTargetStreamer.h - EraVM Target Streamer -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMTARGETSTREAMER_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMTARGETSTREAMER_H
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class EraVMTargetStreamer : public MCTargetStreamer {
public:
  explicit EraVMTargetStreamer(MCStreamer &S);
  ~EraVMTargetStreamer() override;
  EraVMTargetStreamer(const EraVMTargetStreamer &) = delete;
  EraVMTargetStreamer &operator=(EraVMTargetStreamer &) = delete;
  EraVMTargetStreamer(EraVMTargetStreamer &&) = delete;
  EraVMTargetStreamer &&operator=(EraVMTargetStreamer &&) = delete;

  /// Emit .cell N (naturally-aligned 256-bit value).
  virtual void emitCell(const APInt &Value) {}

  /// Emit `.cell @tgt` where `@tgt` is an instruction address.
  virtual void emitJumpTarget(const MCExpr *Expr) {}

  virtual void emitLinkerSymbol(const MCSymbol *Symbol) {}
};
} // namespace llvm

#endif
