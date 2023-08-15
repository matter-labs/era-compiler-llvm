//===-- SyncVMFoldSelect.cpp - Fold select pseudo instruction ---------===//
//
/// \file
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-fold-select"
#define SYNCVM_FOLD_SELECT_NAME "SyncVM fold select with selectees"

namespace {

class SyncVMFoldSelect : public MachineFunctionPass {
public:
  static char ID;
  SyncVMFoldSelect() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &Fn) override;
  StringRef getPassName() const override { return SYNCVM_FOLD_SELECT_NAME; }

private:
  const SyncVMInstrInfo *TII;
};

char SyncVMFoldSelect::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMFoldSelect, DEBUG_TYPE, SYNCVM_FOLD_SELECT_NAME,
                false, false)

bool SyncVMFoldSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');


  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return false;
}

/// createSyncVMFoldPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createSyncVMFoldSelectPass() {
  return new SyncVMFoldSelect();
}
