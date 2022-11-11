//===------ EraVMMoveCallResultSpill.cpp - Fix for EH control flow -------===//
//
/// \file
/// The file contains a pass that moves call result spill to a child bb which
/// corresponds to normal (non-EH) execution. Thus it restore corrctness of
/// EraVM EH scheme.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-movecallresultspill"
#define ERAVM_MOVE_CALL_RESULT_SPILL "EraVM move call result spill"

namespace {

class EraVMMoveCallResultSpill : public MachineFunctionPass {
public:
  static char ID;
  EraVMMoveCallResultSpill() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return ERAVM_MOVE_CALL_RESULT_SPILL;
  }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char EraVMMoveCallResultSpill::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMMoveCallResultSpill, DEBUG_TYPE,
                ERAVM_MOVE_CALL_RESULT_SPILL, false, false)

bool EraVMMoveCallResultSpill::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM MOVE CALL RESULT SPILL **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  bool Changed = false;

#if 0
  for (auto MBBIt = MF.begin(); MBBIt != MF.end(); ++MBBIt) {
    MachineBasicBlock &MBB = *MBBIt;
    if (MBB.size() < 3)
      continue;

    auto II = MBB.rbegin();
    auto &Terminator = *II++;
    if (Terminator.getOpcode() != EraVM::JCC)
      continue;

    unsigned CC = Terminator.getOperand(2).isImm()
                      ? Terminator.getOperand(2).getImm()
                      : Terminator.getOperand(2).getCImm()->getZExtValue();

    // No exception handling involved
    if (CC != EraVMCC::COND_LT || II->getOpcode() == EraVM::CMPrr)
      continue;

    auto E = std::find_if(II, MBB.rend(), [](MachineInstr &MI) {
      return MI.getOpcode() == EraVM::CALL || MI.getOpcode() == EraVM::CALLF ||
             MI.getOpcode() == EraVM::CALLD || MI.getOpcode() == EraVM::CALLC ||
             MI.getOpcode() == EraVM::CALLS;
    });

    if (E == MBB.rend())
      continue;

    std::vector<MachineInstr *> InstrToMove;
    while (II != E) {
      InstrToMove.push_back(&*II++);
    }

    MachineBasicBlock *FalseMBB = Terminator.getOperand(1).getMBB();
    MachineBasicBlock *TrueMBB = Terminator.getOperand(0).getMBB();
    DebugLoc DL = Terminator.getDebugLoc();

    // If false sucessor has more than 1 predecessors, insert an empty BB in
    // between to move instructions into.
    if (FalseMBB->pred_size() != 1) {
      MachineFunction *F = FalseMBB->getParent();
      auto *NewMBB = F->CreateMachineBasicBlock();
      // The only place we can guarantee that fallthrough doesn't happen to the
      // newly created block.
      F->insert(std::next(MBBIt), NewMBB);
      BuildMI(NewMBB, DL, TII->get(EraVM::JCC))
          .addMBB(FalseMBB)
          .addMBB(FalseMBB)
          .addImm(EraVMCC::COND_NONE);
      NewMBB->addSuccessor(FalseMBB);
      MBB.removeSuccessor(FalseMBB);
      MBB.addSuccessor(NewMBB);
      FalseMBB = NewMBB;
    }

    // If true sucessor has more than 1 predecessors, insert an empty BB in
    // between to move instructions into.
    if (TrueMBB->pred_size() != 1) {
      MachineFunction *F = TrueMBB->getParent();
      auto *NewMBB = F->CreateMachineBasicBlock();
      // The only place we can guarantee that fallthrough doesn't happen to the
      // newly created block.
      F->insert(std::next(MBBIt), NewMBB);
      BuildMI(NewMBB, DL, TII->get(EraVM::JCC))
          .addMBB(TrueMBB)
          .addMBB(TrueMBB)
          .addImm(EraVMCC::COND_NONE);
      NewMBB->addSuccessor(TrueMBB);
      MBB.removeSuccessor(TrueMBB);
      MBB.addSuccessor(NewMBB);
      TrueMBB = NewMBB;
    }

    BuildMI(&MBB, DL, TII->get(EraVM::JCC))
        .addMBB(TrueMBB)
        .addMBB(FalseMBB)
        .add(Terminator.getOperand(2));
    Terminator.eraseFromParent();

    for (auto IIt = InstrToMove.begin(), IE = InstrToMove.end(); IIt != IE;
         ++IIt) {
      (*IIt)->removeFromParent();
      auto NewMI = MF.CloneMachineInstr(*IIt);
      FalseMBB->insert(FalseMBB->begin(), *IIt);
      TrueMBB->insert(TrueMBB->begin(), NewMI);
      Changed = true;
    }
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
#endif
  return Changed;
}

FunctionPass *llvm::createEraVMMoveCallResultSpillPass() {
  return new EraVMMoveCallResultSpill();
}
