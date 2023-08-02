//===---- SyncVMCombineReloadUse.cpp - Combine reloads with its uses ------===//
//
/// \file
/// Implements a pass that combines a reload or reload-like stack access with
/// its uses.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMInstrInfo.h"
#include "SyncVMSubtarget.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-combine-reload-use"
#define SYNCVM_COMBINE_RELOAD_USE_NAME "SyncVM combine reloads with uses"

static cl::opt<bool>
    EnableSyncVMCombineReloadUse("enable-syncvm-combine-reload-use",
                                 cl::init(true), cl::Hidden);

STATISTIC(NumReloadFolded, "Number of foldings done");

namespace {

/// Combine reload and reload-like instructions with their usages.
/// A reload is add stack, r0, rN. The pass tries to combine a reload with all
/// its usages, i.o.w. for each usage it substitutes rN with stack. It's
/// possible to combine if for all usages
/// * rN is used in the same BB as reload defines it,
/// * reload dominates use,
/// * reload is the only reaching def for rN,
/// * there is no writes to the reloaded stack slot in between reload and use,
/// Additionally if use writes to stack
class SyncVMCombineReloadUse : public MachineFunctionPass {
public:
  static char ID;
  SyncVMCombineReloadUse() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<ReachingDefAnalysis>();
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return SYNCVM_COMBINE_RELOAD_USE_NAME;
  }

private:
  const SyncVMInstrInfo *TII;
  ReachingDefAnalysis *RDA;
  const MachineDominatorTree *MDT;
  /// Replace in0 of \p Use with in0 of \p Reload.
  /// Put the resulting instruction after \p Use.
  void merge(MachineInstr &Reload, MachineInstr &Use);
  /// Swap in0 and in1, replace in1 of \p Use with in0 of \p Reload.
  /// For a non-commutable instruction, replace the opcode revering `.s` flag.
  /// Put the resulting instruction after \p Use.
  void mergeSwap(MachineInstr &Reload, MachineInstr &Use);
  /// Replace in0 of Select \p Use with in0 of \p Reload.
  /// Put the resulting instruction after \p Use.
  /// Select is a special pseudo instruction which is not covered by instruction
  /// mappings.
  void mergeSelect(MachineInstr &Reload, MachineInstr &Use);
  /// Return whether \p MI is a reload instruction.
  /// I.e. an instruction of form `add stack, r0, rN`.
  bool isReloadInst(MachineInstr &MI) const;
  /// Main function of the pass.
  bool combineReloadUse(MachineFunction &MF);
};

char SyncVMCombineReloadUse::ID = 0;
} // namespace

