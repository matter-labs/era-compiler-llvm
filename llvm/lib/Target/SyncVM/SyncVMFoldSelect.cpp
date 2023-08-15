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
  const MachineInstr* getFlagSettingInstrInSameBB(const MachineInstr &Select) const;
  bool canFoldLHS(const MachineInstr *Select, const MachineInstr *FlagSettingInstr) const;
  bool canFoldRHS(const MachineInstr *Select, const MachineInstr *FlagSettingInstr) const;
  bool canFoldHelper(const MachineInstr *Select,
                     MachineInstr::const_mop_iterator Selectee,
                     const MachineInstr *FlagSettingInstr,
                     DenseSet<unsigned> &FoldableOps) const;
  MachineRegisterInfo *MRI;
  const SyncVMInstrInfo *TII;
};

char SyncVMFoldSelect::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMFoldSelect, DEBUG_TYPE, SYNCVM_FOLD_SELECT_NAME,
                false, false)


const MachineInstr* SyncVMFoldSelect::getFlagSettingInstrInSameBB(const MachineInstr &Select) const {
  auto MBB = Select.getParent();
  auto It = Select.getIterator();
  auto Begin = MBB->begin();
  auto End = MBB->end();
  while (It != Begin) {
    --It;
    if (It->definesRegister(SyncVM::Flags))
      return &*It;
  }
  return nullptr;
}

static bool isAfterInSameMBB(const MachineInstr* A, const MachineInstr* B) {
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

bool SyncVMFoldSelect::canFoldHelper(const MachineInstr *Select,
                                     MachineInstr::const_mop_iterator Selectee,
                                     const MachineInstr *FlagSettingInstr,
                                     DenseSet<unsigned> &FoldableOps) const {
  auto Opc = Select->getOpcode();
  // fast prototyping, needs to be refined:
  if (FoldableOps.count(Opc) == 0)
    return false;

  if (!Selectee->isReg())
    return false;

  auto LHSDef = MRI->getUniqueVRegDef(Selectee->getReg());
  if (!LHSDef)
    return false;

  if (!isAfterInSameMBB(Select, FlagSettingInstr))
    return false;

  return true;
}

bool SyncVMFoldSelect::canFoldLHS(const MachineInstr *Select,
                                  const MachineInstr *FlagSettingInstr) const {
  MachineInstr::const_mop_iterator Selectee = SyncVM::in0ConstIterator(*Select);
  return canFoldHelper(Select, Selectee, FlagSettingInstr, FoldableLHSOps);
}

bool SyncVMFoldSelect::canFoldRHS(const MachineInstr *Select,
                                  const MachineInstr *FlagSettingInstr) const {
  MachineInstr::const_mop_iterator Selectee = SyncVM::in1ConstIterator(*Select);
  return canFoldHelper(Select, Selectee, FlagSettingInstr, FoldableRHSOps);
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

      auto FlagSetter = getFlagSettingInstrInSameBB(MI);
      if (!FlagSetter)
        continue;

      // if the selct is not foldable on both LHS and RHS, just skip
      if (!canFoldLHS(&MI, FlagSetter) && !canFoldRHS(&MI, FlagSetter))
        continue;
    
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
