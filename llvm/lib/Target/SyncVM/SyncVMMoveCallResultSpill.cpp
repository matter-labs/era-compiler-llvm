//===------ SyncVMMoveCallResultSpill.cpp - Fix for EH control flow -------===//
//
/// \file
/// The file contains a pass that moves call result spill to a child bb which
/// corresponds to normal (non-EH) execution. Thus it restore corrctness of
/// SyncVM EH scheme.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-movecallresultspill"
#define SYNCVM_MOVE_CALL_RESULT_SPILL "SyncVM move call result spill"

namespace {

class SyncVMMoveCallResultSpill : public MachineFunctionPass {
public:
  static char ID;
  SyncVMMoveCallResultSpill() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_MOVE_CALL_RESULT_SPILL;
  }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMMoveCallResultSpill::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMMoveCallResultSpill, DEBUG_TYPE,
                SYNCVM_MOVE_CALL_RESULT_SPILL, false, false)

bool SyncVMMoveCallResultSpill::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM MOVE CALL RESULT SPILL **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  bool Changed = false;

  for (auto MBBIt = MF.begin(); MBBIt != MF.end(); ++MBBIt) {
    MachineBasicBlock &MBB = *MBBIt;
    if (MBB.size() < 3)
      continue;

    auto II = MBB.rbegin();
    auto &Terminator = *II++;
    if (Terminator.getOpcode() != SyncVM::JCC)
      continue;

    unsigned CC = Terminator.getOperand(2).isImm()
                      ? Terminator.getOperand(2).getImm()
                      : Terminator.getOperand(2).getCImm()->getZExtValue();

    // No exception handling involved
    if (CC != SyncVMCC::COND_LT || II->getOpcode() == SyncVM::CMPrr)
      continue;

    auto E = std::find_if(II, MBB.rend(), [](MachineInstr &MI) {
      return MI.getOpcode() == SyncVM::CALL || MI.getOpcode() == SyncVM::CALLF;
    });

    if (E == MBB.rend())
      continue;

    std::vector<MachineInstr *> InstrToMove;
    while (II != E) {
      InstrToMove.push_back(&*II++);
    }

    auto *FalseMBB = Terminator.getOperand(1).getMBB();
    if (FalseMBB->pred_size() != 1) {
      DebugLoc DL = Terminator.getDebugLoc();
      MachineFunction *F = FalseMBB->getParent();
      auto *NewMBB = F->CreateMachineBasicBlock();
      // The only place we can guarantee that fallthrough doesn't happen to the
      // newly created block.
      F->insert(std::next(MBBIt), NewMBB);
      BuildMI(NewMBB, DL, TII->get(SyncVM::JCC))
          .addMBB(FalseMBB)
          .addMBB(FalseMBB)
          .addImm(SyncVMCC::COND_NONE);
      NewMBB->addSuccessor(FalseMBB);
      BuildMI(&MBB, DL, TII->get(SyncVM::JCC))
          .add(Terminator.getOperand(0))
          .addMBB(NewMBB)
          .add(Terminator.getOperand(2));
      Terminator.eraseFromParent();
      MBB.removeSuccessor(FalseMBB);
      MBB.addSuccessor(NewMBB);
      FalseMBB = NewMBB;
    }

    for (auto IIt = InstrToMove.begin(), IE = InstrToMove.end(); IIt != IE;
         ++IIt) {
      (*IIt)->removeFromParent();
      FalseMBB->insert(FalseMBB->begin(), *IIt);
      Changed = true;
    }
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

FunctionPass *llvm::createSyncVMMoveCallResultSpillPass() {
  return new SyncVMMoveCallResultSpill();
}
