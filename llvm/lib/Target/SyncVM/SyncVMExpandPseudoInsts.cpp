//===-- SyncVMExpandPseudoInsts.cpp - Expand pseudo instructions ----------===//
//
/// \file
/// This file contains a pass that expands pseudo instructions into target
/// instructions. This pass should be run after register allocation but before
/// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-pseudo"
#define SYNCVM_EXPAND_PSEUDO_NAME "SyncVM expand pseudo instructions"

namespace {

class SyncVMExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  SyncVMExpandPseudo() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_EXPAND_PSEUDO_NAME; }

private:
  void expandConst(MachineInstr &MI) const;
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMExpandPseudo::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMExpandPseudo, DEBUG_TYPE, SYNCVM_EXPAND_PSEUDO_NAME,
                false, false)

void SyncVMExpandPseudo::expandConst(MachineInstr &MI) const {
  MachineOperand Constant = MI.getOperand(1);
  MachineOperand Reg = MI.getOperand(0);
  assert((Constant.isImm() || Constant.isCImm()) && "Unexpected operand type");
  const APInt &Val = Constant.isCImm() ? Constant.getCImm()->getValue()
                                       : APInt(256, Constant.getImm(), true);
  APInt ValLo = Val.shl(128).lshr(128);
  APInt ValHi = Val.lshr(128);
  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(SyncVM::SFLLir))
      .add(Reg)
      .addCImm(ConstantInt::get(*Context, ValLo))
      .add(Reg);
  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(SyncVM::SFLHir))
      .add(Reg)
      .addCImm(ConstantInt::get(*Context, ValHi))
      .add(Reg);
}

bool SyncVMExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> Pseudos;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (MI.isPseudo()) {
        switch (MI.getOpcode()) {
        default:
          llvm_unreachable("Unknown pseudo");
        case SyncVM::CONST:
          expandConst(MI);
          break;
        }
        Pseudos.push_back(&MI);
      }
    }

  for (auto *I : Pseudos)
    I->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !Pseudos.empty();
}

/// createSyncVMExpandPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createSyncVMExpandPseudoPass() {
  return new SyncVMExpandPseudo();
}
