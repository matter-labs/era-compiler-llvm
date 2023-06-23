//===---------- EVMArgumentMove.cpp - Argument instruction moving ---------===//
//
//
//===----------------------------------------------------------------------===//
//
//
// This file moves ARGUMENT instructions after ScheduleDAG scheduling.
//
// Arguments are really live-in registers, however, since we use virtual
// registers and LLVM doesn't support live-in virtual registers, we're
// currently making do with ARGUMENT instructions which are placed at the top
// of the entry block. The trick is to get them to *stay* at the top of the
// entry block.
//
// The ARGUMENTS physical register keeps these instructions pinned in place
// during liveness-aware CodeGen passes, however one thing which does not
// respect this is the ScheduleDAG scheduler. This pass is therefore run
// immediately after that.
//
// This is all hopefully a temporary solution until we find a better solution
// for describing the live-in nature of arguments.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "evm-argument-move"

namespace {
class EVMArgumentMove final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  EVMArgumentMove() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM Argument Move"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addPreserved<MachineBlockFrequencyInfo>();
    AU.addPreservedID(MachineDominatorsID);
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // end anonymous namespace

char EVMArgumentMove::ID = 0;
INITIALIZE_PASS(EVMArgumentMove, DEBUG_TYPE,
                "Move ARGUMENT instructions for EVM", false, false)

FunctionPass *llvm::createEVMArgumentMove() { return new EVMArgumentMove(); }

bool EVMArgumentMove::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Argument Move **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  bool Changed = false;
  MachineBasicBlock &EntryMBB = MF.front();

  // Look for the first NonArg instruction.
  MachineBasicBlock::iterator InsertPt =
      std::find_if_not(EntryMBB.begin(), EntryMBB.end(), [](auto &MI) {
        return EVM::ARGUMENT == MI.getOpcode();
      });

  // Now move any argument instructions later in the block
  // to before our first NonArg instruction.
  for (MachineInstr &MI : llvm::make_range(InsertPt, EntryMBB.end())) {
    if (EVM::ARGUMENT == MI.getOpcode()) {
      EntryMBB.insert(InsertPt, MI.removeFromParent());
      Changed = true;
    }
  }

  return Changed;
}
