//===-- EraVMSubtarget.cpp - EraVM Subtarget Information ------------------===//
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
    : EraVMGenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS), TLInfo(TM, *this) {}
