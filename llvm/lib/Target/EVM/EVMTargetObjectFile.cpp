//===------------- EVMTargetObjectFile.cpp - EVM Object Info --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the functions of the EVM-specific subclass
// of TargetLoweringObjectFile.
//
//===----------------------------------------------------------------------===//

#include "EVMTargetObjectFile.h"

using namespace llvm;

// Code sections need to be aligned on 1, otherwise linker will add padding
// between .text sections of the object files being linked.
unsigned EVMELFTargetObjectFile::getTextSectionAlignment() const { return 1; }
