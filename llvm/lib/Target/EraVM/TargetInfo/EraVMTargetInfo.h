//===-- EraVMTargetInfo.h - EraVM Target Implementation ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file registers the EraVM target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_TARGETINFO_ERAVMTARGETINFO_H
#define LLVM_LIB_TARGET_ERAVM_TARGETINFO_ERAVMTARGETINFO_H

namespace llvm {

class Target;

Target &getTheEraVMTarget();

} // namespace llvm

#endif // LLVM_LIB_TARGET_ERAVM_TARGETINFO_ERAVMTARGETINFO_H
