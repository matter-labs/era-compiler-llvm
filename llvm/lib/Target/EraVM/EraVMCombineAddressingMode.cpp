//===-- EraVMCombineAddressingMode.cpp - Combine addr modes -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements a pass that combines instructions into one with more
// sophisticated addressing mode.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-combine-addressing-mode"
#define ERAVM_COMBINE_ADDRESSING_MODE_NAME                                     \
  "EraVM combine instuctions to use complex addressing modes"

static cl::opt<bool>
    EnableEraVMCombineAddressingMode("enable-eravm-combine-addressing-mode",
                                     cl::init(true), cl::Hidden);

STATISTIC(NumReloadFolded, "Number of reloads folded");
STATISTIC(NumSpillsFolded, "Number of spills folded");
STATISTIC(NumLoadConstFolded, "Number of load const folded");

namespace {

class EraVMCombineAddressingMode : public MachineFunctionPass {
public:
  static char ID;
  EraVMCombineAddressingMode() : MachineFunctionPass(ID) {
    initializeEraVMCombineAddressingModePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<ReachingDefAnalysis>();
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return ERAVM_COMBINE_ADDRESSING_MODE_NAME;
  }

private:
  const EraVMInstrInfo *TII{};
  ReachingDefAnalysis *RDA{};
  const MachineDominatorTree *MDT{};
  /// Replace in0 or in1 of \p Base with \p NewArg.
  /// For in1 swap operands as stack addressing mode is only supported for in0,
  /// and if the instruction is not commutable replace the opcode by reversing
  /// `.s` flag.
  /// Put the resulting instruction before \p Base.
  /// ArgNo: must be either in0 or in1.
  void replaceArgument(MachineInstr &Base, EraVM::ArgumentKind ArgNo,
                       iterator_range<MachineInstr::const_mop_iterator> NewArg,
                       unsigned NewOpcode);
  /// Replace an input of Select \p Base corresponding to the register
  /// defined by \p Def with \p In.
  /// Put the resulting instruction before \p Base.
  /// Select is a special pseudo instruction which is not covered by instruction
  /// mappings.
  void mergeSelect(MachineInstr &Base, MachineInstr &Def,
                   iterator_range<MachineInstr::const_mop_iterator> In);
  /// Return whether \p MI is a load const.
  /// I.e. an instruction of form `add const, r0, rN`.
  bool isConstLoad(const MachineInstr &MI) const;
  /// Return whether \p MI is a reload instruction.
  /// I.e. an instruction of form `add stack, r0, rN`.
  bool isReloadInst(const MachineInstr &MI) const;
  /// Return whether \p MI is a spill instruction.
  /// I.e. an instruction of form `add rN, r0, stack`.
  bool isSpillInst(const MachineInstr &MI) const;
  /// Check if \p Reload can be combined with \p Use.
  /// A reload is add stack, r0, rN. The method tries to combine a reload with
  /// all its usages, i.o.w. for each usage it substitutes rN with stack. It's
  /// possible to combine if for all usages
  /// * rN is used in the same BB as reload defines it,
  /// * reload dominates use,
  /// * reload is the only reaching def for rN,
  /// * there is no writes to the reloaded stack slot in between reload and use,
  bool canCombineUse(const MachineInstr &Reload, const MachineInstr &Use,
                     MachineInstr::const_mop_iterator StackDefIt,
                     MachineInstr *ValidRedefitintion = nullptr) const;
  /// Check if \p Def can be combined with \p Spill.
  /// Spill is add rN, r0, stack. The method combines instructions if
  /// * rN is defined in the same BB where it spilled,
  /// * the definition dominates the spill
  /// * there is no writes to the spilled stack slot in between def and spill.
  bool canCombineDef(const MachineInstr &Def, const MachineInstr &Spill) const;
  /// Combine reload and reload-like instructions with their usages.
  bool combineReloadUse(MachineFunction &MF);
  /// Combine spill and spill-like instructions with definitions the register to
  /// spill.
  bool combineDefSpill(MachineFunction &MF);
  /// Combine constant loading with its usage.
  bool combineConstantUse(MachineFunction &MF);
};

char EraVMCombineAddressingMode::ID = 0;
} // namespace

