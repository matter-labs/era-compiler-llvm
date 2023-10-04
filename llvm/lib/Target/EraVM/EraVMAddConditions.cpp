//===------ EraVMAddConditions.cpp - Expand pseudo instructions -----------===//
//
/// \file
/// This file contains a pass that expands pseudo instructions into target
/// instructions. This pass should be run after register allocation but before
/// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-addcc"
#define ERAVM_ADD_CONDITIONALS_NAME "EraVM add conditionals"

namespace {

class EraVMAddConditions : public MachineFunctionPass {
public:
  static char ID;
  EraVMAddConditions() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI{};

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_ADD_CONDITIONALS_NAME; }

private:
  const TargetInstrInfo *TII{};
};

char EraVMAddConditions::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMAddConditions, DEBUG_TYPE, ERAVM_ADD_CONDITIONALS_NAME,
                false, false)

bool EraVMAddConditions::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      auto mappedOpcode = llvm::EraVM::getPseudoMapOpcode(MI.getOpcode());
      if (mappedOpcode == -1) {
        continue;
      }

      MI.setDesc(TII->get(mappedOpcode));
      MI.addOperand(MachineOperand::CreateImm(0));
      Changed = true;
    }

  return Changed;
}

/// createEraVMAddConditionsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createEraVMAddConditionsPass() {
  return new EraVMAddConditions();
}