INITIALIZE_PASS_BEGIN(SyncVMCombineReloadUse, DEBUG_TYPE,
                      SYNCVM_COMBINE_RELOAD_USE_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(SyncVMCombineReloadUse, DEBUG_TYPE,
                    SYNCVM_COMBINE_RELOAD_USE_NAME, false, false)

bool SyncVMCombineReloadUse::isReloadInst(MachineInstr &MI) const {
  return (MI.getOpcode() == SyncVM::ADDsrr_s ||
          MI.getOpcode() == SyncVM::PTR_ADDsrr_s) &&
         // TODO: CPR-1220 Dynamic reload-like instructions can be combined with
         // additional checks.
         !(SyncVM::in0Iterator(MI) + 1)->isReg() &&
         SyncVM::in1Iterator(MI)->getReg() == SyncVM::R0 &&
         TII->getCCCode(MI) == SyncVMCC::COND_NONE;
}

void SyncVMCombineReloadUse::merge(MachineInstr &Reload, MachineInstr &Use) {
  int NewOpcode = SyncVM::getWithSRInAddrMode(Use.getOpcode());
  auto NewMI =
      BuildMI(*Use.getParent(), Use, Use.getDebugLoc(), TII->get(NewOpcode));
  SyncVM::copyOperands(NewMI, Use.operands_begin(), SyncVM::in0Iterator(Use));
  SyncVM::copyOperands(NewMI, SyncVM::in0Range(Reload));
  SyncVM::copyOperands(NewMI, SyncVM::in0Iterator(Use) + 1,
                       Use.operands_begin() + Use.getNumExplicitOperands());
  LLVM_DEBUG(dbgs() << "== Combine reload"; Reload.dump();
             dbgs() << "   and use:"; Use.dump(); dbgs() << "   into:";
             NewMI->dump(););
}

void SyncVMCombineReloadUse::mergeSwap(MachineInstr &Reload,
                                       MachineInstr &Use) {
  int NewOpcode = SyncVM::getWithSRInAddrMode(Use.getOpcode());
  if (!Use.isCommutable())
    NewOpcode = SyncVM::getWithInsSwapped(NewOpcode);
  assert(NewOpcode != -1);
  auto NewMI =
      BuildMI(*Use.getParent(), Use, Use.getDebugLoc(), TII->get(NewOpcode));
  SyncVM::copyOperands(NewMI, Use.operands_begin(), SyncVM::in0Iterator(Use));
  SyncVM::copyOperands(NewMI, SyncVM::in0Range(Reload));
  NewMI.add(*SyncVM::in0Iterator(Use));
  SyncVM::copyOperands(NewMI, SyncVM::in1Iterator(Use) + 1,
                       Use.operands_begin() + Use.getNumExplicitOperands());
  LLVM_DEBUG(dbgs() << "== Combine reload"; Reload.dump();
             dbgs() << "   and use:"; Use.dump(); dbgs() << "   into:";
             NewMI->dump(););
}

void SyncVMCombineReloadUse::mergeSelect(MachineInstr &Reload,
                                         MachineInstr &Use) {
  // TODO: CPR-1131 Allow to merge twice.
  assert(TII->isSel(Use) && "Expected Select instruction");
  auto In0It = SyncVM::in0Iterator(Use);
  bool IsIn0 = In0It->isReg() &&
               In0It->getReg() == SyncVM::out0Iterator(Reload)->getReg();
  DenseMap<unsigned, unsigned> In0Map = {
      {SyncVM::SELrrr, SyncVM::SELsrr},
      {SyncVM::SELrir, SyncVM::SELsir},
      {SyncVM::SELrcr, SyncVM::SELscr},
      {SyncVM::SELrsr, SyncVM::SELssr},
  };
  DenseMap<unsigned, unsigned> In1Map = {
      {SyncVM::SELrrr, SyncVM::SELrsr},
      {SyncVM::SELirr, SyncVM::SELisr},
      {SyncVM::SELcrr, SyncVM::SELcsr},
      {SyncVM::SELsrr, SyncVM::SELssr},
  };
  auto NewMI = BuildMI(
      *Use.getParent(), Use, Use.getDebugLoc(),
      TII->get(IsIn0 ? In0Map[Use.getOpcode()] : In1Map[Use.getOpcode()]));
  SyncVM::copyOperands(NewMI, Use.operands_begin(), SyncVM::in0Iterator(Use));
  if (IsIn0)
    SyncVM::copyOperands(NewMI, SyncVM::in0Range(Reload));
  else
    SyncVM::copyOperands(NewMI, SyncVM::in0Range(Use));
  if (!IsIn0)
    SyncVM::copyOperands(NewMI, SyncVM::in0Range(Reload));
  else
    SyncVM::copyOperands(NewMI, SyncVM::in1Iterator(Use),
                         Use.operands_begin() + Use.getNumExplicitOperands() -
                             1);
  NewMI.add(Use.getOperand(Use.getNumExplicitOperands() - 1));
  LLVM_DEBUG(dbgs() << "== Combine reload"; Reload.dump();
             dbgs() << "   and use:"; Use.dump(); dbgs() << "   into:";
             NewMI->dump(););
}

static bool areEqualStackSlots(MachineInstr::mop_iterator It1,
                               MachineInstr::mop_iterator It2) {
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

bool SyncVMCombineReloadUse::combineReloadUse(MachineFunction &MF) {
  std::vector<std::pair<MachineInstr *, SmallPtrSet<MachineInstr *, 4>>>
      Deleted;
  // An instruction might have potential to combine twice by in0 and in1, but
  // addressing modes disallow two stack in operands, so keep track uses that
  // are pending to be combined to prevent second combining attempt.
  DenseSet<MachineInstr *> UsesToUpdate;
  // The pass consists of two phases because to not to recompute RDA on every
  // single rewrite.
  // 1. Collect all instructions to be combined.
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (!isReloadInst(MI))
        continue;
      Register Def = MI.getOperand(0).getReg();
      SmallPtrSet<MachineInstr *, 4> Uses;
      RDA->getGlobalUses(&MI, Def, Uses);
      if (std::all_of(
              Uses.begin(), Uses.end(),
              [this, Def, &MI, &UsesToUpdate](MachineInstr *Use) {
                SmallPtrSet<MachineInstr *, 4> ReachingDefs;
                RDA->getGlobalReachingDefs(Use, Def, ReachingDefs);
                // It's expected that if there are more then one reaching def,
                // it's very unlikely that all of them are reloads from the same
                // stack slots which is required to do the transformation for
                // multiple reloads per single use.
                if (ReachingDefs.size() != 1)
                  return false;
                // TODO: CPR-1225 All instructions in between reload and use
                // must not redefine the loaded stack slot.
                // The restriction is to simplify this check.
                if (Use->getParent() != MI.getParent() ||
                    !MDT->dominates(&MI, Use))
                  return false;
                // TODO: CPR-1224 If to check it globally it surprisingly fails
                assert(*ReachingDefs.begin() == &MI);
                if (UsesToUpdate.count(Use) != 0)
                  // Already pending to combine. Can't combine twice.
                  // TODO: CPR-1131 Select is the exception here.
                  return false;
                // There are no stack slot redefinitions in between reload and
                // use.
                if (std::any_of(std::next(MI.getIterator()), Use->getIterator(),
                                [&MI](MachineInstr &CurMI) {
                                  if (!SyncVM::hasSROutAddressingMode(CurMI))
                                    return false;
                                  return areEqualStackSlots(
                                      SyncVM::in0Iterator(MI),
                                      SyncVM::out0Iterator(CurMI));
                                }))
                  return false;
                // Select isn't in addr mode maps, and doesn't restricted with
                // RR input addressing mode, handle it separately.
                if (TII->isSel(*Use)) {
                  MachineOperand &FirstOpnd = *SyncVM::in0Iterator(*Use);
                  MachineOperand &SecondOpnd = *SyncVM::in1Iterator(*Use);
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
                if (SyncVM::hasSROutAddressingMode(*Use))
                  if (std::any_of(std::next(MI.getIterator()),
                                  Use->getIterator(),
                                  [Use](MachineInstr &CurMI) {
                                    if (!SyncVM::hasSRInAddressingMode(CurMI))
                                      return false;
                                    return areEqualStackSlots(
                                        SyncVM::in0Iterator(CurMI),
                                        SyncVM::out0Iterator(*Use));
                                  }))
                    return false;
                if (!SyncVM::hasRRInAddressingMode(*Use))
                  return false;
                // After IfConversion is done, some uses could be predicated,
                // thus require more in-depth analysis to combine. Ignore such
                // cases as unlikely to worth the effort.
                if (getImmOrCImm(*SyncVM::ccIterator(*Use)) != SyncVMCC::COND_NONE)
                  return false;
                // Can't use the same stack slot twice because stack-stack is
                // not a valid addressing mode with an unpredicated reload.
                if (SyncVM::in0Iterator(*Use)->getReg() == Def &&
                    SyncVM::in1Iterator(*Use)->getReg() != Def)
                  return true;
                if ((Use->isCommutable() ||
                     SyncVM::getWithInsSwapped(SyncVM::getWithSRInAddrMode(
                         Use->getOpcode())) != -1) &&
                    SyncVM::in0Iterator(*Use)->getReg() != Def &&
                    SyncVM::in1Iterator(*Use)->getReg() == Def)
                  return true;
                return false;
              })) {
        UsesToUpdate.insert(Uses.begin(), Uses.end());
        Deleted.push_back({&MI, Uses});
      }
    }
  }

  // 2. Combine.
  for (auto [Reload, Uses] : Deleted) {
    for (auto Use : Uses) {
      if (TII->isSel(*Use)) {
        mergeSelect(*Reload, *Use);
      } else {
        if (SyncVM::in0Iterator(*Use)->getReg() ==
            Reload->getOperand(0).getReg())
          merge(*Reload, *Use);
        else
          mergeSwap(*Reload, *Use);
      }
      Use->eraseFromParent();
    }
    Reload->eraseFromParent();
  }

  NumReloadFolded += Deleted.size();

  return !Deleted.empty();
}

bool SyncVMCombineReloadUse::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM COMBINE RELOAD USE **********\n"
                    << "********** Function: " << MF.getName() << '\n');
  bool Changed = false;
  if (!EnableSyncVMCombineReloadUse)
    return false;
  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  MDT = &getAnalysis<MachineDominatorTree>();
  RDA = &getAnalysis<ReachingDefAnalysis>();
  Changed |= combineReloadUse(MF);
  return Changed;
}

FunctionPass *llvm::createSyncVMCombineReloadUsePass() {
  return new SyncVMCombineReloadUse();
}
