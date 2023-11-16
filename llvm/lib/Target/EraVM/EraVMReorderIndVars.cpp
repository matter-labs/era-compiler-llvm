//===-- EraVMReorderIndVars.cpp - Reorder ind vars --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMInstrInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-reorder-indvars"
#define ERAVM_REORDER_IND_VARS_NAME "EraVM reorder indvars"

namespace {

class EraVMReorderIndVars : public MachineFunctionPass {
public:
  static char ID;
  EraVMReorderIndVars() : MachineFunctionPass(ID) {
    initializeEraVMReorderIndVarsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<MachineLoopInfo>();
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return ERAVM_REORDER_IND_VARS_NAME; }

private:
  const MachineRegisterInfo *MRI;
  const MachineDominatorTree *MDT;
  bool reorderIndVars(MachineLoop *L) const;
};

char EraVMReorderIndVars::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(EraVMReorderIndVars, DEBUG_TYPE,
                      ERAVM_REORDER_IND_VARS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(EraVMReorderIndVars, DEBUG_TYPE,
                    ERAVM_REORDER_IND_VARS_NAME, false, false)

static bool isSafeToHoist(const MachineInstr &HoistInst,
                          const MachineInstr &IndVar) {
  SmallSet<Register, 4> UseRegs;
  bool HasFlags = false;
  for (const auto &MO : HoistInst.operands()) {
    if (!MO.isReg())
      continue;

    const Register Reg = MO.getReg();
    if (Reg == EraVM::Flags) {
      HasFlags = true;
      continue;
    }

    if (MO.isUse())
      UseRegs.insert(Reg);
  }

  for (auto &MI : make_range(IndVar.getIterator(), HoistInst.getIterator())) {
    if (MI.isCall() || (MI.mayLoadOrStore() && HoistInst.mayLoadOrStore()))
      return false;

    if (any_of(MI.operands(), [&UseRegs, HasFlags](const MachineOperand &MO) {
          if (!MO.isReg())
            return false;

          const Register Reg = MO.getReg();
          if (HasFlags && Reg == EraVM::Flags)
            return true;

          return MO.isDef() && UseRegs.count(Reg);
        }))
      return false;
  }
  return true;
}

static MachineInstr *
findSinkingPos(MachineInstr &IndVar,
               SmallPtrSetImpl<MachineInstr *> &SinkingPositions) {
  SmallSet<Register, 2> DefRegs;
  for (const auto &MO : IndVar.operands())
    if (MO.isReg() && MO.isDef())
      DefRegs.insert(MO.getReg());

  MachineInstr *SinkPos = nullptr;
  for (auto &MI : make_range(std::next(IndVar.getIterator()),
                             IndVar.getParent()->instr_end())) {
    if (MI.isCall() || (MI.mayLoadOrStore() && IndVar.mayLoadOrStore()))
      break;

    if (any_of(MI.uses(), [&DefRegs](const MachineOperand &MO) {
          return MO.isReg() && DefRegs.count(MO.getReg());
        }))
      break;

    if (!SinkingPositions.count(&MI))
      continue;

    SinkPos = &MI;
    SinkingPositions.erase(&MI);
    if (SinkingPositions.empty())
      break;
  }
  return SinkPos;
}

static bool hoist(MachineInstr &IndVar,
                  SmallPtrSetImpl<MachineInstr *> &HoistingCandidates) {
  bool Changed = false;
  auto *MBB = IndVar.getParent();
  while (!HoistingCandidates.empty()) {
    auto HoistInst =
        std::find_if(std::next(IndVar.getIterator()), MBB->instr_end(),
                     [&HoistingCandidates](const MachineInstr &MI) {
                       return HoistingCandidates.count(&MI);
                     });
    assert(HoistInst != MBB->end() && "No hoisting candidate.");

    HoistingCandidates.erase(&*HoistInst);
    if (!isSafeToHoist(*HoistInst, IndVar))
      continue;

    LLVM_DEBUG(dbgs() << "======== In basic block: " << MBB->getName() << '\n';
               dbgs() << "   Hoisting instruction:"; HoistInst->dump();
               dbgs() << "      Above instruction:"; IndVar.dump(););

    MBB->insert(IndVar, HoistInst->removeFromParent());
    Changed = true;
  }
  return Changed;
}

static bool sink(MachineInstr &IndVar,
                 SmallPtrSetImpl<MachineInstr *> &SinkingPositions) {
  auto *SinkPos = findSinkingPos(IndVar, SinkingPositions);
  if (!SinkPos)
    return false;

  auto *MBB = IndVar.getParent();
  assert(SinkPos->getParent() == MBB &&
         "Instructions are not in the same basic block.");

  LLVM_DEBUG(dbgs() << "======== In basic block: " << MBB->getName() << '\n';
             dbgs() << "    Sinking instruction:"; IndVar.dump();
             dbgs() << "      Below instruction:"; SinkPos->dump(););

  MBB->insertAfter(SinkPos, IndVar.removeFromParent());
  return true;
}

static MachineInstr *findIndVar(const MachineInstr &PHI,
                                const MachineBasicBlock &Latch,
                                const MachineRegisterInfo *MRI) {
  for (unsigned I = 1; I < PHI.getNumOperands(); I += 2) {
    if (PHI.getOperand(I + 1).getMBB() != &Latch)
      continue;

    MachineInstr *MI = MRI->getUniqueVRegDef(PHI.getOperand(I).getReg());
    if (!MI || !MI->readsRegister(PHI.getOperand(0).getReg()))
      return nullptr;

    if (MI->isPHI() || EraVMInstrInfo::isFlagSettingInstruction(*MI))
      return nullptr;
    return MI;
  }
  return nullptr;
}

bool EraVMReorderIndVars::reorderIndVars(MachineLoop *L) const {
  auto *Header = L->getHeader();
  auto *Latch = L->getLoopLatch();
  if (!Header || !Latch)
    return false;

  SmallVector<std::pair<MachineInstr *, SmallPtrSet<MachineInstr *, 4>>, 8>
      ReorderCandidates;
  for (auto &PHI : Header->phis()) {
    auto *IndVar = findIndVar(PHI, *Latch, MRI);
    if (!IndVar)
      continue;

    auto *MBB = IndVar->getParent();
    const Register PHIOutReg = PHI.getOperand(0).getReg();
    SmallPtrSet<MachineInstr *, 4> ReorderPositions;
    for (auto &Use : MRI->use_nodbg_instructions(PHIOutReg))
      if (&Use != IndVar && Use.getParent() == MBB &&
          MDT->dominates(IndVar, &Use))
        ReorderPositions.insert(&Use);

    if (!ReorderPositions.empty())
      ReorderCandidates.emplace_back(IndVar, ReorderPositions);
  }

  bool Changed = false;
  for (auto [IndVar, ReorderPositions] : ReorderCandidates) {
    Changed |= sink(*IndVar, ReorderPositions);
    Changed |= hoist(*IndVar, ReorderPositions);
  }
  return Changed;
}

bool EraVMReorderIndVars::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM REORDER IND VARS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();
  MDT = &getAnalysis<MachineDominatorTree>();
  MRI = &MF.getRegInfo();

  SmallVector<MachineLoop *, 8> Worklist(MLI->begin(), MLI->end());
  while (!Worklist.empty()) {
    MachineLoop *L = Worklist.pop_back_val();
    Changed |= reorderIndVars(L);
    Worklist.append(L->begin(), L->end());
  }
  return Changed;
}

FunctionPass *llvm::createEraVMReorderIndVarsPass() {
  return new EraVMReorderIndVars();
}