INITIALIZE_PASS_BEGIN(EraVMCombineAddressingMode, DEBUG_TYPE,
                      ERAVM_COMBINE_ADDRESSING_MODE_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(EraVMCombineAddressingMode, DEBUG_TYPE,
                    ERAVM_COMBINE_ADDRESSING_MODE_NAME, false, false)

bool EraVMCombineAddressingMode::isConstLoad(const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::ADDcrr_s &&
         EraVM::in1Iterator(MI)->getReg() == EraVM::R0 &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMCombineAddressingMode::isReloadInst(const MachineInstr &MI) const {
  return (MI.getOpcode() == EraVM::ADDsrr_s ||
          MI.getOpcode() == EraVM::PTR_ADDsrr_s) &&
         // TODO: CPR-1220 Dynamic reload-like instructions can be combined with
         // additional checks.
         !(EraVM::in0Iterator(MI) + 1)->isReg() &&
         EraVM::in1Iterator(MI)->getReg() == EraVM::R0 &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMCombineAddressingMode::isSpillInst(const MachineInstr &MI) const {
  return (MI.getOpcode() == EraVM::ADDrrs_s ||
          MI.getOpcode() == EraVM::PTR_ADDrrs_s) &&
         !(EraVM::out0Iterator(MI) + 1)->isReg() &&
         EraVM::in1Iterator(MI)->getReg() == EraVM::R0 &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

void EraVMCombineAddressingMode::replaceArgument(
    MachineInstr &Base, EraVM::ArgumentKind ArgNo,
    iterator_range<MachineInstr::const_mop_iterator> NewArg,
    unsigned NewOpcode) {
  assert(ArgNo == EraVM::ArgumentKind::In0 ||
         ArgNo == EraVM::ArgumentKind::In1);
  auto NewMI =
      BuildMI(*Base.getParent(), Base, Base.getDebugLoc(), TII->get(NewOpcode));
  EraVM::copyOperands(NewMI, Base.operands_begin(), EraVM::in0Iterator(Base));
  EraVM::copyOperands(NewMI, NewArg);
  EraVM::copyOperands(NewMI, (ArgNo == EraVM::ArgumentKind::In1)
                                 ? EraVM::in0Range(Base)
                                 : EraVM::in1Range(Base));
  EraVM::copyOperands(NewMI, EraVM::in1Iterator(Base) + 1,
                      Base.operands_begin() + Base.getNumExplicitOperands());
}

void EraVMCombineAddressingMode::mergeSelect(
    MachineInstr &Base, MachineInstr &Def,
    iterator_range<MachineInstr::const_mop_iterator> In) {
  // TODO: CPR-1131 Allow to merge twice.
  assert(TII->isSel(Base) && "Expected Select instruction");
  auto *In0It = EraVM::in0Iterator(Base);
  bool IsIn0 =
      In0It->isReg() && In0It->getReg() == EraVM::out0Iterator(Def)->getReg();
  DenseMap<unsigned, unsigned> In0Map = {
      {EraVM::SELrrr, EraVM::SELsrr},
      {EraVM::SELrir, EraVM::SELsir},
      {EraVM::SELrcr, EraVM::SELscr},
      {EraVM::SELrsr, EraVM::SELssr},
  };
  DenseMap<unsigned, unsigned> In1Map = {
      {EraVM::SELrrr, EraVM::SELrsr},
      {EraVM::SELirr, EraVM::SELisr},
      {EraVM::SELcrr, EraVM::SELcsr},
      {EraVM::SELsrr, EraVM::SELssr},
  };
  auto NewMI = BuildMI(
      *Base.getParent(), Base, Base.getDebugLoc(),
      TII->get(IsIn0 ? In0Map[Base.getOpcode()] : In1Map[Base.getOpcode()]));
  EraVM::copyOperands(NewMI, Base.operands_begin(), EraVM::in0Iterator(Base));
  if (IsIn0)
    EraVM::copyOperands(NewMI, In);
  else
    EraVM::copyOperands(NewMI, EraVM::in0Range(Base));
  if (!IsIn0)
    EraVM::copyOperands(NewMI, In);
  else
    EraVM::copyOperands(NewMI, EraVM::in1Iterator(Base),
                        Base.operands_begin() + Base.getNumExplicitOperands() -
                            1);
  NewMI.add(Base.getOperand(Base.getNumExplicitOperands() - 1));
}

static bool areEqualStackSlots(MachineInstr::const_mop_iterator It1,
                               MachineInstr::const_mop_iterator It2) {
  // Assume all dynamic stack slots can be the one.
  if ((It1 + 1)->isReg() || (It2 + 1)->isReg())
    return true;
  // Locals and globals don't intersect, UB otherwise.
  if (It1->isReg() != It2->isReg())
    return false;
  auto Const1 = It1 + 2, Const2 = It2 + 2;
  // If addressing mode is the same, match the constant part.
  if (Const1->isImm() && Const2->isImm())
    return Const1->getImm() == Const2->getImm();
  if (Const1->isCImm() && Const2->isCImm())
    return Const1->getCImm() == Const2->getCImm();
  if (Const1->isSymbol() && Const2->isSymbol())
    return Const1->getMCSymbol() == Const2->getMCSymbol();
  return true;
}

bool EraVMCombineAddressingMode::canCombineUse(
    const MachineInstr &Reload, const MachineInstr &Use,
    MachineInstr::const_mop_iterator StackDefIt,
    MachineInstr *ValidRedefitintion) const {
  Register Def = Reload.getOperand(0).getReg();
  SmallPtrSet<MachineInstr *, 4> ReachingDefs;
  // LLVM doesn't provide const qualified version of the method, yet the code
  // below doesn't modify anything.
  RDA->getGlobalReachingDefs(const_cast<MachineInstr *>(&Use), Def,
                             ReachingDefs);
  // It's expected that if there are more than one reaching def,
  // it's very unlikely that all of them are reloads from the same
  // stack slots which is required to do the transformation for
  // multiple reloads per single use.
  if (ReachingDefs.size() != 1)
    return false;
  // TODO: CPR-1225 All instructions in between reload and use
  // must not redefine the loaded stack slot.
  // The restriction is to simplify this check.
  if (Use.getParent() != Reload.getParent() || !MDT->dominates(&Reload, &Use))
    return false;
  // TODO: CPR-1224 If to check it globally it surprisingly fails
  assert(*ReachingDefs.begin() == &Reload);
  // There are no stack slot redefinitions in between reload and
  // use.
  if (std::any_of(std::next(Reload.getIterator()), Use.getIterator(),
                  [StackDefIt, ValidRedefitintion](const MachineInstr &CurMI) {
                    if (!EraVM::hasSROutAddressingMode(CurMI))
                      return false;
                    if (ValidRedefitintion == &CurMI)
                      return false;
                    return areEqualStackSlots(StackDefIt,
                                              EraVM::out0Iterator(CurMI));
                  }))
    return false;
  // Select isn't in addr mode maps, and doesn't restricted with
  // RR input addressing mode, handle it separately.
  if (TII->isSel(Use)) {
    const MachineOperand &FirstOpnd = *EraVM::in0Iterator(Use);
    const MachineOperand &SecondOpnd = *EraVM::in1Iterator(Use);
    return (FirstOpnd.isReg() && FirstOpnd.getReg() == Def) ||
           (SecondOpnd.isReg() && SecondOpnd.getReg() == Def);
  }
  // In case use defines a stack slot, disallow to read from it
  // in between reload and use, otherwise the pattern S1 -> R1,
  // S2 -> R2, R2 -> S1, R1 -> S2 would have been folded to
  // S2 -> S1, S1 -> S2.
  // TODO: CPR-1132
  // Read restriction is an artifact of 2 phase architecture of
  // the pass and can be relaxed to the must not be other combines
  // with the def-stack(use) in between reload and use.
  if (EraVM::hasSROutAddressingMode(Use))
    if (std::any_of(std::next(Reload.getIterator()), Use.getIterator(),
                    [&Use](const MachineInstr &CurMI) {
                      if (!EraVM::hasSRInAddressingMode(CurMI))
                        return false;
                      return areEqualStackSlots(EraVM::in0Iterator(CurMI),
                                                EraVM::out0Iterator(Use));
                    }))
      return false;
  if (!EraVM::hasRRInAddressingMode(Use))
    return false;
  // After IfConversion is done, some uses could be predicated,
  // thus require more in-depth analysis to combine. Ignore such
  // cases as unlikely to worth the effort.
  if (getImmOrCImm(*EraVM::ccIterator(Use)) != EraVMCC::COND_NONE)
    return false;
  // Can't use the same stack slot twice because stack-stack is
  // not a valid addressing mode.
  if (EraVM::in0Iterator(Use)->getReg() == Def &&
      EraVM::in1Iterator(Use)->getReg() != Def)
    return true;
  if ((Use.isCommutable() ||
       EraVM::getWithInsSwapped(EraVM::getWithSRInAddrMode(Use.getOpcode())) !=
           -1) &&
      EraVM::in0Iterator(Use)->getReg() != Def &&
      EraVM::in1Iterator(Use)->getReg() == Def)
    return true;
  return false;
}

bool EraVMCombineAddressingMode::canCombineDef(
    const MachineInstr &Def, const MachineInstr &Spill) const {
  assert(isSpillInst(Spill));
  Register Spilled = EraVM::in0Iterator(Spill)->getReg();
  // Check if addressing mode can be changed to put the definition on stack.
  // TODO: CPR-1259 support select as the definition.
  if (!EraVM::hasRROutAddressingMode(Def) ||
      EraVM::out0Iterator(Def)->getReg() != Spilled ||
      (Def.getNumExplicitDefs() == 2 &&
       EraVM::out1Iterator(Def)->getReg() == Spilled))
    return false;
  // After IfConversion Def could be predicated, thus require more in-depth
  // analysis to combine. Ignore such cases as unlikely to worth the effort.
  if (getImmOrCImm(*EraVM::ccIterator(Def)) != EraVMCC::COND_NONE)
    return false;
  // TODO: CPR-1225 Make transformation global.
  if (Def.getParent() != Spill.getParent() || !MDT->dominates(&Def, &Spill))
    return false;
  // No def or use of the spilled stack slot is expected in between Def and
  // Spill.
  if (std::any_of(std::next(Def.getIterator()), Spill.getIterator(),
                  [&Spill](const MachineInstr &CurMI) {
                    bool ClobberedStackSlot = false;
                    if (EraVM::hasSROutAddressingMode(CurMI))
                      ClobberedStackSlot |=
                          areEqualStackSlots(EraVM::out0Iterator(Spill),
                                             EraVM::out0Iterator(CurMI));
                    if (EraVM::hasSRInAddressingMode(CurMI))
                      ClobberedStackSlot |=
                          areEqualStackSlots(EraVM::out0Iterator(Spill),
                                             EraVM::in0Iterator(CurMI));
                    return ClobberedStackSlot;
                  }))
    return false;
  return true;
}

bool EraVMCombineAddressingMode::combineReloadUse(MachineFunction &MF) {
  std::vector<std::pair<MachineInstr *, SmallPtrSet<MachineInstr *, 4>>>
      Deleted;
  DenseSet<MachineInstr *> UsesToUpdate;
  // The pass consists of two phases to not to recompute RDA on every single
  // rewrite.
  // 1. Collect all instructions to be combined.
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (!isReloadInst(MI))
        continue;
      Register Def = MI.getOperand(0).getReg();
      SmallPtrSet<MachineInstr *, 4> Uses;
      RDA->getGlobalUses(&MI, Def, Uses);
      // Combining is only profitable if all uses can be rewritten, i.e. can be
      // combined according to available addressing modes, and not yet pending
      // to be combined. When all uses are updated to use stack instead of
      // the register defined by a reload instruction, the reload can be erased.
      if (any_of(Uses, [&UsesToUpdate](const MachineInstr *Use) {
            // Already pending to combine. Can't combine twice.
            // TODO: CPR-1131 Select is the exception here.
            return UsesToUpdate.count(Use);
          }))
        continue;
      if (!all_of(Uses, [this, &MI](const MachineInstr *Use) {
            return canCombineUse(MI, *Use, EraVM::in0Iterator(MI));
          }))
        continue;
      UsesToUpdate.insert(Uses.begin(), Uses.end());
      Deleted.emplace_back(&MI, Uses);
    }
  }

  // 2. Combine.
  for (auto [Reload, Uses] : Deleted) {
    for (auto *Use : Uses) {
      if (EraVM::isSelect(*Use)) {
        mergeSelect(*Use, *Reload, EraVM::in0Range(*Reload));
      } else {
        if (EraVM::in0Iterator(*Use)->getReg() ==
            Reload->getOperand(0).getReg()) {
          replaceArgument(*Use, EraVM::ArgumentKind::In0,
                          EraVM::in0Range(*Reload),
                          EraVM::getWithSRInAddrMode(Use->getOpcode()));
        } else {
          int NewOpcode = EraVM::getWithSRInAddrMode(Use->getOpcode());
          if (!Use->isCommutable())
            NewOpcode = EraVM::getWithInsSwapped(NewOpcode);
          assert(NewOpcode != -1);
          replaceArgument(*Use, EraVM::ArgumentKind::In1,
                          EraVM::in0Range(*Reload), NewOpcode);
        }
      }
      LLVM_DEBUG(dbgs() << "== Combine reload"; Reload->dump();
                 dbgs() << "   and use:"; Use->dump(); dbgs() << " into:";
                 std::prev(Use->getIterator())->dump(););
      Use->eraseFromParent();
    }
    Reload->eraseFromParent();
  }

  NumReloadFolded += Deleted.size();

  return !Deleted.empty();
}

bool EraVMCombineAddressingMode::combineDefSpill(MachineFunction &MF) {
  std::vector<std::tuple<MachineInstr *, MachineInstr *,
                         SmallPtrSet<MachineInstr *, 4>>>
      Deleted;
  // An instruction might have potential to combine twice by in0 and in1, but
  // addressing modes disallow two stack in operands, so keep track uses that
  // are pending to be combined to prevent second combining attempt.
  DenseSet<MachineInstr *> UsesToUpdate;
  // The pass consists of two phases to not to recompute RDA on every single
  // rewrite.
  // 1. Collect all instructions to be combined.
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (!isSpillInst(MI))
        continue;
      Register Spilled = EraVM::in0Iterator(MI)->getReg();
      SmallPtrSet<MachineInstr *, 4> ReachingDefs;
      RDA->getGlobalReachingDefs(&MI, Spilled, ReachingDefs);
      // TODO: CPR-1225 While the transformation is local, multiple reaching def
      // is unexpected.
      if (ReachingDefs.size() != 1U)
        continue;
      SmallPtrSet<MachineInstr *, 4> DefUses;
      MachineInstr *DefMI = *ReachingDefs.begin();
      if (UsesToUpdate.count(DefMI) || UsesToUpdate.count(&MI) ||
          !canCombineDef(*DefMI, MI))
        continue;
      RDA->getGlobalUses(DefMI, Spilled, DefUses);

      if (!all_of(DefUses, [&MI, DefMI, &UsesToUpdate,
                            this](const MachineInstr *DefUse) {
            if (UsesToUpdate.count(DefUse))
              return false;
            if (DefUse == &MI)
              return true;
            return canCombineUse(*DefMI, *DefUse, EraVM::in0Iterator(MI), &MI);
          }))
        continue;

      // TODO: CPR-1224 If to check it globally it surprisingly fails
      assert(find(DefUses, &MI) != DefUses.end() &&
             "The spill is expected to be among uses");

      Deleted.emplace_back(DefMI, &MI, DefUses);
      UsesToUpdate.insert(DefUses.begin(), DefUses.end());
      UsesToUpdate.insert(DefMI);
    }
  }

  // 2. Combine.
  for (auto [Def, Spill, Uses] : Deleted) {
    // Rewrite the definition, so it defins the stack slot, thus allowing to
    // remove the spill.
    int NewOpcode = EraVM::getWithSROutAddrMode(Def->getOpcode());
    assert(NewOpcode != -1);
    MachineInstrBuilder NewMI;
    if (Def->getNumExplicitDefs() == 2U)
      NewMI = BuildMI(*Def->getParent(), *Def, Def->getDebugLoc(),
                      TII->get(NewOpcode), EraVM::out1Iterator(*Def)->getReg());
    else
      NewMI = BuildMI(*Def->getParent(), *Def, Def->getDebugLoc(),
                      TII->get(NewOpcode));

    EraVM::copyOperands(NewMI, EraVM::in0Iterator(*Def),
                        EraVM::in1Iterator(*Def) + 1);
    EraVM::copyOperands(NewMI, EraVM::out0Range(*Spill));
    EraVM::copyOperands(NewMI, EraVM::in1Iterator(*Def) + 1,
                        Def->operands_begin() + Def->getNumExplicitOperands());
    LLVM_DEBUG(dbgs() << "== Combine def"; Def->dump();
               dbgs() << "   and spill:"; Spill->dump(); dbgs() << "   into:";
               NewMI->dump(););
    for (MachineInstr *Use : Uses) {
      // Rewrite uses so the use the stack slot as an input insted of the
      // register previously defined.
      if (Use == Spill)
        continue;
      if (TII->isSel(*Use)) {
        mergeSelect(*Use, *Def, EraVM::out0Range(*Spill));
      } else {
        if (EraVM::in0Iterator(*Use)->getReg() == Def->getOperand(0).getReg())
          replaceArgument(*Use, EraVM::ArgumentKind::In0,
                          EraVM::out0Range(*Spill),
                          EraVM::getWithSRInAddrMode(Use->getOpcode()));
        else {
          int NewOpcode = EraVM::getWithSRInAddrMode(Use->getOpcode());
          if (!Use->isCommutable())
            NewOpcode = EraVM::getWithInsSwapped(NewOpcode);
          assert(NewOpcode != -1);
          replaceArgument(*Use, EraVM::ArgumentKind::In1,
                          EraVM::out0Range(*Spill), NewOpcode);
        }
      }
      LLVM_DEBUG(dbgs() << "== Replace use"; Use->dump(); dbgs() << "   with:";
                 std::prev(Use->getIterator())->dump(););
    }
  }
  for (auto [Def, Spill, Uses] : Deleted) {
    Def->eraseFromParent();
    for (auto *Use : Uses)
      Use->eraseFromParent();
  }
  NumSpillsFolded += Deleted.size();
  return !Deleted.empty();
}

