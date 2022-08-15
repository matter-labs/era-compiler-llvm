//===-- SyncVMMCAsmInfo.cpp - SyncVM asm properties -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the SyncVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "SyncVMMCAsmInfo.h"
#include "llvm/ADT/Triple.h"

using namespace llvm;

void SyncVMMCAsmInfo::anchor() { }

SyncVMMCAsmInfo::SyncVMMCAsmInfo(const Triple &TheTriple) {
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
  ExceptionsType = ExceptionHandling::SyncVM;
}
