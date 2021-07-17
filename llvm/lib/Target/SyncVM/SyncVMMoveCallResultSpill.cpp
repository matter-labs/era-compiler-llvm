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

  for (MachineBasicBlock &MBB : MF) {
    std::vector<MachineBasicBlock::iterator> PushIts;
    MachineBasicBlock::iterator PopIt, CallIt, PushIt;
    PushIt = PopIt = CallIt = MBB.end();
    for (auto It = MBB.begin(), E = MBB.end(); It != E; ++It) {
      switch (It->getOpcode()) {
      default:
        continue;
      case SyncVM::PUSH:
        PushIts.push_back(It);
        continue;
      case SyncVM::CALL:
        CallIt = It;
        continue;
      case SyncVM::POP:
        assert(PopIt == E);
        PopIt = It;
        if (CallIt == E) {
          PopIt = CallIt = PushIt = E;
          PushIts.clear();
          continue;
        }
        unsigned Num = PopIt->getOperand(1).isImm()
                           ? PopIt->getOperand(1).getImm()
                           : PopIt->getOperand(1).getCImm()->getZExtValue();
        for (auto PIIt = std::rbegin(PushIts), PIE = std::rend(PushIts);
             PIIt != PIE; ++PIIt) {
          unsigned NumPush =
              (*PIIt)->getOperand(0).isImm()
                  ? (*PIIt)->getOperand(0).getImm()
                  : (*PIIt)->getOperand(0).getCImm()->getZExtValue();
          if (NumPush != 0) {
            PushIt = PopIt = CallIt = E;
            PushIts.clear();
            break;
          }
          if (Num-- == 0) {
            PushIt = *PIIt;
            break;
          }
        }
        if (CallIt == E)
          continue;
        unsigned Adjust = 1;
        for (auto SplIt = std::next(PushIt); SplIt != CallIt;) {
          if (SplIt->getOpcode() == SyncVM::PUSH) {
            ++Adjust;
            ++SplIt;
            continue;
          }
          if (SplIt->getOpcode() == SyncVM::MOVrs ||
              SplIt->getOpcode() == SyncVM::MOVsr) {
            auto NewIt = std::next(SplIt);
            BuildMI(MBB, SplIt, SplIt->getDebugLoc(),
                    TII->get(SplIt->getOpcode()))
                .add(SplIt->getOperand(0))
                .add(SplIt->getOperand(1))
                .add(SplIt->getOperand(2))
                .addImm(SplIt->getOperand(3).getImm() + Adjust * 32);
            SplIt->eraseFromParent();
            SplIt = NewIt;
            Changed = true;
          } else {
            ++SplIt;
          }
        }
        for (auto SplIt = std::next(CallIt); SplIt != PopIt;) {
          if (SplIt->getOpcode() == SyncVM::MOVrs ||
              SplIt->getOpcode() == SyncVM::MOVsr) {
            auto NewIt = std::next(SplIt);
            BuildMI(MBB, SplIt, SplIt->getDebugLoc(),
                    TII->get(SplIt->getOpcode()))
                .add(SplIt->getOperand(0))
                .add(SplIt->getOperand(1))
                .add(SplIt->getOperand(2))
                .addImm(SplIt->getOperand(3).getImm() - Num * 32);
            SplIt->eraseFromParent();
            SplIt = NewIt;
            Changed = true;
          } else {
            ++SplIt;
          }
        }
        PushIt = PopIt = CallIt = E;
        break;
      }
    }
  }

  for (MachineBasicBlock &MBB : MF) {
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

    std::vector<MachineInstr *> InstrToMove;
    while (II->getOpcode() != SyncVM::CALL &&
           II->getOpcode() != SyncVM::CALLF) {
      InstrToMove.push_back(&*II++);
    }

    auto *FalseMBB = Terminator.getOperand(1).getMBB();
    if (FalseMBB->pred_size() != 1) {
      DebugLoc DL = Terminator.getDebugLoc();
      MachineFunction *F = FalseMBB->getParent();
      auto *NewMBB = F->CreateMachineBasicBlock();
      F->insert(MBB.getIterator(), NewMBB);
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
