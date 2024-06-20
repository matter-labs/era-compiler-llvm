//===-------- EVMMCAsmInfo.cpp - EVM asm properties ------*- C++ -*--------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the EVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "EVMMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

EVMMCAsmInfo::EVMMCAsmInfo(const Triple &TT) {
  IsLittleEndian = false;
  HasFunctionAlignment = false;
  HasDotTypeDotSizeDirective = true;
  HasFourStringsDotFile = false;
  PrivateGlobalPrefix = ".";
  PrivateLabelPrefix = ".";
  AlignmentIsInBytes = true;
  PrependSymbolRefWithAt = true;
  CommentString = ";";
  SupportsDebugInformation = true;
}

bool EVMMCAsmInfo::shouldOmitSectionDirective(StringRef) const { return true; }
