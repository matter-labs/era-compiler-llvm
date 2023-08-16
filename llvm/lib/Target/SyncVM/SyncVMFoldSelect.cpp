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
  MachineInstr*
  getFoldableLHS(MachineInstr *Select,
                 const MachineInstr *FlagSettingInstr) const;
  MachineInstr*
  getFoldableRHS(MachineInstr *Select,
                 const MachineInstr *FlagSettingInstr) const;

  MachineInstr* getFoldableInstr(MachineInstr *Select,
                     MachineInstr::const_mop_iterator Selectee,
                     const MachineInstr *FlagSettingInstr,
                     DenseSet<unsigned> &FoldableOps) const;

  MachineInstr* pickFoldingCandidate(MachineInstr* LHS,
                                    MachineInstr* RHS) const;
  void updateCC(MachineInstr* Candidate, unsigned CCVal);
  
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
  while (It != Begin) {
    --It;
    if (It->definesRegister(SyncVM::Flags))
      return &*It;
  }
  return nullptr;
}

// TODO: this can be combined with above
static bool isAfterInSameMBB(const MachineInstr* A, const MachineInstr* B) {
  if (A->getParent() != B->getParent())
    return false;

  auto MBB = A->getParent();
  auto It = A->getIterator();
  auto Begin = MBB->begin();
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

MachineInstr* SyncVMFoldSelect::getFoldableInstr(MachineInstr *Select,
                                     MachineInstr::const_mop_iterator Selectee,
                                     const MachineInstr *FlagSettingInstr,
                                     DenseSet<unsigned> &FoldableOps) const {
  auto Opc = Select->getOpcode();
  // fast prototyping, needs to be refined:
  if (FoldableOps.count(Opc) == 0)
    return nullptr;

  if (!Selectee->isReg())
    return nullptr;

  MachineInstr* SelecteeDef = MRI->getUniqueVRegDef(Selectee->getReg());

  // we need to make sure the selectee is predicable and has COND_NONE
  // as its condition code.
  if (!SelecteeDef)
    return nullptr;
  
  if (!TII->isPredicable(*SelecteeDef))
    return nullptr;

  auto CCIt = SyncVM::ccIterator(*SelecteeDef);
  if (getImmOrCImm(*CCIt) != SyncVMCC::COND_NONE)
    return nullptr;

  return SelecteeDef;
}

MachineInstr*
SyncVMFoldSelect::getFoldableLHS(MachineInstr *Select,
                                 const MachineInstr *FlagSettingInstr) const {
  MachineInstr::mop_iterator Selectee = SyncVM::in0Iterator(*Select);
  return getFoldableInstr(Select, Selectee, FlagSettingInstr, FoldableLHSOps);
}

MachineInstr*
SyncVMFoldSelect::getFoldableRHS(MachineInstr *Select,
                                 const MachineInstr *FlagSettingInstr) const {
  MachineInstr::mop_iterator Selectee = SyncVM::in1Iterator(*Select);
  return getFoldableInstr(Select, Selectee, FlagSettingInstr, FoldableRHSOps);
}

MachineInstr* SyncVMFoldSelect::pickFoldingCandidate(MachineInstr* LHS,
                                                    MachineInstr* RHS) const {
  if (LHS && RHS) {
    if (isAfterInSameMBB(&*LHS, &*RHS))
      return &*LHS;
    else
      return &*RHS;
  } else if (LHS)
    return &*LHS;
  else if (RHS)
    return &*RHS;
  else
    return nullptr;
}

void SyncVMFoldSelect::updateCC(MachineInstr* Candidate, unsigned CCVal) {
  // Precondition:
  // 1. Candidate is a valid folding candidate.
  // 2. Candidate is predicable and has COND_NONE as its condition code.
  // 3. Candidate is in the same basic block as the select instruction.
  // 4. Candidate is closest to the select instruction, and after flag setting.
  
  auto CandidateCCIt = SyncVM::ccIterator(*Candidate);
  CandidateCCIt->setImm(CCVal);
}

bool SyncVMFoldSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM FOLD SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  MRI = &MF.getRegInfo();
  assert(MRI && MRI->isSSA() &&
         "The pass is supposed to be run on SSA form MIR");
  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();

  std::vector<MachineInstr *> FoldedSelects;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!SyncVM::isSelect(MI))
        continue;

      auto CCVal = getImmOrCImm(*SyncVM::ccIterator(MI));

      auto FlagSetter = getFlagSettingInstrInSameBB(MI);
      if (!FlagSetter)
        continue;

      auto LHS = getFoldableLHS(&MI, FlagSetter);
      auto RHS = getFoldableRHS(&MI, FlagSetter);
    
      // For now, we respect the order of the code so we need to
      // find which one (LHS or RHS) is after the other and fold it.
      auto Candidate = pickFoldingCandidate(LHS, RHS);
      if (!Candidate)
        continue;
    
      // The candidate must appear after the flag setter
      if (!isAfterInSameMBB(Candidate, FlagSetter))
        continue;
    
      // if we are trying to fold with the RHS, we need to reverse CC
      bool ShouldReverseCC = (LHS != Candidate);
      if (ShouldReverseCC) {
        auto OptReversedCC =
            TII->getReversedCondition((SyncVMCC::CondCodes)CCVal);
        if (!OptReversedCC)
          continue;
        CCVal = *OptReversedCC;
      }

      updateCC(Candidate, CCVal);
      FoldedSelects.push_back(&MI);
    }

  for (auto *I : FoldedSelects)
    I->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !FoldedSelects.empty();
}

/// createSyncVMFoldPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createSyncVMFoldSelectPass() {
  return new SyncVMFoldSelect();
}
