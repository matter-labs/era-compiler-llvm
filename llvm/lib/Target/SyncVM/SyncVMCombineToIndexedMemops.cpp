//===--- SyncVMCombineToIndexedMemops.cpp - Combine memops to indexed ----===//
//
/// \file
/// This file contains load and store combination pass to emit indexed memory
/// operations. It relies on def-use chains and runs on pre-RA phase.
///
///
/// Generally speaking, this pass does the following:
/// * iterate a function from top to bottom, combine this pattern:
///     {ld|st}.{1|2} %ra, %rv
///     add 32, %ra, %rb
///   into:
///     {ld|st}.{1|2}.inc %ra, %rv, %rb
///
///   where %rb is automatically incremented by the new `.inc` instruction.
///
/// Note that this pass is not able to handle similar patterns within loops
/// because %rb will carry loop dependence.
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "SyncVMSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-generate-indexed-memops"
#define SYNCVM_COMBINE_INDEXED_MEMOPS_NAME                                     \
  "SyncVM combine instructions to generate indexed memory operations"

static cl::opt<bool> EnableSyncVMIndexedMemOpCombining(
    "enable-syncvm-combine-to-indexed-memory-ops", cl::init(true), cl::Hidden);

STATISTIC(NumIndexedMemOpCombined, "Number of indexed mem operations emitted");
STATISTIC(NumAdd32Removed, "Number of add 32, x instructions removed");

namespace {

class SyncVMCombineToIndexedMemops : public MachineFunctionPass {
public:
  static char ID;
  SyncVMCombineToIndexedMemops() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_COMBINE_INDEXED_MEMOPS_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  // map <non-inc opcode> -> <inc opcode>
  const DenseMap<unsigned, unsigned> PostIncOpcMap = {
      {SyncVM::LD,  SyncVM::LDInc},
      {SyncVM::LD1, SyncVM::LD1Inc},
      {SyncVM::LD2, SyncVM::LD2Inc},
      {SyncVM::ST1, SyncVM::ST1Inc},
      {SyncVM::ST2, SyncVM::ST2Inc},
  };

  llvm::Register getLoadStoreOffsetRegister(MachineInstr *MI) const {
    if (MI->getOpcode() == SyncVM::LD1 || MI->getOpcode() == SyncVM::LD2 ||
        MI->getOpcode() == SyncVM::LD) {
      return MI->getOperand(1).getReg();
    }
    if (MI->getOpcode() == SyncVM::ST1 || MI->getOpcode() == SyncVM::ST2) {
      return MI->getOperand(0).getReg();
    }
    llvm_unreachable("unexpected opcode");
  }

  /// Erase all instructions in \p ToErase and replaces registers they define
  /// with \p R.
  void eraseAndReplaceUses(SmallVectorImpl<MachineInstr *> &ToErase,
                           Register R);
  /// Replace load or store instruction with its indexed counterpart.
  /// \par NewOffset the virtual register where increment offset is to be put.
  /// \return Pointer to newly created indexed memory operation.
  /// \precondition MI's opcode must be among PostIncOpcMap keys.
  MachineInstr *replaceWithIndexed(MachineInstr &MI, Register NewOffset);

