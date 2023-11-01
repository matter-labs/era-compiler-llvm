//===-- EraVMSubtarget.cpp - EraVM Subtarget Information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EraVM specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "EraVMGenSubtargetInfo.inc"

void EraVMSubtarget::anchor() {}

EraVMSubtarget::EraVMSubtarget(const Triple &TT, const std::string &CPU,
                               const std::string &FS, const TargetMachine &TM)
    : EraVMGenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS), FrameLowering(),
      InstrInfo(), TLInfo(TM, *this) {}
