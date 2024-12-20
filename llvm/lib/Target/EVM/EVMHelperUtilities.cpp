//===----------- EVMHelperUtilities.cpp - Helper utilities ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "EVMHelperUtilities.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/IR/Function.h"

using namespace llvm;

// Return whether the function of the call instruction will return.
bool EVMUtils::callWillReturn(const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  const MachineOperand *FuncOp = Call->explicit_uses().begin();
  assert(FuncOp->isGlobal() && "Expected a global value");
  const auto *Func = dyn_cast<Function>(FuncOp->getGlobal());
  assert(Func && "Expected a function");
  return !Func->hasFnAttribute(Attribute::NoReturn);
}