bool EraVMCombineAddressingMode::combineConstantUse(MachineFunction &MF) {
  std::vector<std::pair<MachineInstr *, SmallPtrSet<MachineInstr *, 4>>>
      Deleted;
  DenseSet<MachineInstr *> UsesToUpdate;
  // The pass consists of two phases to not to recompute RDA on every single
  // rewrite.
  // 1. Collect all instructions to be combined.
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (!isConstLoad(MI))
        continue;
      Register Def = MI.getOperand(0).getReg();
      SmallPtrSet<MachineInstr *, 4> Uses;
      RDA->getGlobalUses(&MI, Def, Uses);
      // Combining is only profitable if all uses can be rewritten.
      if (any_of(Uses, [&UsesToUpdate](const MachineInstr *Use) {
            // Already pending to combine. Can't combine twice.
            // TODO: CPR-1131 Select is the exception here.
            return UsesToUpdate.count(Use);
          }))
        continue;
      if (!all_of(Uses, [this, &MI, Def](MachineInstr *Use) {
            SmallPtrSet<MachineInstr *, 4> ReachingDefs;
            RDA->getGlobalReachingDefs(Use, Def, ReachingDefs);
            if (ReachingDefs.size() != 1U)
              return false;
            assert(*ReachingDefs.begin() == &MI && "RDA is broken");
            if (TII->getCCCode(*Use) != EraVMCC::COND_NONE ||
                !MDT->dominates(&MI, Use))
              return false;
            // TODO: CPR-1499 Support select.
            return !TII->isSel(*Use) && EraVM::hasRRInAddressingMode(*Use);
          }))
        continue;
      UsesToUpdate.insert(Uses.begin(), Uses.end());
      Deleted.emplace_back(&MI, Uses);
    }
  }

  // 2. Combine.
  for (auto [LoadConst, Uses] : Deleted) {
    for (auto *Use : Uses) {
      if (EraVM::in0Iterator(*Use)->getReg() ==
          LoadConst->getOperand(0).getReg()) {
        replaceArgument(*Use, EraVM::ArgumentKind::In0,
                        EraVM::in0Range(*LoadConst),
                        EraVM::getWithCRInAddrMode(Use->getOpcode()));
      } else {
        int NewOpcode = EraVM::getWithCRInAddrMode(Use->getOpcode());
        if (!Use->isCommutable())
          NewOpcode = EraVM::getWithInsSwapped(NewOpcode);
        assert(NewOpcode != -1);
        replaceArgument(*Use, EraVM::ArgumentKind::In1,
                        EraVM::in0Range(*LoadConst), NewOpcode);
      }
      LLVM_DEBUG(dbgs() << "== Combine load const"; LoadConst->dump();
                 dbgs() << "   and use:"; Use->dump(); dbgs() << " into:";
                 std::prev(Use->getIterator())->dump(););
      Use->eraseFromParent();
    }
    LoadConst->eraseFromParent();
  }

  NumLoadConstFolded += Deleted.size();

  return !Deleted.empty();
}

bool EraVMCombineAddressingMode::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM COMBINE ADDRESSING MODE **********\n"
                    << "********** Function: " << MF.getName() << '\n');
  bool Changed = false;
  if (!EnableEraVMCombineAddressingMode)
    return false;
  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  MDT = &getAnalysis<MachineDominatorTree>();
  RDA = &getAnalysis<ReachingDefAnalysis>();
  Changed |= combineReloadUse(MF);
  RDA->reset();
  Changed |= combineDefSpill(MF);
  RDA->reset();
  Changed |= combineConstantUse(MF);
  return Changed;
}

FunctionPass *llvm::createEraVMCombineAddressingModePass() {
  return new EraVMCombineAddressingMode();
}
