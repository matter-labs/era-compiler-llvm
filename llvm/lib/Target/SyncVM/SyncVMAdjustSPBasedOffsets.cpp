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

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_ADJUST_SP_BASED_OFFSETS;
  }

private:
  void expandConst(MachineInstr &MI) const;
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMAdjustSPBasedOffsets::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMAdjustSPBasedOffsets, DEBUG_TYPE,
                SYNCVM_ADJUST_SP_BASED_OFFSETS, false, false)

MachineBasicBlock::iterator adjSPLookup(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator It,
                                        unsigned Reg) {
  auto Begin = MBB.begin();
  while (Begin != It) {
    for (auto &MOP : It->defs()) {
      if (MOP.isReg() && MOP.getReg() == Reg) {
        if (It->getOpcode() == SyncVM::AdjSP) {
          // FIXME: wrong assumption
          assert(It->getOperand(1).getReg() == Reg && "Unexpected format");
          return It;
        } else {
          return MBB.end();
        }
      }
    }
    --It;
  }
  for (auto &MOP : It->defs()) {
    if (MOP.isReg() && MOP.getReg() == Reg) {
      if (It->getOpcode() == SyncVM::AdjSP) {
        assert(It->getOperand(1).getReg() == Reg && "Unexpected format");
        return It;
      } else {
        return MBB.end();
      }
    }
  }
  llvm_unreachable("No defs for Reg");
  return MBB.end();
}

bool SyncVMAdjustSPBasedOffsets::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM ADJUST SP-BASED OFFSETS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  if (MF.empty())
    return false;

  MachineBasicBlock &EntryBB = MF.front();
  unsigned Adjustment = 0;

  if (!EntryBB.empty() && EntryBB.front().getOpcode() == SyncVM::PUSH)
    Adjustment = EntryBB.front().getOperand(0).getImm();

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> Pseudos;
  for (MachineBasicBlock &MBB : MF) {
    for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE; ++MII) {
      auto &MI = *MII;
      switch (MI.getOpcode()) {
      case SyncVM::MOVsr:
      case SyncVM::MOVrs:
        if (MI.getOperand(2).isReg()) {
          auto It = adjSPLookup(MBB, std::prev(MII), MI.getOperand(2).getReg());
          if (It != MIE) {
            auto Offset = MI.getOperand(3).getImm();
            MI.getOperand(3).ChangeToImmediate(Offset + Adjustment * 32);
          }
        }
        break;
      }
    }
  }

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == SyncVM::AdjSP)
        Pseudos.push_back(&MI);

  for (auto *I : Pseudos)
    I->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !Pseudos.empty();
}

/// createSyncVMAdjustSPBasedOffsetsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createSyncVMAdjustSPBasedOffsetsPass() {
  return new SyncVMAdjustSPBasedOffsets();
}
