//===-- EraVMFoldSimilarInstructions.cpp - Fold similar instrs --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass that folds similar instructions.
// Currently, it only works with adds and subs that are consecutive, but this
// can be improved by adding support for other instructions and by expanding
// the search.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-fold-similar-instrs"
#define ERAVM_FOLD_SIMILAR_INSTRS_NAME "EraVM fold similar instructions"

namespace {

class EraVMFoldSimilarInstructions : public MachineFunctionPass {
public:
  static char ID;
  EraVMFoldSimilarInstructions() : MachineFunctionPass(ID) {
    initializeEraVMFoldSimilarInstructionsPass(
        *PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return ERAVM_FOLD_SIMILAR_INSTRS_NAME;
  }

private:
  const EraVMInstrInfo *TII{};
  MachineConstantPool *MCP{};

  /// Return whether MI is an add const.
  /// I.e. an instruction of form `add const, rIn1, rOut`.
  bool isConstAdd(const MachineInstr &MI) const;

  /// Return whether MI is a sub const that swaps operands and sets flags.
  /// I.e. an instruction of form `sub.s! const, rIn1, rOut`.
  bool isConstSubSWithFlags(const MachineInstr &MI) const;

  /// Return constant from the constpool at the given index.
  APInt getConstFromCP(unsigned CPI) const;

  /// In case first instruction is an add const and the second instruction
  /// is a sub const that swaps operands and sets flags, return whether they
  /// are similar.
  /// They are similar in the following cases:
  ///   add rIN + const
  ///   sub rIN - (-const)
  /// and
  ///   add rIN + (-const)
  ///   sub rIN - const
  bool isConstAddSimilarToConstSubSWithFlags(const MachineInstr &Add,
                                             const MachineInstr &Sub) const;

  /// Return whether these two sub instructions are similar. Instructions
  /// are similar iff their operands are identical, expect in the case
  /// where we are trying to fold first sub with his flag setting pair
  /// sub, and we are allowing for the second sub to have one more
  /// implicit def of the flags register.
  bool areSubsSimilar(const MachineInstr &FirstSub,
                      const MachineInstr &SecondSub) const;

  /// Return whether operands of two instructions are identical. In case
  /// IgnoreFlagSetting is true, we expect that the Second instruction has
  /// flags def implicit register and we should skip it during the checks.
  bool areOperandsIdentical(const MachineInstr &First,
                            const MachineInstr &Second,
                            bool IgnoreFlagSetting) const;
};

char EraVMFoldSimilarInstructions::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMFoldSimilarInstructions, DEBUG_TYPE,
                ERAVM_FOLD_SIMILAR_INSTRS_NAME, false, false)

bool EraVMFoldSimilarInstructions::isConstAdd(const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::ADDcrr_s && EraVM::in0Iterator(MI)->isCPI() &&
         (EraVM::in0Iterator(MI) + 1)->isImm() &&
         !(EraVM::in0Iterator(MI) + 1)->getImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMFoldSimilarInstructions::isConstSubSWithFlags(
    const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::SUByrr_v && EraVM::in0Iterator(MI)->isCPI() &&
         (EraVM::in0Iterator(MI) + 1)->isImm() &&
         !(EraVM::in0Iterator(MI) + 1)->getImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

APInt EraVMFoldSimilarInstructions::getConstFromCP(unsigned CPI) const {
  assert(CPI < MCP->getConstants().size() && "Invalid constpool index");
  const MachineConstantPoolEntry &CPE = MCP->getConstants()[CPI];
  assert(!CPE.isMachineConstantPoolEntry() && "Invalid constpool entry");
  APInt Const = cast<ConstantInt>(CPE.Val.ConstVal)->getValue();
  assert(Const.getBitWidth() == 256 && "Invalid constant bitwidth");
  return Const;
}

bool EraVMFoldSimilarInstructions::areOperandsIdentical(
    const MachineInstr &First, const MachineInstr &Second,
    bool IgnoreFlagSetting) const {
  auto SecondImplicitOps = SmallVector<const MachineOperand *, 4>{map_range(
      make_filter_range(Second.implicit_operands(),
                        [IgnoreFlagSetting](const MachineOperand &OP) {
                          return !IgnoreFlagSetting || !OP.isReg() ||
                                 !OP.isDef() || OP.getReg() != EraVM::Flags;
                        }),
      [](const MachineOperand &MOP) { return &MOP; })};

  // If we need to ignore implicit flags setting register in the second
  // instruction and we didn't find it, bail out.
  if (IgnoreFlagSetting &&
      (SecondImplicitOps.size() + 1 != Second.getNumImplicitOperands()))
    return false;

  if (First.getNumExplicitOperands() != Second.getNumExplicitOperands() ||
      First.getNumImplicitOperands() != SecondImplicitOps.size())
    return false;

  for (unsigned J = 0, I = First.getNumExplicitOperands(),
                E = First.getNumOperands();
       I != E; ++I, ++J)
    if (!First.getOperand(I).isIdenticalTo(*SecondImplicitOps[J]))
      return false;

  // TODO: CPR-1230 Skip checking for conditions, since they can be imm or cimm.
  for (unsigned I = 0, E = First.getNumExplicitOperands() - 1; I != E; ++I) {
    const auto &FirstMO = First.getOperand(I);
    const auto &SecondMO = Second.getOperand(I);

    // Ignore virtual defs, since we want to fold first instruction into second.
    if (FirstMO.isReg() && FirstMO.isDef() && SecondMO.isReg() &&
        SecondMO.isDef() && Register::isVirtualRegister(FirstMO.getReg()) &&
        Register::isVirtualRegister(SecondMO.getReg()))
      continue;

    if (!FirstMO.isIdenticalTo(SecondMO))
      return false;
  }
  return TII->getCCCode(First) == TII->getCCCode(Second);
}

bool EraVMFoldSimilarInstructions::areSubsSimilar(
    const MachineInstr &FirstSub, const MachineInstr &SecondSub) const {
  if (!TII->isSub(FirstSub) || !TII->isSub(SecondSub) ||
      !EraVM::hasRROutAddressingMode(FirstSub) ||
      !EraVM::hasRROutAddressingMode(SecondSub))
    return false;

  bool isSimilarToFlagSettingPair =
      EraVM::getFlagSettingOpcode(FirstSub.getOpcode()) ==
      static_cast<int>(SecondSub.getOpcode());
  return (isSimilarToFlagSettingPair ||
          FirstSub.getOpcode() == SecondSub.getOpcode()) &&
         areOperandsIdentical(FirstSub, SecondSub, isSimilarToFlagSettingPair);
}

bool EraVMFoldSimilarInstructions::isConstAddSimilarToConstSubSWithFlags(
    const MachineInstr &Add, const MachineInstr &Sub) const {
  if (!isConstAdd(Add) || !isConstSubSWithFlags(Sub) ||
      EraVM::in1Iterator(Add)->getReg() != EraVM::in1Iterator(Sub)->getReg())
    return false;

  APInt AddConst = getConstFromCP(EraVM::in0Iterator(Add)->getIndex());
  APInt SubConst = getConstFromCP(EraVM::in0Iterator(Sub)->getIndex());
  return (AddConst + SubConst).isZero();
}

bool EraVMFoldSimilarInstructions::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM FOLD SIMILAR INSTRUCTIONS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MCP = MF.getConstantPool();
  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  MachineRegisterInfo *RegInfo = &MF.getRegInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    if (MBB.empty())
      continue;

    for (auto &MI : make_early_inc_range(drop_end(MBB))) {
      auto &NextMI = *std::next(MI.getIterator());
      // TODO: CPR-1543 Support more cases to fold.
      if (!isConstAddSimilarToConstSubSWithFlags(MI, NextMI) &&
          !areSubsSimilar(MI, NextMI))
        continue;

      LLVM_DEBUG(dbgs() << "== Folding inst:"; MI.dump();
                 dbgs() << "             to:"; NextMI.dump(););
      RegInfo->replaceRegWith(EraVM::out0Iterator(MI)->getReg(),
                              EraVM::out0Iterator(NextMI)->getReg());
      MI.eraseFromParent();
      Changed = true;
    }
  }
  return Changed;
}

/// createEraVMFoldSimilarInstructionsOperandsPass - returns an instance of the
/// fold similar instructions pass.
FunctionPass *llvm::createEraVMFoldSimilarInstructionsPass() {
  return new EraVMFoldSimilarInstructions();
}
