//===-------- EVMSubtarget.cpp - EVM Subtarget Information ---*- C++ -*----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVM specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "EVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "evm-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#define GET_SUBTARGETINFO_ENUM
#include "EVMGenSubtargetInfo.inc"

EVMSubtarget &EVMSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                            StringRef FS) {
  ParseSubtargetFeatures(CPU, /*TuneCPU*/ CPU, FS);
  return *this;
}

EVMSubtarget::EVMSubtarget(const Triple &TT, const std::string &CPU,
                           const std::string &FS, const TargetMachine &TM)
    : EVMGenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS),
      TLInfo(TM, initializeSubtargetDependencies(CPU, FS)) {}

bool EVMSubtarget::useAA() const { return true; }
