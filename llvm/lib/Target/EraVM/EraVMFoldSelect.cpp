//===-- EraVMFoldSelect.cpp - Fold select with its user ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that tries to fold SELECT instruction (which is
// already expanded into two instructions: add and add.cc here) with
// a foldable instruction. Once it is folded with SELECT(add/add.cc), we will
// end up with two or one instructions.
//
// The combinations of SELECT(add/add.cc) and foldable instruction are so
// complicated, for now only simple cases are handled, leaving TODOs as below:
//
// TODO: not handled when (add/add.cc) and foldable instruction are in different
//       basick block. The correct Global DEF/USE info are required.
//
// TODO: for the opcode of foldable instruction, only ADD/OR/XOR/AND are
//       supported, because they are commutative and thus easy to handle.
//       the MUL is not supported.
//
// TODO: since we are folding two instructions, their InAddressingMode matter.
//       to keep things simple, if their InAddressingMode can not be folded into
//       RR or IR mode, we give up.
//
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-fold-select"
#define ERAVM_FOLD_SELECT_NAME "EraVM fold select"

namespace {

/// How to fold the instructions.
enum FoldingDirection {
  // Invalid case
  Fold_INVALID = 0,

  // Fold the foldable inst backward into the add/add.cc
  Fold_Backward = 1,

  // Fold the add/add.cc forward into the foldable inst
  Fold_Forward = 2,

