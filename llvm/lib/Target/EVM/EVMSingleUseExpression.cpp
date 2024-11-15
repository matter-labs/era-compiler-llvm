//===-- EVMSingleUseExpression.cpp - Register Stackification --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass reorders instructions to put register uses and defs in an order
// such that they form single-use expression trees. Registers fitting this form
// are then marked as "stackified", meaning references to them are replaced by
// "push" and "pop" from the stack.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h" // for EVM::ARGUMENT_*
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfoImpls.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <iterator>
using namespace llvm;

#define DEBUG_TYPE "evm-single-use-expressions"

namespace {
class EVMSingleUseExpression final : public MachineFunctionPass {
  StringRef getPassName() const override {
    return "EVM Single use expressions";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<MachineDominatorTree>();
    AU.addRequired<LiveIntervals>();
    AU.addPreserved<MachineBlockFrequencyInfo>();
    AU.addPreserved<SlotIndexes>();
    AU.addPreserved<LiveIntervals>();
    AU.addPreservedID(LiveVariablesID);
    AU.addPreserved<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

public:
  static char ID; // Pass identification, replacement for typeid
  EVMSingleUseExpression() : MachineFunctionPass(ID) {}
};
} // end anonymous namespace

char EVMSingleUseExpression::ID = 0;
INITIALIZE_PASS(EVMSingleUseExpression, DEBUG_TYPE,
                "Reorder instructions to use the EVM value stack", false, false)

FunctionPass *llvm::createEVMSingleUseExpression() {
  return new EVMSingleUseExpression();
}

// Decorate the given instruction with implicit operands that enforce the
// expression stack ordering constraints for an instruction which is on
// the expression stack.
static void imposeStackOrdering(MachineInstr *MI) {
  // Write the opaque VALUE_STACK register.
  if (!MI->definesRegister(EVM::VALUE_STACK))
    MI->addOperand(MachineOperand::CreateReg(EVM::VALUE_STACK,
                                             /*isDef=*/true,
                                             /*isImp=*/true));

  // Also read the opaque VALUE_STACK register.
  if (!MI->readsRegister(EVM::VALUE_STACK))
    MI->addOperand(MachineOperand::CreateReg(EVM::VALUE_STACK,
                                             /*isDef=*/false,
                                             /*isImp=*/true));
}

// Convert an IMPLICIT_DEF instruction into an instruction which defines
// a constant zero value.
static void convertImplicitDefToConstZero(MachineInstr *MI,
                                          MachineRegisterInfo &MRI,
                                          const TargetInstrInfo *TII,
                                          MachineFunction &MF,
                                          LiveIntervals &LIS) {
  assert(MI->getOpcode() == TargetOpcode::IMPLICIT_DEF);

  assert(MRI.getRegClass(MI->getOperand(0).getReg()) == &EVM::GPRRegClass);
  MI->setDesc(TII->get(EVM::CONST_I256));
  MI->addOperand(MachineOperand::CreateImm(0));
}

// Determine whether a call to the callee referenced by
// MI->getOperand(CalleeOpNo) reads memory, writes memory, and/or has side
// effects.
static void queryCallee(const MachineInstr &MI, bool &Read, bool &Write,
                        bool &Effects, bool &StackPointer) {
  // All calls can use the stack pointer.
  StackPointer = true;

  const MachineOperand &MO = MI.getOperand(MI.getNumExplicitDefs());
  if (MO.isGlobal()) {
    const Constant *GV = MO.getGlobal();
    if (const auto *GA = dyn_cast<GlobalAlias>(GV))
      if (!GA->isInterposable())
        GV = GA->getAliasee();

    if (const auto *F = dyn_cast<Function>(GV)) {
      if (!F->doesNotThrow())
        Effects = true;
      if (F->doesNotAccessMemory())
        return;
      if (F->onlyReadsMemory()) {
        Read = true;
        return;
      }
    }
  }

  // Assume the worst.
  Write = true;
  Read = true;
  Effects = true;
}

// Determine whether MI reads memory, writes memory, has side effects,
// and/or uses the stack pointer value.
static void query(const MachineInstr &MI, bool &Read, bool &Write,
                  bool &Effects, bool &StackPointer) {
  assert(!MI.isTerminator());

  if (MI.isDebugInstr() || MI.isPosition())
    return;

  // Check for loads.
  if (MI.mayLoad() && !MI.isDereferenceableInvariantLoad())
    Read = true;

  // Check for stores.
  if (MI.mayStore()) {
    Write = true;
  } else if (MI.hasOrderedMemoryRef()) {
    if (!MI.isCall()) {
      Write = true;
      Effects = true;
    }
  }

  // Check for side effects.
  if (MI.hasUnmodeledSideEffects())
    Effects = true;

  // Analyze calls.
  if (MI.isCall()) {
    queryCallee(MI, Read, Write, Effects, StackPointer);
  }
}

// Identify the definition for this register at this point. This is a
// generalization of MachineRegisterInfo::getUniqueVRegDef that uses
// LiveIntervals to handle complex cases.
static MachineInstr *getVRegDef(unsigned Reg, const MachineInstr *Insert,
                                const MachineRegisterInfo &MRI,
                                const LiveIntervals &LIS) {
  // Most registers are in SSA form here so we try a quick MRI query first.
  if (MachineInstr *Def = MRI.getUniqueVRegDef(Reg))
    return Def;

  // MRI doesn't know what the Def is. Try asking LIS.
  if (const VNInfo *ValNo = LIS.getInterval(Reg).getVNInfoBefore(
          LIS.getInstructionIndex(*Insert)))
    return LIS.getInstructionFromIndex(ValNo->def);

  return nullptr;
}

// Test whether Reg, as defined at Def, has exactly one use. This is a
// generalization of MachineRegisterInfo::hasOneUse that uses LiveIntervals
// to handle complex cases.
static bool hasOneUse(unsigned Reg, MachineInstr *Def, MachineRegisterInfo &MRI,
                      MachineDominatorTree &MDT, LiveIntervals &LIS) {
  // Most registers are in SSA form here so we try a quick MRI query first.
  if (MRI.hasOneUse(Reg))
    return true;

  bool HasOne = false;
  const LiveInterval &LI = LIS.getInterval(Reg);
  const VNInfo *DefVNI =
      LI.getVNInfoAt(LIS.getInstructionIndex(*Def).getRegSlot());
  assert(DefVNI);
  for (auto &I : MRI.use_nodbg_operands(Reg)) {
    const auto &Result = LI.Query(LIS.getInstructionIndex(*I.getParent()));
    if (Result.valueIn() == DefVNI) {
      if (!Result.isKill())
        return false;
      if (HasOne)
        return false;
      HasOne = true;
    }
  }
  return HasOne;
}

// Test whether Def is safe and profitable to rematerialize.
static bool shouldRematerialize(const MachineInstr &Def,
                                const MachineRegisterInfo &MRI,
                                LiveIntervals &LIS,
                                const EVMInstrInfo *TII) {
  // FIXME: remateralization of the CALLDATALOAD and ADD instructions
  // is just an ad-hoc solution to more aggressively form single use expressions
  // to eventually decrease runtime stack height, but this can significantly
  // increase a code size.
  unsigned Opcode = Def.getOpcode();
  if (Opcode == EVM::CALLDATALOAD) {
    MachineInstr *DefI = getVRegDef(Def.getOperand(1).getReg(), &Def, MRI, LIS);
    if (!DefI)
      return false;

    return DefI->getOpcode() == EVM::CONST_I256 ? true : false;
  }

  if (Opcode == EVM::ADD) {
    MachineInstr *DefI1 =
        getVRegDef(Def.getOperand(1).getReg(), &Def, MRI, LIS);
    MachineInstr *DefI2 =
        getVRegDef(Def.getOperand(2).getReg(), &Def, MRI, LIS);
    if (!DefI1 || !DefI2)
      return false;

    MachineInstr *DefBase = nullptr;
    if (DefI1->getOpcode() == EVM::CONST_I256)
      DefBase = DefI2;
    else if (DefI2->getOpcode() == EVM::CONST_I256)
      DefBase = DefI1;
    else
      return false;

    const Register BaseReg = DefBase->getOperand(0).getReg();
    if (DefBase != MRI.getUniqueVRegDef(BaseReg))
      return false;

    const Register DefReg = Def.getOperand(0).getReg();
    if (&Def != MRI.getUniqueVRegDef(DefReg))
      return false;

    LiveInterval &DefLI = LIS.getInterval(DefReg);
    LiveInterval &BaseLI = LIS.getInterval(BaseReg);

    LLVM_DEBUG(dbgs() << "Can rematerialize ADD(base, const): " << Def
                      << "DefLI: " << DefLI << ", BaseLI: " << BaseLI << '\n');

    return BaseLI.covers(DefLI);
  }
  return Def.isAsCheapAsAMove() && TII->isTriviallyReMaterializable(Def);
}

// Test whether it's safe to move Def to just before Insert.
// TODO: Compute memory dependencies in a way that doesn't require always
// walking the block.
// TODO: Compute memory dependencies in a way that uses AliasAnalysis to be
// more precise.
static bool isSafeToMove(const MachineOperand *Def, const MachineOperand *Use,
                         const MachineInstr *Insert,
                         const EVMMachineFunctionInfo &MFI,
                         const MachineRegisterInfo &MRI) {
  const MachineInstr *DefI = Def->getParent();
  const MachineInstr *UseI = Use->getParent();
  assert(DefI->getParent() == Insert->getParent());
  assert(UseI->getParent() == Insert->getParent());

  // The first def of a multivalue instruction can be stackified by moving,
  // since the later defs can always be placed into locals if necessary. Later
  // defs can only be stackified if all previous defs are already stackified
  // since ExplicitLocals will not know how to place a def in a local if a
  // subsequent def is stackified. But only one def can be stackified by moving
  // the instruction, so it must be the first one.
  //
  // TODO: This could be loosened to be the first *live* def, but care would
  // have to be taken to ensure the drops of the initial dead defs can be
  // placed. This would require checking that no previous defs are used in the
  // same instruction as subsequent defs.
  if (Def != DefI->defs().begin())
    return false;

  // If any subsequent def is used prior to the current value by the same
  // instruction in which the current value is used, we cannot
  // stackify. Stackifying in this case would require that def moving below the
  // current def in the stack, which cannot be achieved, even with locals.
  for (const auto &SubsequentDef : drop_begin(DefI->defs())) {
    for (const auto &PriorUse : UseI->uses()) {
      if (&PriorUse == Use)
        break;
      if (PriorUse.isReg() && SubsequentDef.getReg() == PriorUse.getReg())
        return false;
    }
  }

  // If moving is a semantic nop, it is always allowed
  const MachineBasicBlock *MBB = DefI->getParent();
  auto NextI = std::next(MachineBasicBlock::const_iterator(DefI));
  for (auto E = MBB->end(); NextI != E && NextI->isDebugInstr(); ++NextI)
    ;
  if (NextI == Insert)
    return true;

  // Don't move ARGUMENT instructions, as stackification pass relies on this.
  if (DefI->getOpcode() == EVM::ARGUMENT)
    return false;

  // Check for register dependencies.
  SmallVector<unsigned, 4> MutableRegisters;
  for (const MachineOperand &MO : DefI->operands()) {
    if (!MO.isReg() || MO.isUndef())
      continue;
    Register Reg = MO.getReg();

    // If the register is dead here and at Insert, ignore it.
    if (MO.isDead() && Insert->definesRegister(Reg) &&
        !Insert->readsRegister(Reg))
      continue;

    if (Register::isPhysicalRegister(Reg)) {
      // Ignore ARGUMENTS; it's just used to keep the ARGUMENT_* instructions
      // from moving down, and we've already checked for that.
      if (Reg == EVM::ARGUMENTS)
        continue;
      // If the physical register is never modified, ignore it.
      if (!MRI.isPhysRegModified(Reg))
        continue;
      // Otherwise, it's a physical register with unknown liveness.
      return false;
    }

    // If one of the operands isn't in SSA form, it has different values at
    // different times, and we need to make sure we don't move our use across
    // a different def.
    if (!MO.isDef() && !MRI.hasOneDef(Reg))
      MutableRegisters.push_back(Reg);
  }

  bool Read = false, Write = false, Effects = false, StackPointer = false;
  query(*DefI, Read, Write, Effects, StackPointer);

  // If the instruction does not access memory and has no side effects, it has
  // no additional dependencies.
  bool HasMutableRegisters = !MutableRegisters.empty();
  if (!Read && !Write && !Effects && !StackPointer && !HasMutableRegisters)
    return true;

  // Scan through the intervening instructions between DefI and Insert.
  MachineBasicBlock::const_iterator D(DefI), I(Insert);
  for (--I; I != D; --I) {
    bool InterveningRead = false;
    bool InterveningWrite = false;
    bool InterveningEffects = false;
    bool InterveningStackPointer = false;
    query(*I, InterveningRead, InterveningWrite, InterveningEffects,
          InterveningStackPointer);
    if (Effects && InterveningEffects)
      return false;
    if (Read && InterveningWrite)
      return false;
    if (Write && (InterveningRead || InterveningWrite))
      return false;
    if (StackPointer && InterveningStackPointer)
      return false;

    for (unsigned Reg : MutableRegisters)
      for (const MachineOperand &MO : I->operands())
        if (MO.isReg() && MO.isDef() && MO.getReg() == Reg)
          return false;
  }

  return true;
}

// Shrink LI to its uses, cleaning up LI.
static void shrinkToUses(LiveInterval &LI, LiveIntervals &LIS) {
  if (LIS.shrinkToUses(&LI)) {
    SmallVector<LiveInterval *, 4> SplitLIs;
    LIS.splitSeparateComponents(LI, SplitLIs);
  }
}

/// A single-use def in the same block with no intervening memory or register
/// dependencies; move the def down and nest it with the current instruction.
static MachineInstr *moveForSingleUse(unsigned Reg, MachineOperand &Op,
                                      MachineInstr *Def, MachineBasicBlock &MBB,
                                      MachineInstr *Insert, LiveIntervals &LIS,
                                      EVMMachineFunctionInfo &MFI,
                                      MachineRegisterInfo &MRI) {
  LLVM_DEBUG(dbgs() << "Move for single use: "; Def->dump());

  MBB.splice(Insert, &MBB, Def);
  LIS.handleMove(*Def);

  if (MRI.hasOneDef(Reg) && MRI.hasOneUse(Reg)) {
    // No one else is using this register for anything so we can just stackify
    // it in place.
    MFI.stackifyVReg(MRI, Reg);
  } else {
    // The register may have unrelated uses or defs; create a new register for
    // just our one def and use so that we can stackify it.
    Register NewReg = MRI.createVirtualRegister(MRI.getRegClass(Reg));
    Def->getOperand(0).setReg(NewReg);
    Op.setReg(NewReg);

    // Tell LiveIntervals about the new register.
    LIS.createAndComputeVirtRegInterval(NewReg);

    // Tell LiveIntervals about the changes to the old register.
    LiveInterval &LI = LIS.getInterval(Reg);
    LI.removeSegment(LIS.getInstructionIndex(*Def).getRegSlot(),
                     LIS.getInstructionIndex(*Op.getParent()).getRegSlot(),
                     /*RemoveDeadValNo=*/true);

    MFI.stackifyVReg(MRI, NewReg);
    LLVM_DEBUG(dbgs() << " - Replaced register: "; Def->dump());
  }

  imposeStackOrdering(Def);
  return Def;
}

/// A trivially cloneable instruction; clone it and nest the new copy with the
/// current instruction.
static MachineInstr *rematerializeCheapDef(
    unsigned Reg, MachineOperand &Op, MachineInstr &Def, MachineBasicBlock &MBB,
    MachineBasicBlock::instr_iterator Insert, LiveIntervals &LIS,
    EVMMachineFunctionInfo &MFI, MachineRegisterInfo &MRI,
    const EVMInstrInfo *TII, const EVMRegisterInfo *TRI) {
  LLVM_DEBUG(dbgs() << "Rematerializing cheap def: "; Def.dump());
  LLVM_DEBUG(dbgs() << " - for use in "; Op.getParent()->dump());

  Register NewReg = MRI.createVirtualRegister(MRI.getRegClass(Reg));
  TII->reMaterialize(MBB, Insert, NewReg, 0, Def, *TRI);
  Op.setReg(NewReg);
  MachineInstr *Clone = &*std::prev(Insert);
  LIS.InsertMachineInstrInMaps(*Clone);
  LIS.createAndComputeVirtRegInterval(NewReg);
  MFI.stackifyVReg(MRI, NewReg);
  imposeStackOrdering(Clone);

  LLVM_DEBUG(dbgs() << " - Cloned to "; Clone->dump());

  // Shrink the interval.
  bool IsDead = MRI.use_empty(Reg);
  if (!IsDead) {
    LiveInterval &LI = LIS.getInterval(Reg);
    shrinkToUses(LI, LIS);
    IsDead = !LI.liveAt(LIS.getInstructionIndex(Def).getDeadSlot());
  }

  // If that was the last use of the original, delete the original.
  // Move or clone corresponding DBG_VALUEs to the 'Insert' location.
  if (IsDead) {
    LLVM_DEBUG(dbgs() << " - Deleting original\n");
    SlotIndex Idx = LIS.getInstructionIndex(Def).getRegSlot();
    LIS.removePhysRegDefAt(MCRegister::from(EVM::ARGUMENTS), Idx);
    LIS.removeInterval(Reg);
    LIS.RemoveMachineInstrFromMaps(Def);
    Def.eraseFromParent();
  }

  return Clone;
}

namespace {
/// A stack for walking the tree of instructions being built, visiting the
/// MachineOperands in DFS order.
class TreeWalkerState {
  using mop_iterator = MachineInstr::mop_iterator;
  using RangeTy = iterator_range<mop_iterator>;
  SmallVector<RangeTy, 4> Worklist;

public:
  explicit TreeWalkerState(MachineInstr *Insert) {
    const iterator_range<mop_iterator> &Range = Insert->explicit_uses();
    if (!Range.empty())
      Worklist.push_back(Range);
  }

