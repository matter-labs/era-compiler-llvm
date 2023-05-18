//===------- EVMTargetStreamer.cpp - EVMTargetStreamer class --*- C++ -*---===//
//
// This file implements the EVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EVMTargetStreamer.h"

using namespace llvm;

// EVMTargetStreamer implemenations

EVMTargetStreamer::EVMTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

EVMTargetStreamer::~EVMTargetStreamer() = default;

EVMTargetObjStreamer::EVMTargetObjStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetObjStreamer::~EVMTargetObjStreamer() = default;

EVMTargetAsmStreamer::EVMTargetAsmStreamer(MCStreamer &S)
    : EVMTargetStreamer(S) {}

EVMTargetAsmStreamer::~EVMTargetAsmStreamer() = default;
