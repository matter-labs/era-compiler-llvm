//===---- EVMStackDebug.h - Debugging of the stackification -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares classes for dumping a state of stackifcation related data
// structures and algorithms.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMSTACKDEBUG_H
#define LLVM_LIB_TARGET_EVM_EVMSTACKDEBUG_H

#include "EVMMachineCFGInfo.h"
#include "EVMStackModel.h"
#include "llvm/CodeGen/MachineFunction.h"
#include <cassert>
#include <map>
#include <numeric>
#include <set>
#include <variant>

namespace llvm {

class EVMStackLayout;

const Function *getCalledFunction(const MachineInstr &MI);
std::string stackSlotToString(const StackSlot &Slot);
std::string stackToString(Stack const &S);

#ifndef NDEBUG
class StackLayoutPrinter {
public:
  StackLayoutPrinter(raw_ostream &OS, const MachineFunction &MF,
                     const EVMStackLayout &EVMStackLayout,
                     const EVMMachineCFGInfo &CFGInfo,
                     const EVMStackModel &StackModel)
      : OS(OS), MF(MF), Layout(EVMStackLayout), CFGInfo(CFGInfo),
        StackModel(StackModel) {}
  void operator()();

private:
  void printBlock(MachineBasicBlock const &Block);
  std::string getBlockId(MachineBasicBlock const &Block);

  raw_ostream &OS;
  const MachineFunction &MF;
  const EVMStackLayout &Layout;
  const EVMMachineCFGInfo &CFGInfo;
  const EVMStackModel &StackModel;
  size_t BlockCount = 0;
  std::map<const MachineBasicBlock *, size_t> BlockIds;
};
#endif // NDEBUG

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKDEBUG_H