  const MachineDominatorTree *MDT;
  const SyncVMInstrInfo *TII;
  MachineRegisterInfo *RegInfo;
};

char SyncVMCombineToIndexedMemops::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(SyncVMCombineToIndexedMemops, DEBUG_TYPE,
                      SYNCVM_COMBINE_INDEXED_MEMOPS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(SyncVMCombineToIndexedMemops, DEBUG_TYPE,
                    SYNCVM_COMBINE_INDEXED_MEMOPS_NAME, false, false)

void SyncVMCombineToIndexedMemops::eraseAndReplaceUses(
    SmallVectorImpl<MachineInstr *> &ToErase, Register R) {
  for (MachineInstr *Inst : ToErase) {
    auto oldOffset = Inst->getOperand(0);
    assert(oldOffset.isDef() && "expecting a def operand");
    auto oldOffsetReg = oldOffset.getReg();
    RegInfo->replaceRegWith(oldOffsetReg, R);
    LLVM_DEBUG(dbgs() << " .  Removing instruction: "; Inst->dump());
    Inst->eraseFromParent();
    ++NumAdd32Removed;
  }
}

MachineInstr *
SyncVMCombineToIndexedMemops::replaceWithIndexed(MachineInstr &MI,
                                                 Register NewOffset) {
  assert(PostIncOpcMap.count(MI.getOpcode()) && "MI opcode must be in the map");
  MachineBasicBlock &MBB = *MI.getParent();
  auto IncOpcode = PostIncOpcMap.lookup(MI.getOpcode());
  MachineInstr *Result;
  if (MI.mayLoad())
    Result = BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(IncOpcode))
                 .addDef(MI.getOperand(0).getReg())
                 .addDef(NewOffset)
                 .addReg(MI.getOperand(1).getReg()) /*offset*/
                 .addImm(SyncVMCC::COND_NONE)
                 .getInstr();
  else
    Result = BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(IncOpcode))
                 .addDef(NewOffset)
                 .addReg(MI.getOperand(0).getReg()) /*offset*/
                 .addReg(MI.getOperand(1).getReg()) /*value*/
                 .addImm(SyncVMCC::COND_NONE)
                 .getInstr();
  LLVM_DEBUG(dbgs() << "== Replaced instruction:"; MI.dump();
             dbgs() << "       With instruction:"; Result->dump(););
  MI.eraseFromParent();
  return Result;
}

bool SyncVMCombineToIndexedMemops::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM COMBINE LOAD and STORE **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  if (!EnableSyncVMIndexedMemOpCombining) {
    return false;
  }

  RegInfo = &MF.getRegInfo();
  assert(RegInfo->isSSA());
  assert(RegInfo->tracksLiveness());

  MDT = &getAnalysis<MachineDominatorTree>();
  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  bool Changed = false;

  // The main loop. Iterate over instructions, try to find a load/store
  // instruction that has a use of the offset value, and the use is an (add 32,
  // offset) instruction
  for (auto &MBB : MF) {
    MachineBasicBlock::iterator MII = MBB.begin();
    MachineBasicBlock::iterator MIE = MBB.end();
    while (MII != MIE) {
      auto &MI = *MII;
      ++MII;

      // skip non-load/store instructions
      if (PostIncOpcMap.count(MI.getOpcode()) == 0) {
        continue;
      }

      auto OffsetReg = getLoadStoreOffsetRegister(&MI);
      // Cannot handle the case of multiple definition of a register
      if (!RegInfo->hasOneDef(OffsetReg)) {
        continue;
      }

      assert(TII->getCCCode(MI) == SyncVMCC::COND_NONE &&
             "expect unpredicated load or store");

      // Find all instructions `offset' = add 32, offset` that are dominated by
      // MI. These instructions are to be removed, so put pointers to them
      // instead of iterators.
      // clang-format off
      auto Add32ToRemove = SmallVector<MachineInstr *, 4> {
        map_range(
          make_filter_range(RegInfo->use_instructions(OffsetReg),
                            [&MI, this](MachineInstr &CurrentMI) {
                              bool checkFatPtr = MI.getOpcode() == SyncVM::LD;
                              unsigned addOpcode = checkFatPtr ?
                                                   SyncVM::PTR_ADDxrr_s :
                                                   SyncVM::ADDirr_s;
                              return (
                                  CurrentMI.getOpcode() == addOpcode &&
                                  getImmOrCImm(CurrentMI.getOperand(1)) == 32 &&
                                  MDT->dominates(&MI, &CurrentMI));
                            }),
          [](MachineInstr &MI) { return &MI; }
        )
      };
      // clang-format on

      // It's not profitable to use an indexed memory operation if no add is to
      // be removed.
      if (Add32ToRemove.empty()) {
        continue;
      }

      // Convert memory operation to indexed form and remove all dominated
      // add 32, x, y.
      ++NumIndexedMemOpCombined;
      Changed = true;
      Register IncrementedOffsetReg =
          RegInfo->createVirtualRegister(&SyncVM::GR256RegClass);
      MachineInstr *MemIncInst = replaceWithIndexed(MI, IncrementedOffsetReg);
      eraseAndReplaceUses(Add32ToRemove, IncrementedOffsetReg);

      MII = std::next(MemIncInst->getIterator());
    }
  }

  return Changed;
}

/// createSyncVMCombineToIndexedMemopsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createSyncVMCombineToIndexedMemopsPass() {
  return new SyncVMCombineToIndexedMemops();
}
