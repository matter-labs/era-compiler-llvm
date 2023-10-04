//===---- EraVMCombineFlagSetting.cpp - Combine instructions Pre-RA -------===//
//
/// \file
///
/// The pass attempts to combine `sub! x, r0, y` with the definition of x:
/// '''
/// def = op x, y
/// ; No flags def or use in between
/// ; ... ...
/// result = sub! def, r0
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

#include "EraVM.h"

#include <optional>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-combine-flag-setting"
#define ERAVM_COMBINE_FLAG_SETTING_NAME "EraVM combine flag setting"

static cl::opt<bool>
    EnableEraVMCombineFlagSetting("enable-eravm-combine-flag-setting",
                                  cl::init(true), cl::Hidden);

STATISTIC(NumFlagsFolded, "Number of foldings done");

namespace {

class EraVMCombineFlagSetting : public MachineFunctionPass {
public:
  static char ID;
  EraVMCombineFlagSetting() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return ERAVM_COMBINE_FLAG_SETTING_NAME;
  }

private:
  const EraVMInstrInfo *TII;
  /// Check whether there is any flag definition or usage in (Start, End).
  /// \p Start is a instuction that define Flags, so it's ok for it to use
  /// Flags, thus it's excluded from the check.
  bool hasFlagsDefOrUseBetween(MachineBasicBlock::iterator Start,
                               MachineBasicBlock::iterator End) const;
};

char EraVMCombineFlagSetting::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMCombineFlagSetting, DEBUG_TYPE,
                ERAVM_COMBINE_FLAG_SETTING_NAME, false, false)

/// Given sub! x, r0, y or equivalent \p MI return the register for x.
/// Return R0 if precondition is not met.
static Register getValueRegister(const MachineInstr &MI) {
  if (MI.getOpcode() == EraVM::SUBrrr_v && MI.getOperand(1).isReg() &&
      MI.getOperand(2).isReg() && MI.getOperand(2).getReg() == EraVM::R0)
    return MI.getOperand(1).getReg();
  return EraVM::R0;
}

/// Assumming \p MI defines \p Register, return if it's out0.
static bool isOut0(const MachineInstr &MI, Register Reg) {
  // If MI stores out1 is on stack.
  if (MI.mayStore())
    return false;
  return MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == Reg;
}

/// Check whether Flags defined or used in [Start, End).
bool EraVMCombineFlagSetting::hasFlagsDefOrUseBetween(
    MachineBasicBlock::iterator Start, MachineBasicBlock::iterator End) const {
  // In case of different basic blocks, conservatively assume true.
  if (Start->getParent() != End->getParent())
    return true;

  if (std::any_of(Start, End, [](const MachineInstr &MI) {
        return any_of(MI.implicit_operands(), [](const MachineOperand &MO) {
          return MO.isReg() && MO.getReg() == EraVM::Flags;
        });
      }))
    return true;
  return false;
}

bool EraVMCombineFlagSetting::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM COMBINE INSTRUCTIONS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  if (!EnableEraVMCombineFlagSetting)
    return false;

  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  std::vector<MachineInstr *> ToRemove;

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      Register ValReg = getValueRegister(*MI);
      // MI is not sub! x, r0, y.
      if (ValReg == EraVM::R0)
        continue;

      // The pass is run on SSA form, so multiple definitions must be rare, so
      // ignore them.
      if (!RegInfo.hasOneDef(ValReg))
        continue;

      MachineInstr *DefMI = &*RegInfo.def_instructions(ValReg).begin();

      // There must be no flag def or use between the value definition and
      // sub! x, r0, y.
      if (hasFlagsDefOrUseBetween(DefMI->getIterator(), MI))
        continue;

      Register DefResultReg = DefMI->getOperand(0).getReg();

      // Can't fold if DefMI doesn't have a flag setting counterpart.
      if (EraVM::getFlagSettingOpcode(DefMI->getOpcode()) == -1)
        continue;

      auto FlagUses = RegInfo.use_nodbg_instructions(EraVM::Flags);
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

      EraVMCC::CondCodes CC = TII->getCCCode(*FlagUse);
      // In EraVM only EQ flag is defined uniformly across flag setting
      // instruction. Other flags have different semantic for different
      // instructions. So they can't be folded with DefMI.
      if (CC != EraVMCC::COND_E && CC != EraVMCC::COND_NE)
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

      // Fold sub! (op x, y), r0 to op! x, y
      Register ResultReg = MI->getOperand(0).getReg();
      DefMI->setDesc(TII->get(EraVM::getFlagSettingOpcode(DefMI->getOpcode())));
      DefMI->copyImplicitOps(MF, *MI);
      RegInfo.replaceRegWith(ResultReg, DefResultReg);
      ToRemove.push_back(&*MI);

      LLVM_DEBUG(dbgs() << "== Into:"; DefMI->dump(););
    }

  for (auto *MI : ToRemove)
    MI->eraseFromParent();

  return !ToRemove.empty();
}

FunctionPass *llvm::createEraVMCombineFlagSettingPass() {
  return new EraVMCombineFlagSetting();
}
