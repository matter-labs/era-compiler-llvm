//===------------------ EVMInstrInfo.cpp - EVM Instruction Information ----===//
//
// This file contains the EVM implementation of the
// TargetInstrInfo class.
///
//===----------------------------------------------------------------------===//

#include "EVMInstrInfo.h"
using namespace llvm;

#define DEBUG_TYPE "evm-instr-info"

#define GET_INSTRINFO_CTOR_DTOR
#include "EVMGenInstrInfo.inc"

void EVMInstrInfo::anchor() {}

EVMInstrInfo::EVMInstrInfo() : EVMGenInstrInfo(), RI() {}
