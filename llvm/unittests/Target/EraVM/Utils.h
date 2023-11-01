//===-- Utils.h - Testing utilities -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines utilities for testing.
//
//===----------------------------------------------------------------------===//

#include "EraVMTargetMachine.h"
#include "EraVMSubtarget.h"

#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "gtest/gtest.h"

using namespace llvm;
static std::once_flag flag;

static void InitializeEraVMTarget() {
  std::call_once(flag, []() {
    LLVMInitializeEraVMTargetInfo();
    LLVMInitializeEraVMTarget();
    LLVMInitializeEraVMTargetMC();
  });
}

[[maybe_unused]]
std::unique_ptr<const EraVMTargetMachine>
createEraVMTargetMachine() {
  InitializeEraVMTarget();

  std::string Error;
  const Target *T = TargetRegistry::lookupTarget("eravm", Error);
  if (!T)
    return nullptr;

  TargetOptions Options;
  return std::unique_ptr<EraVMTargetMachine>(static_cast<EraVMTargetMachine *>(
      T->createTargetMachine("eravm", "", "", Options, None, None)));
}
