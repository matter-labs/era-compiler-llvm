//===-- SyncVMDropUnusedRegisters.cpp - Replace unused registers with r0 --===//
//
/// \file
/// This file contains a pass that replaces unused definition with r0 so,
/// register allocator doesn't need to care.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-drop-unused-registers"
#define SYNCVM_DROP_UNUSED_REGS_NAME "SyncVM drop unused registers"

namespace {

class SyncVMDropUnusedRegisters : public MachineFunctionPass {
public:
  static char ID;
  SyncVMDropUnusedRegisters() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_DROP_UNUSED_REGS_NAME;
  }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMDropUnusedRegisters::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMDropUnusedRegisters, DEBUG_TYPE,
                SYNCVM_DROP_UNUSED_REGS_NAME, false, false)

bool SyncVMDropUnusedRegisters::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM DROP UNUSED REGISTERS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  for (MachineBasicBlock &MBB : MF)
    for (auto MII = MBB.begin(); MII != MBB.end(); ++MII) {
      MachineInstr &MI = *MII;
      if (MI.getOpcode() == SyncVM::POP) {
        MI.getOperand(0).setReg(SyncVM::R0);
        Changed = true;
      }
      if (MI.getOpcode() == SyncVM::LTFLAG) {
        --MII;
        MI.eraseFromParent();
        Changed = true;
      }
    }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

FunctionPass *llvm::createSyncVMDropUnusedRegistersPass() {
  return new SyncVMDropUnusedRegisters();
}
