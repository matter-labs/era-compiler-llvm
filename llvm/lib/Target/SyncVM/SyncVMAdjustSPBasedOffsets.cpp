//===-- SyncVMAdjustSPBasedOffsets.cpp - Expand pseudo instructions -------===//
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

using namespace llvm;

#define DEBUG_TYPE "syncvm-adjust-sp-based"
#define SYNCVM_ADJUST_SP_BASED_OFFSETS "SyncVM adjust SP-based offsets"

namespace {

class SyncVMAdjustSPBasedOffsets : public MachineFunctionPass {
public:
  static char ID;
  SyncVMAdjustSPBasedOffsets() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_ADJUST_SP_BASED_OFFSETS;
  }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMAdjustSPBasedOffsets::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMAdjustSPBasedOffsets, DEBUG_TYPE,
                SYNCVM_ADJUST_SP_BASED_OFFSETS, false, false)

bool SyncVMAdjustSPBasedOffsets::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM ADJUST SP-BASED OFFSETS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  if (MF.empty())
    return false;

  MachineBasicBlock &EntryBB = MF.front();
  unsigned Adjustment = 0;

  if (!EntryBB.empty() && EntryBB.front().getOpcode() == SyncVM::PUSH)
    Adjustment = EntryBB.front().getOperand(0).getImm() + 1;

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> Pseudos;
  for (MachineBasicBlock &MBB : MF) {
    for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE; ++MII) {
      auto &MI = *MII;
      switch (MI.getOpcode()) {
      case SyncVM::AdjMOVsr: {
        unsigned Offset = MI.getOperand(3).getImm();
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(SyncVM::MOVsr))
            .add(MI.getOperand(0))
            .add(MI.getOperand(1))
            .add(MI.getOperand(2))
            .addImm(Offset + Adjustment * 32);
        Pseudos.push_back(&MI);
        break;
      }
      case SyncVM::AdjMOVrs: {
        unsigned Offset = MI.getOperand(3).getImm();
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(SyncVM::MOVrs))
            .add(MI.getOperand(0))
            .add(MI.getOperand(1))
            .add(MI.getOperand(2))
            .addImm(Offset + Adjustment * 32);
        Pseudos.push_back(&MI);
        break;
      }
      case SyncVM::AdjSP: {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(SyncVM::ADDirr))
            .add(MI.getOperand(0))
            .add(MI.getOperand(1))
            .addImm(Adjustment * 32);
        Pseudos.push_back(&MI);
        break;
      }
      case SyncVM::AdjSPDown: {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(SyncVM::SUBrir))
            .add(MI.getOperand(0))
            .add(MI.getOperand(1))
            .addImm(Adjustment * 32);
        Pseudos.push_back(&MI);
        break;
      }
      }
    }
  }

  for (auto *MI : Pseudos)
    MI->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !Pseudos.empty();
}

/// createSyncVMAdjustSPBasedOffsetsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createSyncVMAdjustSPBasedOffsetsPass() {
  return new SyncVMAdjustSPBasedOffsets();
}
