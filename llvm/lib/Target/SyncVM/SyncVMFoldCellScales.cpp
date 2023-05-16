//===--- SyncVMFoldCellScales.cpp - Combine memops to indexed ----===//
//
/// \file
/// This file contains load and store combination pass to emit indexed memory
/// operations. It relies on def-use chains and runs on pre-RA phase.
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

#define DEBUG_TYPE "syncvm-fold-cell-scales"
#define SYNCVM_FOLD_CELL_SCALES_NAME                                     \
  "SyncVM fold scales of cell memory operations"

STATISTIC(NumInstrsFolded, "Number of scaling instructions folded");

namespace {

class SyncVMFoldCellScales : public MachineFunctionPass {
public:
  static char ID;
  SyncVMFoldCellScales() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_FOLD_CELL_SCALES_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:

  /// Erase all instructions in \p ToErase and replaces registers they define
  /// with \p R.
  void eraseAndReplaceUses(SmallVectorImpl<MachineInstr *> &ToErase,
                           Register R);

  bool FoldCellScales(MachineInstr& MI);
  ///
  bool combineCellScalingInstrs(MachineInstr& MI);

  const MachineDominatorTree *MDT;
  const SyncVMInstrInfo *TII;
  MachineRegisterInfo *RegInfo;
};

char SyncVMFoldCellScales::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(SyncVMFoldCellScales, DEBUG_TYPE,
                      SYNCVM_FOLD_CELL_SCALES_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(SyncVMFoldCellScales, DEBUG_TYPE,
                    SYNCVM_FOLD_CELL_SCALES_NAME, false, false)

void SyncVMFoldCellScales::eraseAndReplaceUses(
    SmallVectorImpl<MachineInstr *> &ToErase, Register R) {
  for (MachineInstr *Inst : ToErase) {
    auto oldOffset = Inst->getOperand(0);
    assert(oldOffset.isDef() && "expecting a def operand");
    auto oldOffsetReg = oldOffset.getReg();
    RegInfo->replaceRegWith(oldOffsetReg, R);
    LLVM_DEBUG(dbgs() << " .  Removing instruction: "; Inst->dump());
    Inst->eraseFromParent();
    ++NumInstrsFolded;
  }
}

bool SyncVMFoldCellScales::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM COMBINE LOAD and STORE **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  RegInfo = &MF.getRegInfo();
  assert(RegInfo->isSSA());
  assert(RegInfo->tracksLiveness());

  MDT = &getAnalysis<MachineDominatorTree>();
  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  bool Changed = false;

  for (auto &MBB : MF) {
    MachineBasicBlock::iterator MII = MBB.begin();
    MachineBasicBlock::iterator MIE = MBB.end();
    while (MII != MIE) {
      auto &MI = *MII;
      ++MII;

      // tries to combine shl and div if they are both for scaling cell addresses
      Changed |= combineCellScalingInstrs(MI);
    }
  }

  return Changed;
}

bool SyncVMFoldCellScales::combineCellScalingInstrs(MachineInstr& MI) {
  // the incoming MI should be: `shl.s 5, rX, rY`
  if (!(MI.getOpcode() == SyncVM::SHLxrr_s &&
      getImmOrCImm(MI.getOperand(1)) == 5)) {
    return false;
  }
  Register SourceReg = MI.getOperand(2).getReg();

  auto Div32Uses =
      SmallVector<MachineInstr *, 4>{map_range(
          make_filter_range(RegInfo->use_instructions(SourceReg),
                            [&MI, this](MachineInstr &CurrentMI) {
                              return (
                                  CurrentMI.getOpcode() == SyncVM::DIVxrrr_s &&
                                  getImmOrCImm(CurrentMI.getOperand(2)) == 32 &&
                                  MDT->dominates(&MI, &CurrentMI));
                            }),
          [](MachineInstr &MI) { return &MI; })};

  eraseAndReplaceUses(Div32Uses, SourceReg);
  
  // also remove the shl.s instruction if rY is not used anymore
  Register DestReg = MI.getOperand(0).getReg();
  if (RegInfo->use_empty(DestReg)) {
    MI.eraseFromParent();
    ++NumInstrsFolded;
    return true;
  }

  return !Div32Uses.empty();
}

/// createSyncVMFoldCellScalesPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createSyncVMFoldCellScalesPass() {
  return new SyncVMFoldCellScales();
}