  bool done() const { return Worklist.empty(); }

  MachineOperand &pop() {
    RangeTy &Range = Worklist.back();
    MachineOperand &Op = *Range.begin();
    Range = drop_begin(Range);
    if (Range.empty())
      Worklist.pop_back();
    assert((Worklist.empty() || !Worklist.back().empty()) &&
           "Empty ranges shouldn't remain in the worklist");
    return Op;
  }

  /// Push Instr's operands onto the stack to be visited.
  void pushOperands(MachineInstr *Instr) {
    const iterator_range<mop_iterator> &Range(Instr->explicit_uses());
    if (!Range.empty())
      Worklist.push_back(Range);
  }

  /// Some of Instr's operands are on the top of the stack; remove them and
  /// re-insert them starting from the beginning (because we've commuted them).
  void resetTopOperands(MachineInstr *Instr) {
    assert(hasRemainingOperands(Instr) &&
           "Reseting operands should only be done when the instruction has "
           "an operand still on the stack");
    Worklist.back() = Instr->explicit_uses();
  }

  /// Test whether Instr has operands remaining to be visited at the top of
  /// the stack.
  bool hasRemainingOperands(const MachineInstr *Instr) const {
    if (Worklist.empty())
      return false;
    const RangeTy &Range = Worklist.back();
    return !Range.empty() && Range.begin()->getParent() == Instr;
  }

