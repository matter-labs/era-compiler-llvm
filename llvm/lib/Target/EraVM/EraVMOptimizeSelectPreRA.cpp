//===-- EraVMOptimizeSelectPreRA.cpp - Optimize select preRA ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pre-ra pass to optimize SELECT instructions. It tries
// to fold SELECT with its user in case one of the input operands of SELECT
// is immediate zero and it tries to fold ADD with SELECT if one of the inputs
// of ADD matches with the input of SELECT.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-opt-select-prera"
#define ERAVM_OPT_SELECT_PRERA_NAME "EraVM select optimization preRA"

namespace {

class EraVMOptimizeSelectPreRA : public MachineFunctionPass {
public:
  static char ID;
  EraVMOptimizeSelectPreRA() : MachineFunctionPass(ID) {
    initializeEraVMOptimizeSelectPreRAPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_OPT_SELECT_PRERA_NAME; }

private:
  const EraVMInstrInfo *TII{};
  MachineRegisterInfo *RegInfo{};
  DenseMap<unsigned, unsigned> InverseCond{
      {EraVMCC::COND_E, EraVMCC::COND_NE},
      {EraVMCC::COND_NE, EraVMCC::COND_E},
      {EraVMCC::COND_LT, EraVMCC::COND_GE},
      {EraVMCC::COND_LE, EraVMCC::COND_GT},
      {EraVMCC::COND_GT, EraVMCC::COND_LE},
      {EraVMCC::COND_GE, EraVMCC::COND_LT},
  };

  /// Non-commutative opcodes that will produce a neutral result once operated
  /// with immediate zero.
  DenseSet<unsigned> NonCommOpSet = {
      EraVM::SUBrrr_s, EraVM::SHLrrr_s, EraVM::SHRrrr_s,
      EraVM::ROLrrr_s, EraVM::RORrrr_s,
  };

  /// Check whether there is any flag definition in [Start, End).
  bool hasFlagsDefBetween(MachineBasicBlock::iterator Start,
                          MachineBasicBlock::iterator End) const;

  /// Check whether current MI can form a neutral operation with immediate zero,
  /// such as add with zero, sub with zero and etc.
  bool isSupportedMI(const MachineInstr &MI, unsigned Reg) const;

  /// Figure out which opcode we should use to fold SELECT with its user.
  unsigned getFoldedMIOp(EraVM::ArgumentKind Kind, const MachineInstr &SelectMI,
                         const MachineInstr &UseMI) const;

  /// Create a folded instruction based on the position of ToFold,
  /// output register of ToFold, opcode of NewOpcode, input operands
  /// In0 and In1 and conditional code of NewCC. TieReg is the input
  /// register which we tie with the output register. In the end
  /// copy flags and memory references from FlagMI.
  MachineInstr *
  getFoldedInst(MachineInstr &ToFold, const MachineInstr &FlagMI,
                iterator_range<MachineInstr::const_mop_iterator> In0,
                Register In1, unsigned NewOpcode, unsigned NewCC,
                Register TieReg) const;

  /// In case one of the input operand of SELECT is immediate zero, then it is
  /// possible to fold it with its sole user to result in just one folded
  /// instruction.
  ///
  /// A typical case is like below:
  ///
  ///   x = SEL non-zero-val, 0, cc
  ///   ...
  ///   z = ADDrrr_s non-zero-val, y
  ///
  /// can be folded into:
  ///   z = ADDirr_s.cc non-zero-val, y
  ///
  /// if z and y can be allocated to same reg. The tie is used to ensure this.
  bool tryFoldSelectZero(MachineBasicBlock &MBB);

  /// In case ADD is fed into a SELECT, and one of the inputs of ADD matches
  /// with the input of SELECT, then it is possible to fold ADD with SELECT
  /// to result in just one folded instruction.
  ///
  /// A typical case is like below:
  ///
  ///   outADD = ADD imm/const/x, y
  ///   ...
  ///   outSEL = SEL outADD, x/y
  ///
  /// can be folded into:
  ///   outSEL = ADD.cc imm/const/x, y
  ///
  /// if outSEL and x/y can be allocated to same reg.
  /// The tie is used to ensure this.
  bool tryFoldAddToSelect(MachineBasicBlock &MBB);
};

char EraVMOptimizeSelectPreRA::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMOptimizeSelectPreRA, DEBUG_TYPE,
                ERAVM_OPT_SELECT_PRERA_NAME, false, false)

bool EraVMOptimizeSelectPreRA::hasFlagsDefBetween(
    MachineBasicBlock::iterator Start, MachineBasicBlock::iterator End) const {
  // In case of different basic blocks, conservatively assume true.
  if (Start->getParent() != End->getParent())
    return true;

  return std::any_of(Start, End, [](const MachineInstr &MI) {
    return any_of(MI.implicit_operands(), [](const MachineOperand &MO) {
      return MO.isReg() && MO.isDef() && MO.getReg() == EraVM::Flags;
    });
  });
}

bool EraVMOptimizeSelectPreRA::isSupportedMI(const MachineInstr &MI,
                                             unsigned Reg) const {
  auto Op = MI.getOpcode();

  // For commutative opcode, no need to check its operands. For non-commutative
  // ones we need to make sure that the output of SELECT is the second input
  // operand, because `a = a - 0` is neutral while `a = 0 - a` is not.
  if (((Op == EraVM::ADDrrr_s || Op == EraVM::ORrrr_s) ||
       (NonCommOpSet.count(Op) && EraVM::in1Iterator(MI)->getReg() == Reg)) &&
      (getImmOrCImm(*EraVM::ccIterator(MI)) == EraVMCC::COND_NONE))
    return true;

  return false;
}

unsigned
EraVMOptimizeSelectPreRA::getFoldedMIOp(EraVM::ArgumentKind Kind,
                                        const MachineInstr &SelectMI,
                                        const MachineInstr &UseMI) const {
  unsigned NewOp = 0;
  auto OpSelect = SelectMI.getOpcode();
  auto OpUseMI = UseMI.getOpcode();
  switch (EraVM::argumentType(Kind, OpSelect)) {
  case EraVM::ArgumentType::Register:
    NewOp = EraVM::getWithRRInAddrMode(OpUseMI);
    break;
  case EraVM::ArgumentType::Immediate:
    NewOp = EraVM::getWithIRInAddrMode(OpUseMI);
    break;
  case EraVM::ArgumentType::Code:
    NewOp = EraVM::getWithCRInAddrMode(OpUseMI);
    break;
  case EraVM::ArgumentType::Stack:
    NewOp = EraVM::getWithSRInAddrMode(OpUseMI);
    break;
  default:
    llvm_unreachable("Unexpected argument type");
  }

  // For non-commutative opcode, need to swap.
  if (NonCommOpSet.count(OpUseMI)) {
    NewOp = EraVM::getWithInsSwapped(NewOp);
  }

  return NewOp;
}

MachineInstr *EraVMOptimizeSelectPreRA::getFoldedInst(
    MachineInstr &ToFold, const MachineInstr &FlagMI,
    iterator_range<MachineInstr::const_mop_iterator> In0, Register In1,
    unsigned NewOpcode, unsigned NewCC, Register TieReg) const {
  auto NewMI =
      BuildMI(*ToFold.getParent(), ToFold, ToFold.getDebugLoc(),
              TII->get(NewOpcode), EraVM::out0Iterator(ToFold)->getReg());
  EraVM::copyOperands(NewMI, In0);
  NewMI.addReg(In1)
      .addImm(NewCC)
      .addReg(TieReg, RegState::Implicit)
      .addReg(EraVM::Flags, RegState::Implicit);

  // Add tie to ensure those two operands will get same reg
  // after RA pass. This is the key to make transformation in this pass
  // correct.
  NewMI->tieOperands(0, NewMI->getNumOperands() - 2);

  // Set the flags.
  NewMI->setFlags(FlagMI.getFlags());

  // Set memory references.
  NewMI.cloneMemRefs(FlagMI);
  return NewMI;
}

bool EraVMOptimizeSelectPreRA::tryFoldSelectZero(MachineBasicBlock &MBB) {
  SmallPtrSet<MachineInstr *, 4> ToRemove;
  for (auto &MI : MBB) {
    if (!TII->isSel(MI))
      continue;

    const auto *const In0Select = EraVM::in0Iterator(MI);
    const auto *const In1Select = EraVM::in1Iterator(MI);
    auto In0Type =
        EraVM::argumentType(EraVM::ArgumentKind::In0, MI.getOpcode());
    auto In1Type =
        EraVM::argumentType(EraVM::ArgumentKind::In1, MI.getOpcode());

    // One of SELECT input values should be zero.
    if (!(In0Type == EraVM::ArgumentType::Immediate &&
          In0Select->getCImm()->isZero()) &&
        !(In1Type == EraVM::ArgumentType::Immediate &&
          In1Select->getCImm()->isZero()))
      continue;

    // Should be only one instruction using the output of SELECT.
    const auto *const Out0Select = EraVM::out0Iterator(MI);
    if (!RegInfo->hasOneNonDBGUser(Out0Select->getReg()))
      continue;

    MachineInstr &UseMI = *RegInfo->use_instr_nodbg_begin(Out0Select->getReg());

    // Start to check whether we can fold SELECT into its sole user.
    if (!isSupportedMI(UseMI, Out0Select->getReg()))
      continue;

    // If UseMI is already folded, bail out early.
    if (ToRemove.count(&UseMI))
      continue;

    // [SELECT, UseMI), the flag register used by SELECT shouldn't be
    // redefined in between.
    if (hasFlagsDefBetween(MI.getIterator(), UseMI.getIterator()))
      continue;

    // Start to fold SELECT into its sole user.
    // Figure out the final CC code.
    bool In0IsZeroSelect = (In0Type == EraVM::ArgumentType::Immediate &&
                            In0Select->getCImm()->isZero());

    auto CCSelect = getImmOrCImm(*EraVM::ccIterator(MI));
    auto CCNewMI = In0IsZeroSelect ? InverseCond[CCSelect] : CCSelect;

    // Pick the non-zero input operand of SELECT.
    auto NonZeroOpnd =
        In0IsZeroSelect ? EraVM::in1Range(MI) : EraVM::in0Range(MI);

    auto Kind =
        In0IsZeroSelect ? EraVM::ArgumentKind::In1 : EraVM::ArgumentKind::In0;

    // Figure out the opcode for folded MI.
    auto NewOp = getFoldedMIOp(Kind, MI, UseMI);

    // The UseMI has two inputs, pick the one that is not
    // the output of SELECT.
    const auto *NonSelectOutOpnd =
        (EraVM::in0Iterator(UseMI)->getReg() == Out0Select->getReg())
            ? EraVM::in1Iterator(UseMI)
            : EraVM::in0Iterator(UseMI);

    // Now we fold the SELECT with its sole user.
    auto *NewMI =
        getFoldedInst(UseMI, UseMI, NonZeroOpnd, NonSelectOutOpnd->getReg(),
                      NewOp, CCNewMI, NonSelectOutOpnd->getReg());
    LLVM_DEBUG(dbgs() << "== Folding select:"; MI.dump();
               dbgs() << "          and use:"; UseMI.dump();
               dbgs() << "             into:"; NewMI->dump(););

    ToRemove.insert(&UseMI);
    ToRemove.insert(&MI);
  }

  for (auto *MI : ToRemove)
    MI->eraseFromParent();

  return !ToRemove.empty();
}

bool EraVMOptimizeSelectPreRA::tryFoldAddToSelect(MachineBasicBlock &MBB) {
  SmallVector<MachineInstr *, 16> ToRemove;
  SmallPtrSet<MachineInstr *, 16> UsesToUpdate;

  // 1. Collect all instructions to be combined.
  for (auto &MI : MBB) {
    if (!TII->isAdd(MI) || TII->getCCCode(MI) != EraVMCC::COND_NONE ||
        !EraVM::hasRROutAddressingMode(MI))
      continue;

    // It's expected that if there are more uses of add, it's very unlikely that
    // all of them are select instruction where folding is feasible.
    Register OutAddReg = EraVM::out0Iterator(MI)->getReg();
    if (!RegInfo->hasOneNonDBGUser(OutAddReg))
      continue;

    MachineInstr &UseMI = *RegInfo->use_instr_nodbg_begin(OutAddReg);
    if (UsesToUpdate.count(&UseMI))
      continue;

    SmallSet<Register, 2> InAddRegs;
    InAddRegs.insert(EraVM::in1Iterator(MI)->getReg());
    if (EraVM::hasRRInAddressingMode(MI))
      InAddRegs.insert(EraVM::in0Iterator(MI)->getReg());

    // In order to fold add to select, we expect that other input of a select
    // instruction is matching with one of the add inputs.
    if (UseMI.getOpcode() != EraVM::SELrrr ||
        (!InAddRegs.count(EraVM::in0Iterator(UseMI)->getReg()) &&
         !InAddRegs.count(EraVM::in1Iterator(UseMI)->getReg())))
      continue;

    bool OutAddIsIn1Use = EraVM::out0Iterator(MI)->getReg() ==
                          EraVM::in1Iterator(UseMI)->getReg();
    auto CC = getImmOrCImm(*EraVM::ccIterator(UseMI));
    auto CCNewMI = OutAddIsIn1Use ? InverseCond[CC] : CC;
    Register TieReg = OutAddIsIn1Use ? EraVM::in0Iterator(UseMI)->getReg()
                                     : EraVM::in1Iterator(UseMI)->getReg();

    auto *NewMI = getFoldedInst(UseMI, MI, EraVM::in0Range(MI),
                                EraVM::in1Iterator(MI)->getReg(),
                                MI.getOpcode(), CCNewMI, TieReg);
    LLVM_DEBUG(dbgs() << "== Folding add:"; MI.dump();
               dbgs() << "       and use:"; UseMI.dump();
               dbgs() << "          into:"; NewMI->dump(););

    UsesToUpdate.insert(&UseMI);
    ToRemove.emplace_back(&MI);
    ToRemove.emplace_back(&UseMI);
  }

  for (auto *MI : ToRemove)
    MI->eraseFromParent();

  return !ToRemove.empty();
}

bool EraVMOptimizeSelectPreRA::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM OPTIMIZE SELECT PRERA **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  RegInfo = &MF.getRegInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    Changed |= tryFoldSelectZero(MBB);
    Changed |= tryFoldAddToSelect(MBB);
  }
  return Changed;
}

/// createEraVMOptimizeSelectPreRAOperandsPass - returns an instance of the
/// optimize select preRA pass.
FunctionPass *llvm::createEraVMOptimizeSelectPreRAPass() {
  return new EraVMOptimizeSelectPreRA();
}
