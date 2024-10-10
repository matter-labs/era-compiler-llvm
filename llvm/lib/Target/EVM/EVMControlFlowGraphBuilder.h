//===----- EVMControlFlowGraphBuilder.h - CFG builder -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file builds the Control Flow Graph used for the stackification
// algorithm.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPHBUILDER_H
#define LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPHBUILDER_H

#include "EVMControlFlowGraph.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

namespace llvm {

class MachineFunction;
class MachineBasicBlock;

class ControlFlowGraphBuilder {
public:
  ControlFlowGraphBuilder(ControlFlowGraphBuilder const &) = delete;
  ControlFlowGraphBuilder &operator=(ControlFlowGraphBuilder const &) = delete;
  static std::unique_ptr<CFG>
  build(MachineFunction &MF, const LiveIntervals &LIS, MachineLoopInfo *MLI);

private:
  ControlFlowGraphBuilder(CFG &Cfg, const LiveIntervals &LIS,
                          MachineLoopInfo *MLI)
      : Cfg(Cfg), LIS(LIS), MLI(MLI) {}

  void handleBasicBlock(MachineBasicBlock &MBB);
  void handleMachineInstr(MachineInstr &MI);
  void handleFunctionCall(const MachineInstr &MI);
  void handleReturn(const MachineInstr &MI);
  void handleBasicBlockSuccessors(MachineBasicBlock &MBB);
  void collectInstrOperands(const MachineInstr &MI, Stack &Input,
                            Stack &Output);

  CFG &Cfg;
  const LiveIntervals &LIS;
  MachineLoopInfo *MLI = nullptr;
  CFG::BasicBlock *CurrentBlock = nullptr;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPH_H
