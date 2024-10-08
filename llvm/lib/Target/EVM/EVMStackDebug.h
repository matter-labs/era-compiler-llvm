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

#include "EVMControlFlowGraph.h"
#include "llvm/CodeGen/MachineFunction.h"
#include <cassert>
#include <map>
#include <numeric>
#include <set>
#include <variant>

namespace llvm {

struct StackLayout;

#ifndef NDEBUG
const Function *getCalledFunction(const MachineInstr &MI);
std::string stackSlotToString(const StackSlot &Slot);
std::string stackToString(Stack const &S);

class ControlFlowGraphPrinter {
public:
  ControlFlowGraphPrinter(raw_ostream &OS) : OS(OS) {}
  void operator()(const CFG &Cfg);

private:
  void operator()(const CFG::FunctionInfo &Info);
  std::string getBlockId(const CFG::BasicBlock &Block);
  void printBlock(const CFG::BasicBlock &Block);

  raw_ostream &OS;
};

class StackLayoutPrinter {
public:
  StackLayoutPrinter(raw_ostream &OS, const StackLayout &StackLayout)
      : OS(OS), Layout(StackLayout) {}

  void operator()(CFG::BasicBlock const &Block, bool IsMainEntry = true);
  void operator()(CFG::FunctionInfo const &Info);

private:
  void printBlock(CFG::BasicBlock const &Block);
  std::string getBlockId(CFG::BasicBlock const &Block);

  raw_ostream &OS;
  const StackLayout &Layout;
  std::map<CFG::BasicBlock const *, size_t> BlockIds;
  size_t BlockCount = 0;
  std::list<CFG::BasicBlock const *> BlocksToPrint;
};
#endif // NDEBUG

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMSTACKDEBUG_H
