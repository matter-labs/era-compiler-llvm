//===-- EraVMTargetInfo.cpp - EraVM Target Implementation -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file registers the EraVM target.
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/EraVMTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheEraVMTarget() {
  static Target TheEraVMTarget;
  return TheEraVMTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMTargetInfo() {
  RegisterTarget<Triple::eravm, /*HasJIT*/false> X(
    getTheEraVMTarget(),
    "eravm",
    "EraVM [experimental]",
    "EraVM"
  );
}
