//===----- EVMCalculateModuleSize.h - Calculate module size ----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A utility set for determining the overall size of a module.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMCALCULATEMODULESIZE_H
#define LLVM_LIB_TARGET_EVM_EVMCALCULATEMODULESIZE_H

#include <cstdint>

namespace llvm {
class MachineFunction;
class MachineModuleInfo;
class Module;

namespace EVM {
uint64_t calculateFunctionCodeSize(const MachineFunction &MF);
uint64_t calculateModuleCodeSize(Module &M, const MachineModuleInfo &MMI);
} // namespace EVM
} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMCALCULATEMODULESIZE_H
