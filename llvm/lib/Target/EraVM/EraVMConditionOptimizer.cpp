//===-- EraVMConditionOptimizer.cpp - Optimize conditions -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass tries to adjust compares in order to expose CSE opportunities.
// For this, it analyzes compares with immediate and the following instruction
// that should be a select or a conditional branch, whose conditional code can
// be converted:
//  * LT -> LE
//  * GT -> GE
// and adjusts immediate value in compare appropriately.
// This is needed because ISEL (mainly SelectionDAGBuilder::visitSwitchCase),
// can generate compares that are not CSE friendly:
//
//     ...
//     sub.s   10, r1, r2
//     sub.s!   9, r1, r0
//     jump.gt @.BB0_2
//     ...
//
// Same assembly after this pass | and after MachineCSE pass:
//     ...                       |
//     sub.s   10, r1, r2        |     ...
//     sub.s!  10, r1, r0        |     sub.s!  10, r1, r2
//     jump.ge @.BB0_2           |     jump.ge @.BB0_2
//     ...                       |     ...
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-condopt"
#define ERAVM_COND_OPT_NAME "EraVM condition optimizer"

namespace {

class EraVMConditionOptimizer : public MachineFunctionPass {
public:
  static char ID;
  EraVMConditionOptimizer() : MachineFunctionPass(ID) {
    initializeEraVMConditionOptimizerPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return ERAVM_COND_OPT_NAME; }

private:
  const EraVMInstrInfo *TII{};
  MachineRegisterInfo *RegInfo{};
  const MachineDominatorTree *MDT{};
  LLVMContext *Context{};
  MachineConstantPool *MCP{};

  // No need to add LE and GE, since ISEL will canonicalize them to LT and GT.
  DenseMap<unsigned, unsigned> AdjustedCond{
      {EraVMCC::COND_LT, EraVMCC::COND_LE},
      {EraVMCC::COND_GT, EraVMCC::COND_GE},
  };

  /// Return whether MI is a sub with imm that swaps operands and sets flags.
  /// I.e. an instruction of form:
  ///    `sub.s! imm, rIn1, rOut`.
  ///    `sub.s! constantpool, rIn1, rOut`.
  ///
  /// In this particular stage (before PeepholeOptimizer), we can only
  /// distinguish between cmp and sub by checking if the definition of flags
  /// register is dead. If it is dead, then it is a sub instruction, otherwise
  /// it is a cmp instruction. Based on this, if FlagsDead is true, we are
  /// looking for sub instruction, otherwise we are looking for cmp instruction.
  bool isImmSubSWithFlags(const MachineInstr &MI, bool FlagsDead) const;

  /// Return constant from the constpool at the given index.
  APInt getConstFromCP(unsigned CPI) const;

  /// Return immediate from the machine operand.
  APInt getImm(const MachineOperand &MO) const;

  /// Adjust compare with immediate instructions in order to expose CSE
  /// opportunities with SUB instructions. In order to do this, we need to find
  /// a compare with immediate instruction that is followed by a select or
  /// conditional branch instruction whose conditional code can be adjusted.
  /// After that, we need to find how many SUB instructions can be CSEd with
  /// this compare instruction. If there are more SUB instructions that can be
  /// CSEd with the adjusted immediate of the CMP, we adjust the immediate of
  /// the compare instruction and the conditional code of the select or
  /// conditional branch instruction.
  bool tryToAdjustCompareWithImm(MachineBasicBlock &MBB);
};

char EraVMConditionOptimizer::ID = 0;
} // namespace

