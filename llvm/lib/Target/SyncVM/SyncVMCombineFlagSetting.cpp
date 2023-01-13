//===---- SyncVMCombineFlagSetting.cpp - Combine instructions Pre-RA ------===//
//
/// \file
///
/// The pass attempts to combine `sub.s! 0, x, y` with the definition of x:
/// '''
/// def = op x, y
/// ; No flags def or use in between
/// ; ... ...
/// result = sub.s! 0, def
/// '''
///
/// can be folded into:
/// '''
/// def = op! x, y
/// '''
///
/// The pass implemented as a local optimization and it require SSA form of MIR.
/// The pass require a single usage of each definition of Flags, so passes the
/// break that invariant must be scheduled after.
///
/// The pass layering is chosen this way because despite the pass can't handle
/// COPY and PHI pseudos, later passes elide most of the copies, and thus
/// `result` from the example above is defined by a near call instruction.
/// It require to make ret instruction flag setting to fold which is not
/// supported the VM now.
/// As for PHIs, it's unlikely that control-flow reaches the basic block
/// unconditionally from all predecessors, thus another flag setting is used
/// for a jump and it makes folding impossible.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include <optional>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-combine-flag-setting"
#define SYNCVM_COMBINE_FLAG_SETTING_NAME "SyncVM combine flag setting"

static cl::opt<bool>
    EnableSyncVMCombineFlagSetting("enable-syncvm-combine-flag-setting",
                                   cl::init(true), cl::Hidden);

STATISTIC(NumFlagsFolded, "Number of foldings done");

namespace {

class SyncVMCombineFlagSetting : public MachineFunctionPass {
public:
  static char ID;
  SyncVMCombineFlagSetting() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_COMBINE_FLAG_SETTING_NAME;
  }

private:
  const SyncVMInstrInfo *TII;
  /// Check whether there is any flag definition or usage in (Start, End).
  /// \p Start is a instuction that define Flags, so it's ok for it to use
  /// Flags, thus it's excluded from the check.
  bool hasFlagsDefOrUseBetween(MachineBasicBlock::iterator Start,
                               MachineBasicBlock::iterator End) const;
};

char SyncVMCombineFlagSetting::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMCombineFlagSetting, DEBUG_TYPE,
                SYNCVM_COMBINE_FLAG_SETTING_NAME, false, false)

/// Given sub.s! 0, x, y or equivalent \p MI return the register for x.
/// Return R0 if precondition is not met.
/// TODO: CPR-965 It might worth canonicalizing instead of checking different
/// forms, but there is a chance that the proper layering of CPR-965 is post-RA.
static Register getValueRegister(const MachineInstr &MI) {
  if (MI.getOpcode() == SyncVM::SUBrrr_v && MI.getOperand(1).isReg() &&
      MI.getOperand(2).isReg() && MI.getOperand(2).getReg() == SyncVM::R0)
    return MI.getOperand(1).getReg();
  if (MI.getOpcode() == SyncVM::SUBxrr_v && MI.getOperand(2).isReg() &&
      MI.getOperand(1).getCImm()->isZero())
    return MI.getOperand(2).getReg();
  return SyncVM::R0;
}

/// Assumming \p MI defines \p Register, return if it's out0.
static bool isOut0(const MachineInstr &MI, Register Reg) {
  // If MI stores out1 is on stack.
  if (MI.mayStore())
    return false;
  return MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == Reg;
}

/// Check whether Flags defined or used in [Start, End).
bool SyncVMCombineFlagSetting::hasFlagsDefOrUseBetween(
    MachineBasicBlock::iterator Start, MachineBasicBlock::iterator End) const {
  // In case of different basic blocks, conservatively assume true.
  if (Start->getParent() != End->getParent())
    return true;

  if (std::any_of(Start, End, [](const MachineInstr &MI) {
        return any_of(MI.implicit_operands(), [](const MachineOperand &MO) {
          return MO.isReg() && MO.getReg() == SyncVM::Flags;
        });
      }))
    return true;
  return false;
}

bool SyncVMCombineFlagSetting::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM COMBINE INSTRUCTIONS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  if (!EnableSyncVMCombineFlagSetting)
    return false;

  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  std::vector<MachineInstr *> ToRemove;

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      Register ValReg = getValueRegister(*MI);
      // MI is not sub.s! 0, x, y or equivalent.
      if (ValReg == SyncVM::R0)
        continue;

      // The pass is run on SSA form, so multiple definitions must be rare, so
      // ignore them.
      if (!RegInfo.hasOneDef(ValReg))
        continue;

      MachineInstr *DefMI = &*RegInfo.def_instructions(ValReg).begin();

      // There must be no flag def or use between the value definition and
      // sub.s! 0, x, y.
      if (hasFlagsDefOrUseBetween(DefMI->getIterator(), MI))
        continue;

      Register DefResultReg = DefMI->getOperand(0).getReg();

      // Can't fold if DefMI doesn't have a flag setting counterpart.
      if (SyncVM::getFlagSettingOpcode(DefMI->getOpcode()) == -1)
        continue;

      auto FlagUses = RegInfo.use_nodbg_instructions(SyncVM::Flags);
      // Flags are supposed to have the only use.
      auto FlagUse = std::find_if(
          std::next(MI), MBB.end(), [FlagUses](const MachineInstr &MI) {
            return any_of(FlagUses,
                          [&MI](const MachineInstr &MU) { return &MI == &MU; });
          });

      // It's needed to know which flags are used because not all of them can
      // be folded.
      if (FlagUse == MBB.end())
        continue;

      SyncVMCC::CondCodes CC = TII->getCCCode(*FlagUse);
      // In SyncVM only EQ flag is defined uniformly across flag setting
      // instruction. Other flags have different semantic for different
      // instructions. So they can't be folded with DefMI.
      if (CC != SyncVMCC::COND_E && CC != SyncVMCC::COND_NE)
        continue;

      if (TII->isMul(*DefMI) && !isOut0(*DefMI, ValReg))
        // If ValReg is out1 and CC = COND_NE, folding can be done, but
        // COND_LT is needed and it should be non-reversible for the sake of
        // branch folding. No profitability found on Uniswap.
        continue;

      if (TII->isDiv(*DefMI) && !isOut0(*DefMI, ValReg))
        // If ValReg is out1 and CC = COND_E, folding can be done, but
        // COND_GT is needed and it should be non-reversible for the sake of
        // branch folding. No profitability found on Uniswap.
        continue;

      LLVM_DEBUG(dbgs() << "== Combined instruction:"; DefMI->dump();
                 dbgs() << "        And instruction:"; MI->dump(););
      ++NumFlagsFolded;

      // Fold sub.s! 0, (op x, y) to op! x, y
      Register ResultReg = MI->getOperand(0).getReg();
      DefMI->setDesc(
          TII->get(SyncVM::getFlagSettingOpcode(DefMI->getOpcode())));
      RegInfo.replaceRegWith(ResultReg, DefResultReg);
      ToRemove.push_back(&*MI);

      LLVM_DEBUG(dbgs() << "== Into:"; DefMI->dump(););
    }

  for (auto MI : ToRemove)
    MI->eraseFromParent();

  return !ToRemove.empty();
}

FunctionPass *llvm::createSyncVMCombineFlagSettingPass() {
  return new SyncVMCombineFlagSetting();
}
