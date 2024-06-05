//===-- EraVMHoistFlagSetting.cpp - Hoist flag setting insts ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements a pass to hoist flag setting instructions above an induction
// variable in loops, in order to remove copy instruction that is introduced
// by a register allocator. The hoisting makes the allocated register for an
// induction variable killed before the next iteration value is computed, so
// the register allocator is able to avoid using a temporary register.
//
// This pass is doing following transformation:
//  loop before:
//    PhiOut         = phi ..., [ IndvarOut, LatchBlock ]
//    IndvarOut      = op  PhiOut, InAny1
//    FlagSettingOut = op  PhiOut, InAny2
//
//  loop after:
//    PhiOut         = phi ..., [ IndvarOut, LatchBlock ]
//    FlagSettingOut = op  PhiOut, InAny2
//    IndvarOut      = op  PhiOut, InAny1
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Support/Debug.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "eravm-hoist-flag-setting"
#define ERAVM_HOIST_FLAG_SETTING_NAME "EraVM hoist flag setting instruction"

namespace {

class EraVMHoistFlagSetting : public MachineFunctionPass {
public:
  static char ID;
  EraVMHoistFlagSetting() : MachineFunctionPass(ID) {
    initializeEraVMHoistFlagSettingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return ERAVM_HOIST_FLAG_SETTING_NAME;
  }

private:
  const MachineRegisterInfo *MRI{};
  const EraVMInstrInfo *TII{};
  /// Hoist flag setting instruction above induction variable in a loop \p L.
  /// In order to do that, loop has to have a header and a latch, and we need to
  /// do the following:
  ///
  ///   1. Find the induction variable. This has to match the following
  ///      induction pattern:
  ///        PhiOut    = phi ..., [ IndvarOut, LatchBlock ]
  ///        IndvarOut = op  PhiOut, InAny
  ///
  ///   2. Find the first flag setting instruction after the induction variable
  ///      which uses output of the phi:
  ///        PhiOut         = phi ..., [ IndvarOut, LatchBlock ]
  ///        FlagSettingOut = op  PhiOut, InAny
  ///
  ///   3. Check if it is safe to hoist the flag setting instruction above the
  ///      induction variable. For this, we need to make sure that there is no
  ///      instructions between that define flag setting uses and instructions
  ///      that manipulate with the flags.
  ///
  ///   4. Hoist the flag setting instruction.
  ///
  bool hoistFlagSettingInsts(MachineLoop *L) const;
};

char EraVMHoistFlagSetting::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(EraVMHoistFlagSetting, DEBUG_TYPE,
                      ERAVM_HOIST_FLAG_SETTING_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(EraVMHoistFlagSetting, DEBUG_TYPE,
                    ERAVM_HOIST_FLAG_SETTING_NAME, false, false)

// Return whether is safe to hoist flag setting instruction above induction
// variable. Check if there are no instructions between that define uses of flag
// setting instruction, or instructions that manipulate with the flags.
static bool isSafeToHoist(const MachineInstr &FlagSetting,
                          const MachineInstr &IndVar) {
  SmallSet<Register, 4> UseRegs;
  // No need to add flag register, since we are explicitly checking for it.
  for (const auto &Op : FlagSetting.uses())
    if (Op.isReg() && Op.getReg() != EraVM::Flags)
      UseRegs.insert(Op.getReg());

  for (const auto &MI :
       make_range(IndVar.getIterator(), FlagSetting.getIterator())) {
    if (MI.isCall())
      return false;

    // Ignore reordering memory operations as it is unlikely to worth the
    // effort, except if both instructions are loading.
    if (MI.mayLoadOrStore() && FlagSetting.mayLoadOrStore() &&
        (!MI.mayLoad() || !FlagSetting.mayLoad()))
      return false;

    // Check if this instruction manipulates with the flags, or defines flag
    // setting uses.
    if (any_of(MI.operands(), [&UseRegs](const MachineOperand &MO) {
          if (!MO.isReg())
            return false;
          if (MO.getReg() == EraVM::Flags)
            return true;
          return MO.isDef() && UseRegs.count(MO.getReg());
        }))
      return false;
  }
  return true;
}

// Return whether this is a valid candidate for an induction variable or for
// an instruction to hoist.
static bool isValidCandidate(const MachineInstr &MI, const EraVMInstrInfo *TII,
                             bool IsIndVar) {
  if (IsIndVar) {
    // Return true for indexed memory operations since we are running this
    // optimization after CombineToIndexedMemops pass, and these instructions
    // might be induction variable.
    switch (MI.getOpcode()) {
    default:
      break;
    case EraVM::LDPI:
    case EraVM::LDMIhr:
    case EraVM::LDMIahr:
    case EraVM::STMIhr:
    case EraVM::STMIahr:
      return true;
    }
  }
  return TII->isArithmetic(MI) || TII->isBitwise(MI);
}

// Find the first flag setting instruction after the induction variable
// that uses output of a phi.
static MachineInstr *findFlagSettingToHoist(MachineInstr &IndVar,
                                            const Register PhiOut,
                                            const EraVMInstrInfo *TII) {
  for (auto &MI : make_range(std::next(IndVar.getIterator()),
                             IndVar.getParent()->instr_end())) {
    if (!EraVMInstrInfo::isFlagSettingInstruction(MI))
      continue;

    // If the first flag setting instruction doesn't read phi output or it is
    // not a valid candidate, don't bother looking for the next one as we won't
    // be able to hoist one flag setting instruction above other one.
    if (!MI.readsRegister(PhiOut) || !isValidCandidate(MI, TII, false))
      return nullptr;
    return &MI;
  }
  return nullptr;
}

// Find the loop controlling induction variable. In order to do that, for
// a given phi instruction find operand that corresponds to the latch block,
// and see if operand is a result of an instruction that uses output of a phi.
// When we find induction variable instruction, return it if we can use it to
// hoist flag setting instruction above it.
static MachineInstr *findIndVar(const MachineInstr &PHI,
                                const MachineBasicBlock &Latch,
                                const MachineRegisterInfo *MRI,
                                const EraVMInstrInfo *TII) {
  for (unsigned I = 1, E = PHI.getNumOperands(); I != E; I += 2) {
    if (PHI.getOperand(I + 1).getMBB() != &Latch)
      continue;

    // On MIR level, PHI operand is always register, even for constants
    // which are propagated through the registers.
    const Register PHIOpReg = PHI.getOperand(I).getReg();
    const Register PHIOutReg = PHI.getOperand(0).getReg();
    MachineInstr *MI = MRI->getUniqueVRegDef(PHIOpReg);
    if (!MI || !MI->readsRegister(PHIOutReg))
      return nullptr;

    // Since we are hoisting flag setting instruction above induction variable,
    // we won't be able to do it if an induction variable manipulates with
    // the flags.
    if (any_of(MI->implicit_operands(), [](const MachineOperand &MO) {
          return MO.isReg() && MO.getReg() == EraVM::Flags;
        }))
      return nullptr;

    if (!isValidCandidate(*MI, TII, true))
      return nullptr;
    return MI;
  }
  return nullptr;
}

bool EraVMHoistFlagSetting::hoistFlagSettingInsts(MachineLoop *L) const {
  auto *Header = L->getHeader();
  auto *Latch = L->getLoopLatch();
  if (!Header || !Latch)
    return false;

  SmallVector<std::pair<MachineInstr *, MachineInstr *>, 4> HoistingCandidates;
  for (auto &PHI : Header->phis())
    if (auto *IndVar = findIndVar(PHI, *Latch, MRI, TII))
      if (auto *FlagSetting =
              findFlagSettingToHoist(*IndVar, PHI.getOperand(0).getReg(), TII))
        HoistingCandidates.emplace_back(IndVar, FlagSetting);

  bool Changed = false;
  for (auto [IndVar, FlagSetting] : HoistingCandidates) {
    if (!isSafeToHoist(*FlagSetting, *IndVar))
      continue;

    auto *MBB = IndVar->getParent();
    assert(FlagSetting->getParent() == MBB &&
           "Instructions are not in the same basic block.");
    LLVM_DEBUG(dbgs() << "======== In basic block: " << MBB->getName() << '\n';
               dbgs() << "   Hoisting instruction:"; FlagSetting->dump();
               dbgs() << "      Above instruction:"; IndVar->dump(););

    MBB->insert(IndVar, FlagSetting->removeFromParent());
    Changed = true;
  }
  return Changed;
}

bool EraVMHoistFlagSetting::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM HOIST FLAG SETTING INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();
  MRI = &MF.getRegInfo();
  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();

  SmallVector<MachineLoop *, 8> Worklist(MLI->begin(), MLI->end());
  while (!Worklist.empty()) {
    MachineLoop *L = Worklist.pop_back_val();
    Changed |= hoistFlagSettingInsts(L);
    Worklist.append(L->begin(), L->end());
  }
  return Changed;
}

FunctionPass *llvm::createEraVMHoistFlagSettingPass() {
  return new EraVMHoistFlagSetting();
}
