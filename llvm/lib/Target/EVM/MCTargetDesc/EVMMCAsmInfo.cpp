//===-------- EVMMCAsmInfo.cpp - EVM asm properties -----------------------===//
//
// This file contains the declarations of the EVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "EVMMCAsmInfo.h"
#include "llvm/ADT/Triple.h"

using namespace llvm;

EVMMCAsmInfo::EVMMCAsmInfo(const Triple &TheTriple) {
  IsLittleEndian = false;
  HasFunctionAlignment = false;
  HasDotTypeDotSizeDirective = false;
  HasFourStringsDotFile = false;
  PrivateGlobalPrefix = ".";
  PrivateLabelPrefix = ".";
  AlignmentIsInBytes = true;
  PrependSymbolRefWithAt = true;
  CommentString = ";";
  SupportsDebugInformation = true;
}

bool EVMMCAsmInfo::shouldOmitSectionDirective(StringRef) const { return true; }
