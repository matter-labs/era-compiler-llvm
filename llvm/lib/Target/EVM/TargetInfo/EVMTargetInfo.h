//===-------- EVMTargetInfo.h - EVM Target Implementation -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file registers the EVM target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H
#define LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H

namespace llvm {

class Target;

Target &getTheEVMTarget();

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H
