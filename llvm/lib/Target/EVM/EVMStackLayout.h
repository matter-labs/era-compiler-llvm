//===---- EVMStackLayout.h - Stack layout ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the stack layout class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUT_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUT_H

#include "EVMStackModel.h"
#include "llvm/ADT/DenseMap.h"

#include <deque>

namespace llvm {
using MBBLayoutMapType = DenseMap<const MachineBasicBlock *, Stack>;
using OperationLayoutMapType = DenseMap<const Operation *, Stack>;

class EVMStackLayout {
public:
  EVMStackLayout(const MBBLayoutMapType &MBBEntryLayoutMap,
                 const MBBLayoutMapType &MBBExitLayoutMap,
                 const OperationLayoutMapType &OpEntryLayout)
      : MBBEntryLayoutMap(MBBEntryLayoutMap),
        MBBExitLayoutMap(MBBExitLayoutMap), OpEntryLayoutMap(OpEntryLayout) {}
  EVMStackLayout(const EVMStackLayout &) = delete;
  EVMStackLayout &operator=(const EVMStackLayout &) = delete;

  const Stack &getMBBEntryLayout(const MachineBasicBlock *MBB) const {
    return MBBEntryLayoutMap.at(MBB);
  }

  const Stack &getMBBExitLayout(const MachineBasicBlock *MBB) const {
    return MBBExitLayoutMap.at(MBB);
  }

  const Stack &getOperationEntryLayout(const Operation *Op) const {
    return OpEntryLayoutMap.at(Op);
  }

private:
  // Complete stack layout required at MBB entry.
  MBBLayoutMapType MBBEntryLayoutMap;
  // Complete stack layout required at MBB exit.
  MBBLayoutMapType MBBExitLayoutMap;
  // Complete stack layout that has the slots required for the
  // operation at the stack top.
  OperationLayoutMapType OpEntryLayoutMap;
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKLAYOUT_H
