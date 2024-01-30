//===-- EraVMOptimizeSelectCmpPreRA.cpp - Optimize select preRA -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pre-ra pass to optimize SELECT and CMP instructions.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-opt-select-cmp-prera"
#define ERAVM_OPT_SELECT_CMP_NAME "EraVM select and cmp optimization preRA"

namespace {

class EraVMOptimizeSelectCmpPreRA : public MachineFunctionPass {
public:
  static char ID;
  EraVMOptimizeSelectCmpPreRA() : MachineFunctionPass(ID) {
    initializeEraVMOptimizeSelectCmpPreRAPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_OPT_SELECT_CMP_NAME; }

private:
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

  bool isConstAdd(const MachineInstr &MI) const;
  bool isImmAdd(const MachineInstr &MI) const;
  bool isRegAdd(const MachineInstr &MI) const;
  bool isImmSubWithFlags(const MachineInstr &MI) const;
  bool isImmSubWithoutFlags(const MachineInstr &MI) const;
  bool isConstSub(const MachineInstr &MI) const;
  bool isConstAddIdenticalToConstSub(const MachineInstr &Add,
                                     const MachineInstr &Sub) const;
  bool areSubsIdentical(const MachineInstr &FirstSub,
                        const MachineInstr &SecondSub) const;
  APInt getConstFromCP(unsigned CPI) const;
  bool tryFoldAddToSelect(MachineBasicBlock &MBB);
  bool tryReplaceOpWithCmp(MachineBasicBlock &MBB);

  const EraVMInstrInfo *TII{};
  MachineRegisterInfo *RegInfo{};
  MachineConstantPool *MCP{};
};

char EraVMOptimizeSelectCmpPreRA::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMOptimizeSelectCmpPreRA, DEBUG_TYPE,
                ERAVM_OPT_SELECT_CMP_NAME, false, false)