  // Both are ok.
  Fold_Both = 3,
};

/// Record information about how to do the folding.
/// members are populated during the checks.
struct FoldingInfoEntryType {
  MachineInstr *MovMI;
  MachineInstr *MovccMI;
  MachineInstr *FoldingMI;
  // drop the instruction like 'add 0, X, X'
  bool DropMov;
  FoldingDirection Direction;
  unsigned FoldMIOp;
  unsigned FoldMIccOp;
};

class EraVMFoldSelect : public MachineFunctionPass {
public:
  static char ID;
  EraVMFoldSelect() : MachineFunctionPass(ID) {
    initializeEraVMFoldSelectPass(*PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &Fn) override;
  StringRef getPassName() const override { return ERAVM_FOLD_SELECT_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<ReachingDefAnalysis>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  /// return true if the pattern is a conditional move which is expanded from
  /// SELECT instruction.
  /// `add.xx %Y, r0, %rb`
  bool isMoveCC(const MachineInstr &MI) const;

  /// return true if the pattern is a unconditional move which is expanded from
  /// SELECT instruction.
  /// `add.xx %Y, r0, %rb`
  bool isMove(const MachineInstr &MI) const;

  /// return true if those two MOV instructions are the results of expanding
  /// SELECT instruction.
  bool isExpandedSelect(const MachineInstr &FirstMov,
                        const MachineInstr &SecondMov) const;

  /// The MI is the one we are going to fold with SELECT, return true if its
  /// opcode is the supported one.
  bool isSupportedMI(const MachineInstr &MI) const;

  /// Perform a basic check on register overlapping, return true if it's
  /// safe to fold. Case like below are not safe to fold:
  ///   add    X, R0, T
  ///   add.cc Y, R0, T
  /// with
  ///   op     Z, T, Z
  /// If fold them, we will end up with wrong result:
  ///   op    X, Z, Z
  ///   op.cc Y, Z, Z
  bool checkRegOverlap(const MachineInstr &FoldableMI,
                       FoldingInfoEntryType &FoldingEntry) const;

  /// Perform a bunch of checks to attempt to find a foldable instruction.
  /// If we can find one, return true and save information about how to
  /// fold in FoldingEntry.
  bool findFoldingMI(FoldingInfoEntryType &FoldingEntry) const;

  /// The labor function which does actual folding work.
  void foldMI(MachineInstr *MI, MachineInstr *FoldingMI, unsigned FoldedMIOp,
              FoldingDirection Direction);

  /// Fold the instructions based on information in FoldingEntry. Usually three
  /// jobs included: fold the unconditional move, fold the conditional move,
  /// record them for deleting.
  void doMIFold(const FoldingInfoEntryType &FoldingEntry,
                SmallPtrSet<MachineInstr *, 4> &DeletedMI);

  const EraVMInstrInfo *TII{};
  ReachingDefAnalysis *RDA{};
};

char EraVMFoldSelect::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(EraVMFoldSelect, DEBUG_TYPE, ERAVM_FOLD_SELECT_NAME,
                      false, false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_END(EraVMFoldSelect, DEBUG_TYPE, ERAVM_FOLD_SELECT_NAME, false,
                    false)

using ForwardIter = MachineBasicBlock::const_iterator;
using BackwardIter = MachineBasicBlock::const_reverse_iterator;

/// If the folding instruction isn't next to the SELECT instruction,
/// folding them requires moving them, thus the check on liveness
/// is required.
///
/// This function is a mimic to the isSafeToRemove function in RDA module,
/// except that we drop the check on side effects in order to find more
/// chances on folding.
///
/// The folding information is recoded in FoldingEntry. The SELECT instruction
/// always comes first than the folding instruction in a basic block.
/// However there are two cases to fold them:
///   fold SELECT forwardly with the folding instruction
///   fold the folding instruction backward into the SELECT
/// Hence we need two iterators here.
template <typename Iterator>
static bool doLivenessCheck(const FoldingInfoEntryType &FoldingEntry) {
  SmallSet<int, 2> Defs;
  SmallSet<int, 2> Uses;
  SmallPtrSet<MachineInstr *, 2> StartInsts;
  MachineInstr *StartPos, *EndPos;
  if (std::is_same<Iterator, ForwardIter>::value) {
    StartInsts = {FoldingEntry.MovMI, FoldingEntry.MovccMI};
    StartPos = FoldingEntry.MovccMI;
    EndPos = FoldingEntry.FoldingMI;
  } else {
    StartInsts = {FoldingEntry.FoldingMI};
    StartPos = FoldingEntry.FoldingMI;
    EndPos = FoldingEntry.MovccMI;
  }

  // Collect the Use and Def info before start to check the liveness.
  for (const auto *MI : StartInsts) {
    for (auto &MO : MI->operands()) {
      if (!MO.isReg())
        continue;
      if (MO.isDef())
        Defs.insert(MO.getReg());
      else if (MO.isUse())
        Uses.insert(MO.getReg());
    }
  }

  // Iterate the instructions inbetween to check the liveness.
  for (auto I = ++Iterator(StartPos), E = Iterator(EndPos); I != E; ++I) {
    for (auto &MO : I->operands()) {
      if (!MO.isReg())
        continue;
      if ((MO.isDef() &&
           (Uses.count(MO.getReg()) || Defs.count(MO.getReg()))) ||
          (MO.isUse() && Defs.count(MO.getReg())))
        return false;
    }
  }

  return true;
}

/// Do liveness check to see if it is safe to fold. Also decide how to
/// fold if it is safe to fold.
/// Case like below should fold backward:
///   add    X, R0, T
///   add.cc Y, R0, T
///   ...some inst that will set flag register
///   op  Z, T, S
///
/// The correct way is to fold them like below:
///   op    X, Z, S
///   op.cc Y, Z, S
///   ...some inst that will set flag register
///
static bool checkRegLiveness(FoldingInfoEntryType &FoldingEntry) {
  if (doLivenessCheck<ForwardIter>(FoldingEntry)) {
    FoldingEntry.Direction = Fold_Forward;
    return true;
  }

  if (doLivenessCheck<BackwardIter>(FoldingEntry)) {
    FoldingEntry.Direction = Fold_Backward;
    return true;
  }

  return false;
}

/// Folding addressing modes IR with IR will endup with II addressing
/// mode, this is not supported right now.
/// Once we finish the folding, the resulted instructions should be
/// RR or IR for InAddressingMode.
/// Thus case like below are not folded:
///   add immX, R0, T
///   ...
///   op  immZ, T, S
///
/// If the InAddressingMode of two input instructions are acceptable,
/// the opcode of folding them is figured out and recorded as well.
static bool checkAddressingMode(FoldingInfoEntryType &FoldingEntry) {
  MachineInstr *MovMI = FoldingEntry.MovMI;
  MachineInstr *MovccMI = FoldingEntry.MovccMI;
  MachineInstr *FoldingMI = FoldingEntry.FoldingMI;
  bool OkToFoldMov = true, OkToFoldMovcc = true;

  if (EraVM::hasRRInAddressingMode(MovMI->getOpcode()) &&
      EraVM::hasRRInAddressingMode(FoldingMI->getOpcode())) {
    FoldingEntry.FoldMIOp = FoldingMI->getOpcode();
  } else if ((EraVM::hasRRInAddressingMode(MovMI->getOpcode()) &&
              EraVM::hasIRInAddressingMode(FoldingMI->getOpcode())) ||
             (EraVM::hasIRInAddressingMode(MovMI->getOpcode()) &&
              EraVM::hasRRInAddressingMode(FoldingMI->getOpcode()))) {
    FoldingEntry.FoldMIOp = EraVM::getWithIRInAddrMode(FoldingMI->getOpcode());
  } else {
    OkToFoldMov = false;
  }

  if (EraVM::hasRRInAddressingMode(MovccMI->getOpcode()) &&
      EraVM::hasRRInAddressingMode(FoldingMI->getOpcode())) {
    FoldingEntry.FoldMIccOp = FoldingMI->getOpcode();
  } else if ((EraVM::hasRRInAddressingMode(MovccMI->getOpcode()) &&
              EraVM::hasIRInAddressingMode(FoldingMI->getOpcode())) ||
             (EraVM::hasIRInAddressingMode(MovccMI->getOpcode()) &&
              EraVM::hasRRInAddressingMode(FoldingMI->getOpcode()))) {
    FoldingEntry.FoldMIccOp =
        EraVM::getWithIRInAddrMode(FoldingMI->getOpcode());
  } else {
    OkToFoldMovcc = false;
  }

  if (!OkToFoldMov || !OkToFoldMovcc)
    return false;

  return true;
}

bool EraVMFoldSelect::isMoveCC(const MachineInstr &MI) const {
  if (!TII->isAdd(MI) || !EraVM::hasRROutAddressingMode(MI) ||
      EraVM::hasSRInAddressingMode(MI) ||
      getImmOrCImm(*EraVM::ccIterator(MI)) == EraVMCC::COND_NONE)
    return false;

  const auto *const In1 = EraVM::in1Iterator(MI);

  if (!In1->isReg())
    return false;

  return In1->getReg() == EraVM::R0;
}

bool EraVMFoldSelect::isMove(const MachineInstr &MI) const {
  if (!TII->isAdd(MI) || !EraVM::hasRROutAddressingMode(MI) ||
      EraVM::hasSRInAddressingMode(MI) ||
      getImmOrCImm(*EraVM::ccIterator(MI)) != EraVMCC::COND_NONE)
    return false;

  const auto *const In1 = EraVM::in1Iterator(MI);

  if (!In1->isReg())
    return false;

  return In1->getReg() == EraVM::R0;
}

bool EraVMFoldSelect::isExpandedSelect(const MachineInstr &FirstMov,
                                       const MachineInstr &SecondMov) const {
  if (!isMoveCC(SecondMov) || !isMove(FirstMov))
    return false;

  const auto *const Out1 = EraVM::out0Iterator(FirstMov);
  const auto *const Out2 = EraVM::out0Iterator(SecondMov);

  if (Out1->getReg() != Out2->getReg())
    return false;

  return true;
}

bool EraVMFoldSelect::isSupportedMI(const MachineInstr &MI) const {
  if (!TII->isAdd(MI) && !TII->isOr(MI) && !TII->isXor(MI) && !TII->isAnd(MI))
    return false;

  // Don't fold instruction that sets flags with SELECT instruction.
  if (EraVMInstrInfo::isFlagSettingInstruction(MI.getOpcode()))
    return false;

  return true;
}

bool EraVMFoldSelect::checkRegOverlap(
    const MachineInstr &FoldingMI, FoldingInfoEntryType &FoldingEntry) const {
  const auto *const OutMov = EraVM::out0Iterator(*FoldingEntry.MovMI);
  const auto *const In0Mov = EraVM::in0Iterator(*FoldingEntry.MovMI);
  const auto *const OutFolding = EraVM::out0Iterator(FoldingMI);
  const auto *const In0Folding = EraVM::in0Iterator(FoldingMI);
  const auto *const In1Folding = EraVM::in1Iterator(FoldingMI);

  // The most general pattern for SELECT and FoldingMI are like blow:
  //
  // add     X, R0, T
  // add.cc  Y, R0, T
  // op      Z, T,  S  # FoldingMI
  //
  // The T is used as transitive purpose and won't be needed once the
  // above three instructions can be folded.
  // If Z and S are register operands and share same register number,
  // then we shouldn't fold them, otherwise we will end up with below
  // wrong code sequence:
  // op    X, S, S
  // op.cc Y, S, S
  //
  // However if op is ADD and X is zero. After fold, we will get:
  // ADD    0, S, S
  // ADD.cc Y, S, S
  //
  // This is correct and the first ADD becomes unnecssary and can be
  // dropped.

  // If the output of folding instruction isn't register, no need to do
  // current check.
  if (EraVM::hasSROutAddressingMode(FoldingMI))
    return true;

  // Find the non-transitive operand and check reg overlapping once it
  // is a register.
  const auto *NonTransitiveOpr = In0Folding;
  if (In0Folding->isReg() && In0Folding->getReg() == OutMov->getReg())
    NonTransitiveOpr = In1Folding;

  if (!NonTransitiveOpr->isReg())
    return true;

  if (NonTransitiveOpr->getReg() != OutFolding->getReg()) {
    return true;
  }

  // A special case: if two instructions can be folded into
  // add 0, X, X
  // then we can just drop it. No need to do current check.
  if (TII->isAdd(FoldingMI) &&
      ((In0Mov->isCImm() && In0Mov->getCImm()->isZero()) ||
       (In0Mov->isImm() && In0Mov->getImm() == 0))) {
    FoldingEntry.DropMov = true;
    return true;
  }

  return false;
}

bool EraVMFoldSelect::findFoldingMI(FoldingInfoEntryType &FoldingEntry) const {
  const auto *const OutOfMov = EraVM::out0Iterator(*FoldingEntry.MovccMI);

  // TODO: handle the case where the foldable instruction is in another BB.
  //
  // Below is to collect the local uses in the same BB.
  SmallPtrSet<MachineInstr *, 4> Uses;
  RDA->getReachingLocalUses(const_cast<MachineInstr *>(FoldingEntry.MovccMI),
                            OutOfMov->getReg(), Uses);

  // The folding instruction should be the only user of the output of SELECT
  // instruction.
  if (Uses.size() != 1)
    return false;

  MachineInstr *FoldingMI = *Uses.begin();

  // The output of SELECT instruction shouldn't be used from another
  // basic block as well.
  //
  // TODO: once the bug of RDA->getGlobalUses is fixed, we should
  // use it instead.
  if (RDA->isReachingDefLiveOut(FoldingMI, OutOfMov->getReg())) {
    return false;
  }

  if (!isSupportedMI(*FoldingMI))
    return false;

  if (!checkRegOverlap(*FoldingMI, FoldingEntry)) {
    return false;
  }

  FoldingEntry.FoldingMI = FoldingMI;
  if (!checkAddressingMode(FoldingEntry)) {
    return false;
  }

  const MachineInstr *MovccNextMI =
      &(*std::next(FoldingEntry.MovccMI->getIterator()));

  // If the folding MI is next to the Movcc, then no bother to do liveness
  // analysis. Should be OK to fold them directly.
  if (MovccNextMI == FoldingMI) {
    FoldingEntry.FoldingMI = FoldingMI;
    FoldingEntry.Direction = Fold_Both;
    return true;
  }

  if (!checkRegLiveness(FoldingEntry)) {
    return false;
  }

  return true;
}

void EraVMFoldSelect::foldMI(MachineInstr *MI, MachineInstr *FoldingMI,
                             unsigned FoldedMIOp, FoldingDirection Direction) {
  const DebugLoc DL = MI->getDebugLoc();
  MachineBasicBlock &MBB = *MI->getParent();

  const auto *const OutMI = EraVM::out0Iterator(*MI);
  const auto *const In0MI = EraVM::in0Iterator(*MI);
  const auto *const OutFoldingMI = EraVM::out0Iterator(*FoldingMI);
  const auto *const In0FoldingMI = EraVM::in0Iterator(*FoldingMI);
  const auto *const In1FoldingMI = EraVM::in1Iterator(*FoldingMI);

  // Get the insert point.
  MachineInstr *InsertPoint = Direction == Fold_Backward ? MI : FoldingMI;

  // Build the trunk of the new MI.
  auto NewFoldedMI = EraVM::hasRROutAddressingMode(*FoldingMI)
                         ? BuildMI(MBB, InsertPoint, DL, TII->get(FoldedMIOp),
                                   OutFoldingMI->getReg())
                         : BuildMI(MBB, InsertPoint, DL, TII->get(FoldedMIOp));

  // Add the first In operand.
  if (In0MI->isReg()) {
    NewFoldedMI.addReg(In0MI->getReg());
  } else if (In0MI->isCImm()) {
    NewFoldedMI.addCImm(In0MI->getCImm());
  } else if (In0MI->isImm()) {
    NewFoldedMI.addImm(In0MI->getImm());
  }

  // Add the second In operand.
  if (In0FoldingMI->isCImm()) {
    NewFoldedMI.addCImm(In0FoldingMI->getCImm());
  } else if (In0FoldingMI->isImm()) {
    NewFoldedMI.addImm(In0FoldingMI->getImm());
  } else if (In0FoldingMI->isReg() &&
             In0FoldingMI->getReg() != OutMI->getReg()) {
    NewFoldedMI.addReg(In0FoldingMI->getReg());
  } else if (In0FoldingMI->isReg() &&
             In0FoldingMI->getReg() == OutMI->getReg()) {
    NewFoldedMI.addReg(In1FoldingMI->getReg());
  }

  // Handle the output part.
  if (!EraVM::hasRROutAddressingMode(*FoldingMI))
    EraVM::copyOperands(NewFoldedMI, EraVM::out0Range(*FoldingMI));

  // Add the CC part.
  NewFoldedMI.add(*EraVM::ccIterator(*MI));

  // Add the implicit part.
  NewFoldedMI.copyImplicitOps(*MI);
}

void EraVMFoldSelect::doMIFold(const FoldingInfoEntryType &FoldingEntry,
                               SmallPtrSet<MachineInstr *, 4> &DeletedMI) {

  MachineInstr *MovMI = FoldingEntry.MovMI;
  MachineInstr *MovccMI = FoldingEntry.MovccMI;

  if (FoldingEntry.DropMov) {
    DeletedMI.insert(MovMI);
  } else {
    foldMI(MovMI, FoldingEntry.FoldingMI, FoldingEntry.FoldMIOp,
           FoldingEntry.Direction);
    DeletedMI.insert(MovMI);
  }

  foldMI(MovccMI, FoldingEntry.FoldingMI, FoldingEntry.FoldMIccOp,
         FoldingEntry.Direction);

  DeletedMI.insert(MovccMI);
  DeletedMI.insert(FoldingEntry.FoldingMI);
}

using MBBIter = MachineBasicBlock::iterator;

bool EraVMFoldSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM FoldSelect optimization **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  RDA = &getAnalysis<ReachingDefAnalysis>();

  std::vector<FoldingInfoEntryType> FoldableInstVec;
  for (MachineBasicBlock &MBB : MF) {
    for (MBBIter I = MBB.begin(); I != std::prev(MBB.end()); I++) {
      MachineInstr &MI = *I;
      MachineInstr &NextMI = *std::next(I);
      FoldingInfoEntryType FoldingEntry = {};

      if (!isExpandedSelect(MI, NextMI))
        continue;

      FoldingEntry.MovMI = &MI;
      FoldingEntry.MovccMI = &NextMI;

      if (findFoldingMI(FoldingEntry)) {
        FoldableInstVec.push_back(FoldingEntry);

        LLVM_DEBUG(dbgs() << "\tFold:\n"; dbgs() << "\t\t"; MI.dump();
                   dbgs() << "\t\t"; NextMI.dump(); dbgs() << "\tWith:\n";
                   dbgs() << "\t\t"; FoldingEntry.FoldingMI->dump(););
      }
    }
  }

  SmallPtrSet<MachineInstr *, 4> DeletedMI;
  for (const auto &FoldingEntry : FoldableInstVec)
    doMIFold(FoldingEntry, DeletedMI);

  for (auto *I : DeletedMI)
    I->eraseFromParent();

  RDA->reset();

  return !DeletedMI.empty();
}

/// createEraVMFoldSelectPass - returns an instance of the select
/// folding pass.
FunctionPass *llvm::createEraVMFoldSelectPass() {
  return new EraVMFoldSelect();
}
