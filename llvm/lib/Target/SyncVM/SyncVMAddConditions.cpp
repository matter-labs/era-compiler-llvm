//===------ SyncVMAddConditions.cpp - Expand pseudo instructions ----------===//
//
/// \file
/// This file contains a pass that expands pseudo instructions into target
/// instructions. This pass should be run after register allocation but before
/// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"
#include "SyncVMInstrInfo.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-addcc"
#define SYNCVM_ADD_CONDITIONALS_NAME "SyncVM add conditionals"

namespace {

class SyncVMAddConditions : public MachineFunctionPass {
public:
  static char ID;
  SyncVMAddConditions() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_ADD_CONDITIONALS_NAME;
  }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMAddConditions::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMAddConditions, DEBUG_TYPE, SYNCVM_ADD_CONDITIONALS_NAME,
                false, false)

bool SyncVMAddConditions::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      auto mappedOpcode = llvm::SyncVM::getPseudoMapOpcode(MI.getOpcode());
      if (mappedOpcode == -1) {
        continue;
      }

      MI.setDesc(TII->get(mappedOpcode));
      MI.addOperand(MachineOperand::CreateImm(0));
      Changed = true;
    }

  return Changed;
}

/// createSyncVMAddConditionsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createSyncVMAddConditionsPass() {
  return new SyncVMAddConditions();
}
