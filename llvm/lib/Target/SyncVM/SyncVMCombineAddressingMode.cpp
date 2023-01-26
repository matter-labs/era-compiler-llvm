//===- SyncVMCombineAddressingMode.cpp - Combine instructions via addrmode-===//
//
/// \file
///
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-combine-addressing-mode"
#define SYNCVM_COMBINE_ADDRESSING_MODE_NAME "SyncVM combine addressing mode"

static cl::opt<bool>
    EnableSyncVMCombineAddressingMode("enable-syncvm-combine-addressing-mode",
                                      cl::init(true), cl::Hidden);

STATISTIC(NumAMFolded, "Number of foldings done");

namespace {

class SyncVMCombineAddressingMode : public MachineFunctionPass {
public:
  static char ID;
  SyncVMCombineAddressingMode() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<ReachingDefAnalysis>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return SYNCVM_COMBINE_ADDRESSING_MODE_NAME;
  }

private:
  const SyncVMInstrInfo *TII;
  ReachingDefAnalysis *RDA;
  bool isZero(const MachineInstr& MI) const;
  bool canonicalizesOperationWithZero(MachineFunction &MF);
  //bool combineAddressingMode(MachineFunction &MF);
};

char SyncVMCombineAddressingMode::ID = 0;

}

INITIALIZE_PASS_BEGIN(SyncVMCombineAddressingMode, DEBUG_TYPE,
                SYNCVM_COMBINE_ADDRESSING_MODE_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_END(SyncVMCombineAddressingMode, DEBUG_TYPE,
                SYNCVM_COMBINE_ADDRESSING_MODE_NAME, false, false)

/// If MI materializes zero.
bool SyncVMCombineAddressingMode::isZero(const MachineInstr& MI) const {
  if (!TII->isAdd(MI))
    return false;
  unsigned NumDefs = MI.getNumExplicitDefs();
  return (TII->hasRROperandAddressingMode(MI) && MI.getOperand(NumDefs).getReg() == SyncVM::R0 && MI.getOperand(NumDefs + 1).getReg() == SyncVM::R0)
    || (TII->hasRIOperandAddressingMode(MI) && getImmOrCImm(MI.getOperand(NumDefs)) == 0 && MI.getOperand(NumDefs + 1).getReg() == SyncVM::R0);
}

bool isImmediateMaterialization(const MachineInstr& MI) {
  return MI.getOpcode() == SyncVM::ADDirr_s && MI.getOperand(1).getReg() == SyncVM::R0;
}

static void canonicalize(MachineInstr &MI) {
}

bool SyncVMCombineAddressingMode::canonicalizesOperationWithZero(MachineFunction &MF) {
  bool Changed = false;
  std::vector<MachineInstr *> Deleted;
  for (auto &MBB: MF)
    for (auto &MI: MBB) {
      if (isZero(MI) && MI.getNumDefs() == 1) {
        SmallPtrSet<MachineInstr*, 4> ZeroUses;
        Register ZeroDef = MI.getOperand(0).getReg();
        RDA->getGlobalUses(&MI, MI.getOperand(0).getReg(), ZeroUses);
        // If all the usages reached by 0 definition only.
        bool OnlyZeroUsed = true;
        for (MachineInstr *ZeroUse : ZeroUses) {
          if (ZeroUse->isReturn() || ZeroUse->isCall()) {
            OnlyZeroUsed = false;
            continue;
          }
          if (RDA->getUniqueReachingMIDef(ZeroUse, ZeroDef) == &MI) {
            for (MachineOperand &MO: ZeroUse->operands())
              if (MO.isReg() && MO.getReg() == ZeroDef) {
                MO.ChangeToRegister(SyncVM::R0, false);
                Changed = true;
              }
          } else {
            OnlyZeroUsed = false;
          }
        }
        if (OnlyZeroUsed)
          Deleted.push_back(&MI);
      }
    }
  for (MachineInstr *MI: Deleted)
    MI->eraseFromParent();
  return Changed;
}

/*
bool SyncVMCombineAddressingMode::combineAddressingMode(MachineFunction &MF) {
  bool Changed = false;
  std::vector<MachineInstr *> Deleted;
  for (auto &MBB: MF)
    for (auto &MI: MBB) {
      if (isZero(MI) && MI.getNumDefs() == 1) {
        SmallPtrSet<MachineInstr*, 4> ZeroUses;
        Register ZeroDef = MI.getOperand(0).getReg();
        RDA->getGlobalUses(&MI, MI.getOperand(0).getReg(), ZeroUses);
        // If all the usages reached by 0 definition only.
        bool OnlyZeroUsed = true;
        for (MachineInstr *ZeroUse : ZeroUses) {
          if (ZeroUse->isReturn() || ZeroUse->isCall()) {
            OnlyZeroUsed = false;
            continue;
          }
          if (RDA->getUniqueReachingMIDef(ZeroUse, ZeroDef) == &MI) {
            for (MachineOperand &MO: ZeroUse->operands())
              if (MO.isReg() && MO.getReg() == ZeroDef) {
                MO.ChangeToRegister(SyncVM::R0, false);
                Changed = true;
              }
          } else {
            OnlyZeroUsed = false;
          }
        }
        if (OnlyZeroUsed)
          Deleted.push_back(&MI);
      }
    }
  for (MachineInstr *MI: Deleted)
    MI->eraseFromParent();
  return Changed;
}
*/

bool SyncVMCombineAddressingMode::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM COMBINE INSTRUCTIONS **********\n"
                    << "********** Function: " << MF.getName() << '\n');
  if (!EnableSyncVMCombineAddressingMode)
    return false;
  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  RDA = &getAnalysis<ReachingDefAnalysis>();
  return canonicalizesOperationWithZero(MF);
}

FunctionPass *llvm::createSyncVMCombineAddressingModePass() {
  return new SyncVMCombineAddressingMode();
}
