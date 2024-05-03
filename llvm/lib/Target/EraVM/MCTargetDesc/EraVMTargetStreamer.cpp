//===-- EraVMTargetStreamer.cpp - EraVM Target Streamer ---------*- C++ -*-===//
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

#include "MCTargetDesc/EraVMTargetStreamer.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/TargetParser/TargetParser.h"

using namespace llvm;

//
// EraVMTargetStreamer Implemenation
//

EraVMTargetStreamer::EraVMTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

EraVMTargetStreamer::~EraVMTargetStreamer() = default;
