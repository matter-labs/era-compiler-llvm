//===------- EVMTargetStreamer.cpp - EVMTargetStreamer class --*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMTargetStreamer.h"

using namespace llvm;

// EVMTargetStreamer implementations

EVMTargetStreamer::EVMTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

EVMTargetStreamer::~EVMTargetStreamer() = default;

EVMTargetObjStreamer::EVMTargetObjStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetObjStreamer::~EVMTargetObjStreamer() = default;

EVMTargetAsmStreamer::EVMTargetAsmStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetAsmStreamer::~EVMTargetAsmStreamer() = default;