  /// Test whether the given register is present on the stack, indicating an
  /// operand in the tree that we haven't visited yet. Moving a definition of
  /// Reg to a point in the tree after that would change its value.
  ///
  /// This is needed as a consequence of using implicit local.gets for
  /// uses and implicit local.sets for defs.
  bool isOnStack(unsigned Reg) const {
    for (const RangeTy &Range : Worklist)
      for (const MachineOperand &MO : Range)
        if (MO.isReg() && MO.getReg() == Reg)
          return true;
    return false;
  }
};

/// State to keep track of whether commuting is in flight or whether it's been
/// tried for the current instruction and didn't work.
class CommutingState {
  /// There are effectively three states: the initial state where we haven't
  /// started commuting anything and we don't know anything yet, the tentative
  /// state where we've commuted the operands of the current instruction and are
  /// revisiting it, and the declined state where we've reverted the operands
  /// back to their original order and will no longer commute it further.
  bool TentativelyCommuting = false;
  bool Declined = false;

  /// During the tentative state, these hold the operand indices of the commuted
  /// operands.
  unsigned Operand0 = 0, Operand1 = 0;

public:
  /// Stackification for an operand was not successful due to ordering
  /// constraints. If possible, and if we haven't already tried it and declined
  /// it, commute Insert's operands and prepare to revisit it.
  void maybeCommute(MachineInstr *Insert, TreeWalkerState &TreeWalker,
                    const EVMInstrInfo *TII) {
    if (TentativelyCommuting) {
      assert(!Declined &&
             "Don't decline commuting until you've finished trying it");
      // Commuting didn't help. Revert it.
      TII->commuteInstruction(*Insert, /*NewMI=*/false, Operand0, Operand1);
      TentativelyCommuting = false;
      Declined = true;
    } else if (!Declined && TreeWalker.hasRemainingOperands(Insert)) {
      Operand0 = TargetInstrInfo::CommuteAnyOperandIndex;
      Operand1 = TargetInstrInfo::CommuteAnyOperandIndex;
      if (TII->findCommutedOpIndices(*Insert, Operand0, Operand1)) {
        // Tentatively commute the operands and try again.
        TII->commuteInstruction(*Insert, /*NewMI=*/false, Operand0, Operand1);
        TreeWalker.resetTopOperands(Insert);
        TentativelyCommuting = true;
        Declined = false;
      }
    }
  }