bool EraVMOptimizeSelectCmpPreRA::hasFlagsDefBetween(
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

bool EraVMOptimizeSelectCmpPreRA::isSupportedMI(const MachineInstr &MI,
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
EraVMOptimizeSelectCmpPreRA::getFoldedMIOp(EraVM::ArgumentKind Kind,
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

bool EraVMOptimizeSelectCmpPreRA::tryFoldSelectZero(MachineBasicBlock &MBB) {
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
    auto NewMI = BuildMI(*UseMI.getParent(), &UseMI, UseMI.getDebugLoc(),
                         TII->get(NewOp), EraVM::out0Iterator(UseMI)->getReg());

    EraVM::copyOperands(NewMI, NonZeroOpnd);

    NewMI.addReg(NonSelectOutOpnd->getReg())
        .addImm(CCNewMI)
        .addReg(NonSelectOutOpnd->getReg(), RegState::Implicit)
        .addReg(EraVM::Flags, RegState::Implicit);

    // Add tie to ensure those two operands will get same reg
    // after RA pass. This is the key to make transformation in this pass
    // correct.
    NewMI->tieOperands(0, NewMI->getNumOperands() - 2);

    // Set the flags.
    NewMI->setFlags(UseMI.getFlags());

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

bool EraVMOptimizeSelectCmpPreRA::isRegAdd(const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::ADDrrr_s &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMOptimizeSelectCmpPreRA::isConstAdd(const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::ADDcrr_s && EraVM::in0Iterator(MI)->isCPI() &&
         (EraVM::in0Iterator(MI) + 1)->isImm() &&
         !(EraVM::in0Iterator(MI) + 1)->getImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMOptimizeSelectCmpPreRA::isImmAdd(const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::ADDirr_s &&
         EraVM::in0Iterator(MI)->isCImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMOptimizeSelectCmpPreRA::isImmSubWithoutFlags(
    const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::SUBxrr_s &&
         EraVM::in0Iterator(MI)->isCImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMOptimizeSelectCmpPreRA::isImmSubWithFlags(
    const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::SUBxrr_v &&
         EraVM::in0Iterator(MI)->isCImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

bool EraVMOptimizeSelectCmpPreRA::isConstSub(const MachineInstr &MI) const {
  return MI.getOpcode() == EraVM::SUByrr_v && EraVM::in0Iterator(MI)->isCPI() &&
         (EraVM::in0Iterator(MI) + 1)->isImm() &&
         !(EraVM::in0Iterator(MI) + 1)->getImm() &&
         TII->getCCCode(MI) == EraVMCC::COND_NONE;
}

APInt EraVMOptimizeSelectCmpPreRA::getConstFromCP(unsigned CPI) const {
  assert(CPI < MCP->getConstants().size() && "Invalid constpool index");
  const MachineConstantPoolEntry &CPE = MCP->getConstants()[CPI];
  assert(!CPE.isMachineConstantPoolEntry() && "Invalid constpool entry");
  APInt Const = cast<ConstantInt>(CPE.Val.ConstVal)->getValue();
  assert(Const.getBitWidth() == 256 && "Invalid constant bitwidth");
  return Const;
}

bool EraVMOptimizeSelectCmpPreRA::areSubsIdentical(
    const MachineInstr &FirstSub, const MachineInstr &SecondSub) const {
  if (!isImmSubWithoutFlags(FirstSub) || !isImmSubWithFlags(SecondSub) ||
      EraVM::in1Iterator(FirstSub)->getReg() !=
          EraVM::in1Iterator(SecondSub)->getReg())
    return false;

  uint64_t FirstSubImm =
      EraVM::in0Iterator(FirstSub)->getCImm()->getZExtValue();
  uint64_t SecondSubImm =
      EraVM::in0Iterator(SecondSub)->getCImm()->getZExtValue();
  return FirstSubImm == SecondSubImm;
}

bool EraVMOptimizeSelectCmpPreRA::isConstAddIdenticalToConstSub(
    const MachineInstr &Add, const MachineInstr &Sub) const {
  if (!isConstAdd(Add) || !isConstSub(Sub) ||
      EraVM::in1Iterator(Add)->getReg() != EraVM::in1Iterator(Sub)->getReg())
    return false;

  APInt AddConst = getConstFromCP(EraVM::in0Iterator(Add)->getIndex());
  APInt SubConst = getConstFromCP(EraVM::in0Iterator(Sub)->getIndex());
  return (AddConst + SubConst).isZero();
}

bool EraVMOptimizeSelectCmpPreRA::tryFoldAddToSelect(MachineBasicBlock &MBB) {
  SmallVector<std::pair<MachineInstr *, MachineInstr *>, 16> Deleted;
  SmallPtrSet<MachineInstr *, 16> UsesToUpdate;

  // 1. Collect all instructions to be combined.
  for (auto &MI : MBB) {
    if (!isConstAdd(MI) && !isImmAdd(MI) && !isRegAdd(MI))
      continue;

    Register OutAddReg = EraVM::out0Iterator(MI)->getReg();
    if (!RegInfo->hasOneNonDBGUser(OutAddReg))
      continue;

    MachineInstr &UseMI = *RegInfo->use_instr_nodbg_begin(OutAddReg);
    if (UsesToUpdate.count(&UseMI) || MI.getParent() != UseMI.getParent())
      continue;

    SmallSet<Register, 2> InRegs;
    InRegs.insert(EraVM::in1Iterator(MI)->getReg());
    if (EraVM::hasRRInAddressingMode(MI))
      InRegs.insert(EraVM::in0Iterator(MI)->getReg());

    if (UseMI.getOpcode() != EraVM::SELrrr ||
        (!InRegs.count(EraVM::in0Iterator(UseMI)->getReg()) &&
         !InRegs.count(EraVM::in1Iterator(UseMI)->getReg())))
      continue;

    UsesToUpdate.insert(&UseMI);
    Deleted.emplace_back(&MI, &UseMI);
  }

  // 2. Combine.
  for (auto [Add, Use] : Deleted) {
    bool OutAddIsIn1Use = EraVM::out0Iterator(*Add)->getReg() ==
                          EraVM::in1Iterator(*Use)->getReg();
    auto CC = getImmOrCImm(*EraVM::ccIterator(*Use));
    auto CCNewMI = OutAddIsIn1Use ? InverseCond[CC] : CC;
    Register TieReg;
    if (EraVM::hasRRInAddressingMode(*Add))
      TieReg = OutAddIsIn1Use ? EraVM::in0Iterator(*Use)->getReg()
                              : EraVM::in1Iterator(*Use)->getReg();
    else
      TieReg = EraVM::in1Iterator(*Add)->getReg();

    // Create new instruction.
    auto NewMI = BuildMI(*Use->getParent(), Use, Use->getDebugLoc(),
                         TII->get(Add->getOpcode()),
                         EraVM::out0Iterator(*Use)->getReg());
    EraVM::copyOperands(NewMI, EraVM::in0Range(*Add));
    NewMI.addReg(EraVM::in1Iterator(*Add)->getReg())
        .addImm(CCNewMI)
        .addReg(TieReg, RegState::Implicit)
        .addReg(EraVM::Flags, RegState::Implicit);

    // Add tie to ensure those two operands will get same reg
    // after RA pass. This is the key to make transformation in this pass
    // correct.
    NewMI->tieOperands(0, NewMI->getNumOperands() - 2);

    // Set the flags.
    NewMI->setFlags(Add->getFlags());

    LLVM_DEBUG(dbgs() << "== Combine add:"; Add->dump();
               dbgs() << "       and use:"; Use->dump();
               dbgs() << "          into:"; NewMI->dump(););
    Use->eraseFromParent();
    Add->eraseFromParent();
  }
  return !Deleted.empty();
}

bool EraVMOptimizeSelectCmpPreRA::tryReplaceOpWithCmp(MachineBasicBlock &MBB) {
  if (MBB.empty())
    return false;

  bool Changed = false;
  for (auto &MI : make_early_inc_range(drop_end(MBB))) {
    auto &NextMI = *std::next(MI.getIterator());
    if (!isConstAddIdenticalToConstSub(MI, NextMI) &&
        !areSubsIdentical(MI, NextMI))
      continue;

    LLVM_DEBUG(dbgs() << "== Replacing inst:"; MI.dump();
               dbgs() << "             with:"; NextMI.dump(););
    RegInfo->replaceRegWith(EraVM::out0Iterator(MI)->getReg(),
                            EraVM::out0Iterator(NextMI)->getReg());
    MI.eraseFromParent();
    Changed = true;
  }
  return Changed;
}

bool EraVMOptimizeSelectCmpPreRA::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM OPTIMIZE SELECT AND CMP PRERA **********\n"
             << "********** Function: " << MF.getName() << '\n');

  MCP = MF.getConstantPool();
  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  RegInfo = &MF.getRegInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    Changed |= tryFoldSelectZero(MBB);
    Changed |= tryReplaceOpWithCmp(MBB);
    Changed |= tryFoldAddToSelect(MBB);
  }
  return Changed;
}

/// createEraVMOptimizeSelectCmpPreRAOperandsPass - returns an instance of the
/// optimize select and cmp preRA pass.
FunctionPass *llvm::createEraVMOptimizeSelectCmpPreRAPass() {
  return new EraVMOptimizeSelectCmpPreRA();
}
