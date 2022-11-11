//===-- EraVMMCAsmInfo.cpp - EraVM asm properties ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the EraVMMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "EraVMMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void EraVMMCAsmInfo::anchor() {}

EraVMMCAsmInfo::EraVMMCAsmInfo(const Triple &TheTriple) {
  IsLittleEndian = false;
  HasFunctionAlignment = false;
  HasDotTypeDotSizeDirective = false;
  HasFourStringsDotFile = false;
  // GlobalDirective = nullptr;
  PrivateGlobalPrefix = ".";
  PrivateLabelPrefix = ".";
  AlignmentIsInBytes = true;

  CommentString = ";";

  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::EraVM;
}
