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

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGET_STREAMER_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGET_STREAMER_H
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class EraVMTargetStreamer : public MCTargetStreamer {
public:
  EraVMTargetStreamer(MCStreamer &S);
  ~EraVMTargetStreamer() override;
  virtual void emitGlobalConst(APInt Value);

private:
  std::unique_ptr<AssemblerConstantPools> ConstantPools;
};
} // namespace llvm

#endif
