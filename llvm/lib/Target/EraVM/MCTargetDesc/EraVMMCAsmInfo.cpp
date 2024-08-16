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

EraVMMCAsmInfo::EraVMMCAsmInfo(const Triple &TheTriple) {
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
  ExceptionsType = ExceptionHandling::EraVM;
  AllowDollarAtStartOfIdentifier = true;
  UseParensForDollarSignNames = false;
}

bool EraVMMCAsmInfo::shouldOmitSectionDirective(StringRef Name) const {
  return Name.starts_with(".linker_symbol_name") ? false : true;
}
