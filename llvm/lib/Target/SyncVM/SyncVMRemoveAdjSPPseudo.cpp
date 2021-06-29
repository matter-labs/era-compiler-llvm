//===-- SyncVMRemoveAdjSPPseudo.cpp - Remove AdjSP pseudo instructions ----===//
//
/// \file
/// Contains a pass that removes AdjSP x instructions and replaces all the uses
/// with x.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-remove-adjsp"
#define SYNCVM_REMOVE_ADJSP_PSEUDO "SyncVM remove AdjSP pseudos"

namespace {

class SyncVMRemoveAdjSPPseudo : public MachineFunctionPass {
public:
  static char ID;
  SyncVMRemoveAdjSPPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_REMOVE_ADJSP_PSEUDO; }
};

char SyncVMRemoveAdjSPPseudo::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMRemoveAdjSPPseudo, DEBUG_TYPE, SYNCVM_REMOVE_ADJSP_PSEUDO,
                false, false)

bool SyncVMRemoveAdjSPPseudo::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM REMOVE AdjSP PSEUDOS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MachineRegisterInfo &MRI = MF.getRegInfo();

  std::vector<MachineInstr *> Pseudos;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == SyncVM::AdjSP) {
        MRI.replaceRegWith(MI.getOperand(0).getReg(),
                           MI.getOperand(1).getReg());
        Pseudos.push_back(&MI);
      }

  for (auto *MI : Pseudos)
    MI->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !Pseudos.empty();
}

FunctionPass *llvm::createSyncVMRemoveAdjSPPseudoPass() {
  return new SyncVMRemoveAdjSPPseudo();
}
