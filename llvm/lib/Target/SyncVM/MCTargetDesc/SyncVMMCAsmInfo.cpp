//===-- SyncVMMCAsmInfo.cpp - SyncVM asm properties -----------------------===//
//
// This file contains the declarations of the SyncVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "SyncVMMCAsmInfo.h"
using namespace llvm;

void SyncVMMCAsmInfo::anchor() { }

SyncVMMCAsmInfo::SyncVMMCAsmInfo(const Triple &TT,
                                 const MCTargetOptions &Options) {
  CodePointerSize = CalleeSaveStackSlotSize = 2;

  CommentString = ";";
  SeparatorString = "{";

  AlignmentIsInBytes = false;
  UsesELFSectionDirectiveForBSS = true;

  SupportsDebugInformation = true;
}