INITIALIZE_PASS_BEGIN(EraVMConditionOptimizer, DEBUG_TYPE, ERAVM_COND_OPT_NAME,
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(EraVMConditionOptimizer, DEBUG_TYPE, ERAVM_COND_OPT_NAME,
                    false, false)

static constexpr unsigned ImmBitWidth = 256;
static constexpr unsigned CPEntryAlign = 32;

APInt EraVMConditionOptimizer::getConstFromCP(unsigned CPI) const {
  assert(CPI < MCP->getConstants().size() && "Invalid constpool index");
  const MachineConstantPoolEntry &CPE = MCP->getConstants()[CPI];
  assert(!CPE.isMachineConstantPoolEntry() && "Invalid constpool entry");
  assert(CPE.Alignment == Align(CPEntryAlign) && "Invalid constpool alignment");
  APInt Const = cast<ConstantInt>(CPE.Val.ConstVal)->getValue();
  assert(Const.getBitWidth() == ImmBitWidth && "Invalid constant bitwidth");
  return Const;
}

APInt EraVMConditionOptimizer::getImm(const MachineOperand &MO) const {
  assert(MO.isCImm() || MO.isCPI() && "Invalid machine operand");
  return MO.isCImm() ? MO.getCImm()->getValue() : getConstFromCP(MO.getIndex());
}

bool EraVMConditionOptimizer::isImmSubSWithFlags(const MachineInstr &MI,
                                                 bool FlagsDead) const {
  if (MI.getOpcode() != EraVM::SUBxrr_v && MI.getOpcode() != EraVM::SUByrr_v)
    return false;

  const auto *FlagDef = MI.findRegisterDefOperand(EraVM::Flags);
  if (!FlagDef || (FlagsDead != FlagDef->isDead()))
    return false;

  if (!EraVM::out0Iterator(MI)->getReg().isVirtual() ||
      !EraVM::in1Iterator(MI)->getReg().isVirtual() ||
      TII->getCCCode(MI) != EraVMCC::COND_NONE)
    return false;

  if (MI.getOpcode() == EraVM::SUBxrr_v) {
    assert(EraVM::in0Iterator(MI)->isCImm() &&
           EraVM::in0Iterator(MI)->getCImm()->getBitWidth() == ImmBitWidth &&
           "Expected 256-bit constant immediate operand");
    return true;
  }
  return EraVM::in0Iterator(MI)->isCPI() &&
         !EraVM::in0Iterator(MI)->getOffset() &&
         (EraVM::in0Iterator(MI) + 1)->isImm() &&
         !(EraVM::in0Iterator(MI) + 1)->getImm();
}

bool EraVMConditionOptimizer::tryToAdjustCompareWithImm(
    MachineBasicBlock &MBB) {
  bool Changed = false;
  for (auto &MI : make_early_inc_range(drop_end(MBB))) {
    // Skip non-compare instructions, and compare whose output is used.
    if (!isImmSubSWithFlags(MI, false) ||
        !RegInfo->use_nodbg_empty(EraVM::out0Iterator(MI)->getReg()))
      continue;

    auto &NextMI = *std::next(MI.getIterator());
    auto CCNextMI = TII->getCCCode(NextMI);

    // Next instruction should be a select or a conditional branch,
    // which conditional code can be adjusted. It is only needed to
    // check the next instruction, since in ISEL compare is always
    // glued with either select or conditional branch.
    if ((!TII->isSel(NextMI) && !NextMI.isConditionalBranch()) ||
        !AdjustedCond.count(CCNextMI))
      continue;

    auto &ImmOP = *EraVM::in0Iterator(MI);
    APInt ImmVal = getImm(ImmOP);

    // TODO #654: Add support for changing CImm to CPI and vice versa.
    if ((CCNextMI == EraVMCC::COND_GT &&
         ImmVal == APInt(ImmBitWidth, UINT16_MAX)) ||
        (CCNextMI == EraVMCC::COND_LT &&
         ImmVal == APInt(ImmBitWidth, UINT16_MAX + 1)))
      continue;

    // If the condition is GT, we need to adjust the immediate by 1,
    // in case it is LT, we need to adjust the immediate by -1.
    APInt Adjustment = CCNextMI == EraVMCC::COND_GT
                           ? APInt(ImmBitWidth, 1)
                           : APInt::getAllOnes(ImmBitWidth);
    APInt AdjustedVal = ImmVal + Adjustment;

    // Walk through all users of the input register and try to find sub
    // instructions that can be CSEd with this cmp. Only adjust compare
    // in case there are more sub instructions with the adjusted imm
    // than the original imm, so we can potentially CSE more instructions.
    int GoodToAdjust = 0;
    for (auto &UseMI :
         RegInfo->use_nodbg_instructions(EraVM::in1Iterator(MI)->getReg())) {
      // Skip if this is not a sub instruction that can be CSEd with compare.
      if (&MI == &UseMI || UseMI.getOpcode() != MI.getOpcode() ||
          !isImmSubSWithFlags(UseMI, true) ||
          EraVM::in1Iterator(MI)->getReg() !=
              EraVM::in1Iterator(UseMI)->getReg())
        continue;

      // Only take into account instructions that can actually be CSEd.
      // This means that a sub should be dominated by the compare or
      // sub should dominate the compare. In the latter case, we only
      // check if they are in the same BB. This is because in order to
      // replace cmp with sub, sub will need to define flags register,
      // and to safely do so, there mustn't be an instruction that
      // defines flags register between them. This check is performed
      // by MachineCSE::PhysRegDefsReach and it will likely fail if
      // sub and cmp are in different BBs.
      if (!MDT->dominates(&MI, &UseMI) &&
          (MI.getParent() != UseMI.getParent() || !MDT->dominates(&UseMI, &MI)))
        continue;

      APInt UseImmVal = getImm(*EraVM::in0Iterator(UseMI));
      if (AdjustedVal == UseImmVal)
        ++GoodToAdjust;
      else if (ImmVal == UseImmVal)
        --GoodToAdjust;
    }

    if (GoodToAdjust <= 0)
      continue;

    LLVM_DEBUG(dbgs() << "==   Adjusting compare:"; MI.dump(););

    // Update the immediate operand of the compare instruction.
    const auto *C = ConstantInt::get(*Context, AdjustedVal);
    if (ImmOP.isCImm()) {
      ImmOP.setCImm(C);
    } else {
      assert(ImmOP.isCPI() && "Machine operand should be a CPI");
      ImmOP.setIndex(MCP->getConstantPoolIndex(C, Align(CPEntryAlign)));
    }

    LLVM_DEBUG(dbgs() << "                    to:"; MI.dump(););
    LLVM_DEBUG(dbgs() << "   Adjusting cond inst:"; NextMI.dump(););

    // Update the condition code of the select or conditional branch.
    EraVM::ccIterator(NextMI)->setCImm(
        ConstantInt::get(*Context, APInt(ImmBitWidth, AdjustedCond[CCNextMI])));

    LLVM_DEBUG(dbgs() << "                    to:"; NextMI.dump();
               dbgs() << "to potentially CSE " << GoodToAdjust
                      << " instructions\n";);
    Changed = true;
  }
  return Changed;
}

bool EraVMConditionOptimizer::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM CONDITION OPTIMIZER **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MCP = MF.getConstantPool();
  Context = &MF.getFunction().getContext();
  RegInfo = &MF.getRegInfo();
  MDT = &getAnalysis<MachineDominatorTree>();
  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    Changed |= tryToAdjustCompareWithImm(MBB);

  return Changed;
}

/// createEraVMConditionOptimizerOperandsPass - returns an instance of the
/// condition optimizer pass.
FunctionPass *llvm::createEraVMConditionOptimizerPass() {
  return new EraVMConditionOptimizer();
}