  /// Stackification for some operand was successful. Reset to the default
  /// state.
  void reset() {
    TentativelyCommuting = false;
    Declined = false;
  }
};
} // end anonymous namespace

bool EVMSingleUseExpression::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** Single-use expressions **********\n"
                       "********** Function: "
                    << MF.getName() << '\n');

  bool Changed = false;
  MachineRegisterInfo &MRI = MF.getRegInfo();
  EVMMachineFunctionInfo &MFI = *MF.getInfo<EVMMachineFunctionInfo>();
  const auto *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();
  const auto *TRI = MF.getSubtarget<EVMSubtarget>().getRegisterInfo();
  auto &MDT = getAnalysis<MachineDominatorTree>();
  auto &LIS = getAnalysis<LiveIntervals>();

  // Walk the instructions from the bottom up. Currently we don't look past
  // block boundaries, and the blocks aren't ordered so the block visitation
  // order isn't significant, but we may want to change this in the future.
  for (MachineBasicBlock &MBB : MF) {
    // Don't use a range-based for loop, because we modify the list as we're
    // iterating over it and the end iterator may change.
    for (auto MII = MBB.rbegin(); MII != MBB.rend(); ++MII) {
      MachineInstr *Insert = &*MII;
      // Don't nest anything inside an inline asm, because we don't have
      // constraints for $push inputs.
      if (Insert->isInlineAsm())
        continue;

      // Ignore debugging intrinsics.
      if (Insert->isDebugValue())
        continue;

      // Iterate through the inputs in reverse order, since we'll be pulling
      // operands off the stack in LIFO order.
      CommutingState Commuting;
      TreeWalkerState TreeWalker(Insert);
      while (!TreeWalker.done()) {
        MachineOperand &Use = TreeWalker.pop();

        // We're only interested in explicit virtual register operands.
        if (!Use.isReg())
          continue;

        Register Reg = Use.getReg();
        assert(Use.isUse() && "explicit_uses() should only iterate over uses");
        assert(!Use.isImplicit() &&
               "explicit_uses() should only iterate over explicit operands");
        if (Register::isPhysicalRegister(Reg))
          continue;

        // Identify the definition for this register at this point.
        MachineInstr *DefI = getVRegDef(Reg, Insert, MRI, LIS);
        if (!DefI)
          continue;

        // Don't nest an INLINE_ASM def into anything, because we don't have
        // constraints for $pop outputs.
        if (DefI->isInlineAsm())
          continue;

        MachineOperand *Def = DefI->findRegisterDefOperand(Reg);
        assert(Def != nullptr);

        // Decide which strategy to take. Prefer to move a single-use value
        // over cloning it, and prefer cloning over introducing a tee.
        // For moving, we require the def to be in the same block as the use;
        // this makes things simpler (LiveIntervals' handleMove function only
        // supports intra-block moves) and it's MachineSink's job to catch all
        // the sinking opportunities anyway.
        bool SameBlock = DefI->getParent() == &MBB;
        bool CanMove = SameBlock &&
                       isSafeToMove(Def, &Use, Insert, MFI, MRI) &&
                       !TreeWalker.isOnStack(Reg);
        if (CanMove && hasOneUse(Reg, DefI, MRI, MDT, LIS)) {
          Insert =
              moveForSingleUse(Reg, Use, DefI, MBB, Insert, LIS, MFI, MRI);
        } else if (shouldRematerialize(*DefI, MRI, LIS, TII)) {
          if (DefI->getOpcode() == EVM::CALLDATALOAD) {
            MachineInstr *OffsetI = getVRegDef(DefI->getOperand(1).getReg(), DefI, MRI,
                                               LIS);
            assert(OffsetI);
            Insert = rematerializeCheapDef(Reg, Use, *DefI, MBB,
                                           Insert->getIterator(), LIS, MFI,
                                           MRI, TII, TRI);

            MachineOperand &OffsetMO = Insert->getOperand(1);
            Insert = rematerializeCheapDef(OffsetMO.getReg(), OffsetMO, *OffsetI, MBB,
                                           Insert->getIterator(), LIS, MFI,
                                           MRI, TII, TRI);
          } else if (DefI->getOpcode() == EVM::ADD) {
            MachineInstr *DefI1 =
                getVRegDef(DefI->getOperand(1).getReg(), DefI, MRI, LIS);
            MachineInstr *DefI2 =
                getVRegDef(DefI->getOperand(2).getReg(), DefI, MRI, LIS);
            assert(DefI1 && DefI2);

            MachineInstr *OffsetI =
                (DefI1->getOpcode() == EVM::CONST_I256) ? DefI1 : DefI2;
            Insert = rematerializeCheapDef(Reg, Use, *DefI, MBB,
                                           Insert->getIterator(), LIS, MFI, MRI,
                                           TII, TRI);

            MachineOperand &OffsetMO = (DefI1->getOpcode() == EVM::CONST_I256)
                                           ? Insert->getOperand(1)
                                           : Insert->getOperand(2);
            Insert = rematerializeCheapDef(OffsetMO.getReg(), OffsetMO,
                                           *OffsetI, MBB, Insert->getIterator(),
                                           LIS, MFI, MRI, TII, TRI);
          } else {
            Insert = rematerializeCheapDef(Reg, Use, *DefI, MBB,
                                         Insert->getIterator(), LIS, MFI,
                                         MRI, TII, TRI);
          }
        } else {
          // We failed to stackify the operand. If the problem was ordering
          // constraints, Commuting may be able to help.
          if (!CanMove && SameBlock)
            Commuting.maybeCommute(Insert, TreeWalker, TII);
          // Proceed to the next operand.
          continue;
        }

        // Stackifying a multivalue def may unlock in-place stackification of
        // subsequent defs. TODO: Handle the case where the consecutive uses are
        // not all in the same instruction.
        auto *SubsequentDef = Insert->defs().begin();
        auto *SubsequentUse = &Use;
        while (SubsequentDef != Insert->defs().end() &&
               SubsequentUse != Use.getParent()->uses().end()) {
          if (!SubsequentDef->isReg() || !SubsequentUse->isReg())
            break;
          Register DefReg = SubsequentDef->getReg();
          Register UseReg = SubsequentUse->getReg();
          // TODO: This single-use restriction could be relaxed by using tees
          if (DefReg != UseReg || !MRI.hasOneUse(DefReg))
            break;
          MFI.stackifyVReg(MRI, DefReg);
          ++SubsequentDef;
          ++SubsequentUse;
        }

        // If the instruction we just stackified is an IMPLICIT_DEF, convert it
        // to a constant 0 so that the def is explicit, and the push/pop
        // correspondence is maintained.
        if (Insert->getOpcode() == TargetOpcode::IMPLICIT_DEF)
          convertImplicitDefToConstZero(Insert, MRI, TII, MF, LIS);

        // We stackified an operand. Add the defining instruction's operands to
        // the worklist stack now to continue to build an ever deeper tree.
        Commuting.reset();
        TreeWalker.pushOperands(Insert);
      }

      // If we stackified any operands, skip over the tree to start looking for
      // the next instruction we can build a tree on.
      if (Insert != &*MII) {
        imposeStackOrdering(&*MII);
        MII = MachineBasicBlock::iterator(Insert).getReverse();
        Changed = true;
      }
    }
  }

  // If we used VALUE_STACK anywhere, add it to the live-in sets everywhere so
  // that it never looks like a use-before-def.
  if (Changed) {
    MF.getRegInfo().addLiveIn(EVM::VALUE_STACK);
    for (MachineBasicBlock &MBB : MF)
      MBB.addLiveIn(EVM::VALUE_STACK);
  }

#ifndef NDEBUG
  // Verify that pushes and pops are performed in LIFO order.
  SmallVector<unsigned, 0> Stack;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;
      for (MachineOperand &MO : MI.explicit_uses()) {
        if (!MO.isReg())
          continue;
        Register Reg = MO.getReg();
        if (MFI.isVRegStackified(Reg)) {
          LLVM_DEBUG(dbgs() << "Use stackified reg: " << MO << '\n');
          assert(Stack.back() == Reg &&
                 "Register stack pop should be paired with a push");
          Stack.pop_back();
        }
      }
      for (MachineOperand &MO : reverse(MI.defs())) {
        if (!MO.isReg())
          continue;
        Register Reg = MO.getReg();
        if (MFI.isVRegStackified(Reg)) {
          LLVM_DEBUG(dbgs() << "Stackify reg: " << MO << '\n');
          Stack.push_back(MO.getReg());
        }
      }
    }
    // TODO: Generalize this code to support keeping values on the stack across
    // basic block boundaries.
    assert(Stack.empty() &&
           "Register stack pushes and pops should be balanced");
  }
#endif

  return Changed;
}
