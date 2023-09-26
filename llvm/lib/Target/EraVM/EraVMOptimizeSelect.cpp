//===--------------- EraVMOptimizeSelect.cpp - Optimize select ------------===//
//
/// \file
/// This file contains a pass that tries to optimize after select instruction
/// expansion. Usually a conditional move instruction is expanded, which might
/// be folded with a previous instruction.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-opt-select"
#define SYNCVM_OPT_SELECT_NAME "EraVM select optimization"

namespace {

/// This pass folds conditional move from an expanded select instructions, with
/// an instruction that is in the same MBB.
class EraVMOptimizeSelect : public MachineFunctionPass {
public:
  static char ID;
  EraVMOptimizeSelect() : MachineFunctionPass(ID) {
    initializeEraVMOptimizeSelectPass(*PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &Fn) override;
  StringRef getPassName() const override { return SYNCVM_OPT_SELECT_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<ReachingDefAnalysis>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  /// Look for candidate pairs:
  /// op ra, rb, rc ; where `op` is predicable
  /// add.cc rc, r0, rd
  ///
  /// where the output operand is only used for transitive uses, and fold them
  /// into:
  /// op.cc ra, rb, rd
  ///
  /// return true if transformation successful.
  bool foldConditionalMove(MachineInstr &MI) const;

  /// Returns a folding-candidate out register from MI.
  /// Returns invalid register if the output register of MI is not a folding
  /// candidate.
  Register getOutRegToFold(const MachineInstr &MI) const;

  /// look ahead in the same MBB to find a folding candidate instruction, return
  /// null if no such candidate found.
  ///
  /// A qualified candidate instruction must:
  /// 1. be predicable, and currently does not have a predicate.
  /// 2. has a single register output, or ther other register output is killed.
  /// 3. does not set flags.
  /// 4. does not have any register operands that are used in between.
  ///
  /// UseReg is the intermediate result register unconditionally defined by the
  /// folding candidate. ConditionalDefReg is the register that is conditionally
  /// defined by MI.
  std::optional<MachineInstr *>
  findFoldingCandidateInstr(MachineInstr *MI, MCRegister UseReg,
                            MCRegister DefReg) const;
  const EraVMInstrInfo *TII;
  ReachingDefAnalysis *RDA;
};

char EraVMOptimizeSelect::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMOptimizeSelect, DEBUG_TYPE, SYNCVM_OPT_SELECT_NAME, false,
                false)

Register EraVMOptimizeSelect::getOutRegToFold(const MachineInstr &MI) const {
  // For writing to stack, it is risky to fold it into conditional move because
  // we currently have no way to track a stackop's liveness. So just bail out if
  // we are not dealing with register out form.
  if (!EraVM::hasRROutAddressingMode(MI))
    return {};

  // We can only safely fold this instruction with conditional move if it is
  // single register output. If there are two register outputs (when operation
  // is either Mul or Div), have to make sure at least one of them is killed.
  if (TII->isMul(MI) || TII->isDiv(MI)) {
    if (!EraVM::out0Iterator(MI)->isDead() &&
        !EraVM::out1Iterator(MI)->isDead())
      return {};
    return EraVM::out0Iterator(MI)->isDead()
               ? EraVM::out1Iterator(MI)->getReg()
               : EraVM::out0Iterator(MI)->getReg();
  }

  // the case which there is one register out
  return EraVM::out0Iterator(MI)->getReg();
}

/// return true if the pattern is a conditional move:
/// `add.xx %rc (killed), r0, %rb`
static bool isConditionalMove(const MachineInstr &MI) {
  if (MI.getOpcode() != EraVM::ADDrrr_s ||
      getImmOrCImm(*EraVM::ccIterator(MI)) == EraVMCC::COND_NONE)
    return false;
  const auto *const In0 = EraVM::in0Iterator(MI);
  const auto *const In1 = EraVM::in1Iterator(MI);

  return In1->getReg() == EraVM::R0 && In0->isKill();
}

std::optional<MachineInstr *> EraVMOptimizeSelect::findFoldingCandidateInstr(
    MachineInstr *MI, MCRegister UseReg, MCRegister ConditionalDefReg) const {
  assert(MI && "MI cannot be null");
  assert(RDA && "RDA cannot be null");

  // Only profitable to fold if both instructions are in a single MBB, going
  // across MBBs not worth it: predicated instructions always
  // come with a local flag-setting instruction, using a flag passed from other
  // MBB is not guaranteed conformant.
  if (!RDA->hasLocalDefBefore(MI, UseReg))
    return {};
  MachineInstr *DefMI = RDA->getUniqueReachingMIDef(MI, UseReg);
  if (!DefMI || EraVMInstrInfo::isFlagSettingInstruction(*DefMI))
    return {};

  // TODO: CPR-1399 generalize idiom checking
  // The candidate must be a predicable instruction:
  if (!TII->isPredicatedInstr(*DefMI) ||
      getImmOrCImm(*EraVM::ccIterator(*DefMI)) != EraVMCC::COND_NONE)
    return {};

  const auto DefMIIter = MachineBasicBlock::iterator(DefMI);
  const auto MIIter = MachineBasicBlock::iterator(MI);

  // To fold the candidate, the UseReg must only be used once, which is in MI:
  SmallPtrSet<MachineInstr *, 4> Uses;
  RDA->getGlobalUses(DefMI, UseReg, Uses);
  if (Uses.size() != 1)
    return {};
  assert(Uses.count(MI) == 1);

  // ConditionalDefReg of the conditional move cannot appear in between DefMI
  // and MI:
  for (const MachineInstr &I : make_range(std::next(DefMIIter), MIIter)) {
    // This instruction cannot set flags:
    if (EraVMInstrInfo::isFlagSettingInstruction(I))
      return {};

    // folding registers cannot appear in between:
    if (llvm::any_of(I.operands(),
                     [ConditionalDefReg, UseReg](const MachineOperand &Opnd) {
                       // return false means it is safe
                       if (!Opnd.isReg())
                         return false;
                       const Register OpndReg = Opnd.getReg();
                       if (OpndReg == ConditionalDefReg || OpndReg == UseReg)
                         return true;
                       return false;
                     }))
      return {};
  }

  return DefMI;
}

bool EraVMOptimizeSelect::foldConditionalMove(MachineInstr &MI) const {
  if (!isConditionalMove(MI))
    return false;

  const Register IntermediateReg = EraVM::in0Iterator(MI)->getReg();
  const Register OutReg = EraVM::out0Iterator(MI)->getReg();

  // Find if the previous instruction is a folding candidte.
  auto CandidateOpt = findFoldingCandidateInstr(&MI, IntermediateReg, OutReg);
  if (!CandidateOpt)
    return false;
  MachineInstr &CandidateInstr = **CandidateOpt;

  // If there are more than 1 outputs, we have to make sure the
  // other output is killed before we can fold it.
  const Register CandidateOutReg = getOutRegToFold(CandidateInstr);
  if (!CandidateOutReg)
    return false;

  // Check the CandidateOutReg is the intermedate value register:
  if (CandidateOutReg != IntermediateReg)
    return false;

  // Now we have a candidate pair:
  // 1. move the condition code to PrevMI
  // 2. replace CandidateOutReg with OutReg
  // 3. return true to remove MI
  EraVM::ccIterator(CandidateInstr)
      ->ChangeToImmediate(EraVM::ccIterator(MI)->getImm());
  EraVM::out0Iterator(CandidateInstr)->setReg(OutReg);
  return true;
}

bool EraVMOptimizeSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM OptimizeSelect optimization **********\n"
             << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  RDA = &getAnalysis<ReachingDefAnalysis>();

  std::vector<MachineInstr *> ToBeErased;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (foldConditionalMove(MI))
        ToBeErased.push_back(&MI);

  for (auto *I : ToBeErased)
    I->eraseFromParent();

  return !ToBeErased.empty();
}

/// createEraVMOptimizeSelectPass - returns an instance of the select
/// optimization pass.
FunctionPass *llvm::createEraVMOptimizeSelectPass() {
  return new EraVMOptimizeSelect();
}
