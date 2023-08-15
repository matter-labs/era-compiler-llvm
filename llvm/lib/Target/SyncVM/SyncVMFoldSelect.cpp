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
  const MachineInstr* getFlagSettingInstrInSameBB(const MachineInstr* Select) const;
  bool canFoldLHS(MachineInstr *Select, MachineInstr *FlagSettingInstr) const;
  bool canFoldRHS(MachineInstr *Select, MachineInstr *FlagSettingInstr) const;
  bool canFoldHelper(MachineInstr *Select,
                                            MachineInstr *FlagSettingInstr,
                                            DenseSet<unsigned> &FoldableOps) const;
  MachineRegisterInfo *MRI;
  const SyncVMInstrInfo *TII;
};

char SyncVMFoldSelect::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMFoldSelect, DEBUG_TYPE, SYNCVM_FOLD_SELECT_NAME,
                false, false)


const MachineInstr* SyncVMFoldSelect::getFlagSettingInstrInSameBB(const MachineInstr* Select) const {
  auto MBB = Select->getParent();
  auto It = Select->getIterator();
  auto Begin = MBB->begin();
  auto End = MBB->end();
  while (It != Begin) {
    --It;
    if (It->definesRegister(SyncVM::Flags))
      return &*It;
  }
  return nullptr;
}

static bool isAfterInSameMBB(MachineInstr* A, MachineInstr* B) {
  if (A->getParent() != B->getParent())
    return false;

  auto MBB = A->getParent();
  auto It = A->getIterator();
  auto Begin = MBB->begin();
  auto End = MBB->end();
  while (It != Begin) {
    --It;
    if (&*It == B)
      return true;
  }
  return false;
}

static DenseSet<unsigned> FoldableLHSOps = {SyncVM::SELrrr, SyncVM::SELrir,
                                            SyncVM::SELrcr, SyncVM::SELrsr,
                                            SyncVM::FATPTR_SELrrr};
static DenseSet<unsigned> FoldableRHSOps = {
    SyncVM::SELrrr, SyncVM::SELrir, SyncVM::SELrcr, SyncVM::SELrsr,
    SyncVM::SELirr, SyncVM::SELcrr, SyncVM::SELsrr, SyncVM::FATPTR_SELrrr};

bool SyncVMFoldSelect::canFoldHelper(MachineInstr *Select,
                                            MachineInstr *FlagSettingInstr,
                                            DenseSet<unsigned> &FoldableOps) const {
  auto Opc = Select->getOpcode();
  // fast prototyping, needs to be refined:
  if (FoldableOps.count(Opc) == 0)
    return false;

  // def of LHS must be after the flag setting instruction
  auto LHSResult = *SyncVM::in0Iterator(*Select);
  assert(LHSResult.isReg() && "LHS must be a register");

  auto LHSDef = MRI->getUniqueVRegDef(LHSResult.getReg());
  if (!LHSDef)
    return false;

  if (!isAfterInSameMBB(Select, FlagSettingInstr))
    return false;

  return true;
}

bool SyncVMFoldSelect::canFoldLHS(MachineInstr *Select,
                                  MachineInstr *FlagSettingInstr) const {
  return canFoldHelper(Select, FlagSettingInstr, FoldableLHSOps);
}

bool SyncVMFoldSelect::canFoldRHS(MachineInstr *Select,
                                  MachineInstr *FlagSettingInstr) const {
  return canFoldHelper(Select, FlagSettingInstr, FoldableRHSOps);
}

bool SyncVMFoldSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM FOLD SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  MRI = &MF.getRegInfo();
  assert(MRI && MRI->isSSA() &&
         "The pass is supposed to be run on SSA form MIR");
  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();

  std::vector<MachineInstr *> FoldedInstrs;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!SyncVM::isSelect(MI))
        continue;

      unsigned Opc = MI.getOpcode();
      DebugLoc DL = MI.getDebugLoc();
      auto In0 = SyncVM::in0Iterator(MI);
      auto In0Range = SyncVM::in0Range(MI);
      auto In1Range = SyncVM::in1Range(MI);
      auto Out = SyncVM::out0Iterator(MI);
      auto CCVal = getImmOrCImm(*SyncVM::ccIterator(MI));

      // if the selct is not foldable on both LHS and RHS, just skip
      /*
      if (!canFoldLHS(Opc) && !canFoldRHS(Opc))
        continue;
      */
    
      // can convert: flag setting instruction must be before the evaluation of



    }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return false;
}

/// createSyncVMFoldPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createSyncVMFoldSelectPass() {
  return new SyncVMFoldSelect();
}
