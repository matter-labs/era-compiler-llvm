//===-- SyncVMSubtarget.cpp - SyncVM Subtarget Information ----------------===//
//
// This file implements the SyncVM specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "SyncVMSubtarget.h"
#include "SyncVM.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "SyncVMGenSubtargetInfo.inc"

void SyncVMSubtarget::anchor() { }

SyncVMSubtarget::SyncVMSubtarget(const Triple &TT, const std::string &CPU,
                                 const std::string &FS, const TargetMachine &TM)
    : SyncVMGenSubtargetInfo(TT, CPU, FS),
      FrameLowering(),
      InstrInfo(),
      TLInfo(TM, *this)
{}
