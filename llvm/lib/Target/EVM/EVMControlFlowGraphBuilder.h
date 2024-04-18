#ifndef LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPHBUILDER_H
#define LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPHBUILDER_H

#include "EVMControlFlowGraph.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

namespace llvm {

class MachineFunction;
class MachineBasicBlock;

class ControlFlowGraphPrinter {
public:
  ControlFlowGraphPrinter(raw_ostream &OS) : OS(OS) {}

  void operator()(CFG &Cfg);

private:
  void operator()(CFG::FunctionInfo const &Info);
  std::string getBlockId(CFG::BasicBlock const &Block);
  void printBlock(CFG::BasicBlock const &Block);

  raw_ostream &OS;
};

class ControlFlowGraphBuilder {
public:
  ControlFlowGraphBuilder(ControlFlowGraphBuilder const &) = delete;
  ControlFlowGraphBuilder &operator=(ControlFlowGraphBuilder const &) = delete;
  static std::unique_ptr<CFG>
  build(MachineFunction &MF, const LiveIntervals &LIS, MachineLoopInfo *MLI);

private:
  ControlFlowGraphBuilder(CFG &Cfg, MachineFunction &MF,
                          const LiveIntervals &LIS, MachineLoopInfo *MLI)
      : Cfg(Cfg), MF(MF), LIS(LIS), MLI(MLI) {}

  void handleBasicBlock(const MachineBasicBlock &MBB);
  void handleMachineInstr(const MachineInstr &MI);
  void handleFunctionCall(const MachineInstr &MI);
  void handleReturn(const MachineInstr &MI);
  void handleBasicBlockSuccessors(MachineBasicBlock &MBB);
  void collectInOut(const MachineInstr &MI, Stack &Input, Stack &Output);
  void dump() const;

  CFG &Cfg;
  const MachineFunction &MF;
  const LiveIntervals &LIS;
  MachineLoopInfo *MLI = nullptr;
  CFG::BasicBlock *CurrentBlock = nullptr;
  DenseSet<const MachineInstr *> InstrToSkip;
};

} // namespace llvm
#endif // LLVM_LIB_TARGET_EVM_EVMCONTROLFLOWGRAPH_H
