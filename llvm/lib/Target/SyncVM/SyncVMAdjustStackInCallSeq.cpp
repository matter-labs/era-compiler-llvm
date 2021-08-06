//===------ SyncVMAdjustStackInCallseq.cpp - Update SP-based address ------===//
//
/// \file
/// The file contains a pass that updated displacement for all stack addresses
/// in between push and pop instructions.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-adjust-stack-in-callseq"
#define SYNCVM_ADJUST_STACK_IN_CALLSEQ "SyncVM adjust stack in call sequence"

namespace {

class SyncVMAdjustStackInCallseq : public MachineFunctionPass {
public:
  static char ID;
  SyncVMAdjustStackInCallseq() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_ADJUST_STACK_IN_CALLSEQ;
  }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMAdjustStackInCallseq::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMAdjustStackInCallseq, DEBUG_TYPE,
                SYNCVM_ADJUST_STACK_IN_CALLSEQ, false, false)

bool SyncVMAdjustStackInCallseq::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM Adjust SP-based addresses in call "
                       "sequnce **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  if (MF.empty())
    return false;

  bool Changed = false;

  struct BBToProcess {
    MachineBasicBlock *BB;
    unsigned Disp;
  };

  DenseMap<MachineBasicBlock *, unsigned> Processed;
  std::deque<BBToProcess> WorkList;

  WorkList.push_back({&MF.front(), 0});
  while (!WorkList.empty()) {
    BBToProcess Current = WorkList.front();
    MachineBasicBlock *BB = Current.BB;
    unsigned Disp = Current.Disp;
    if (Processed.count(BB) != 0) {
      assert(Processed[BB] == Disp && "Stack corrupted");
      WorkList.pop_front();
      continue;
    }
    for (auto I = BB->begin(), E = BB->end(); I != E; ++I) {
      if (I->getOpcode() == SyncVM::PUSH &&
          !(&*I == &BB->front() && BB == &MF.front())) {
        unsigned Num = I->getOperand(0).isImm()
                           ? I->getOperand(0).getImm()
                           : I->getOperand(0).getCImm()->getZExtValue();
        if (!Num)
          ++Disp;
      } else if (I->getOpcode() == SyncVM::POP) {
        unsigned Num = I->getOperand(1).isImm()
                           ? I->getOperand(1).getImm()
                           : I->getOperand(1).getCImm()->getZExtValue();
        // Overflows when reachin final pop, but it should not impact
        // the processing because no stack manipulations are expected after.
        Disp -= Num + 1;
      } else if (Disp && (I->getOpcode() == SyncVM::MOVrs ||
                          I->getOpcode() == SyncVM::MOVsr)) {
        auto Erace = I;
        I = BuildMI(*BB, I, I->getDebugLoc(), TII->get(I->getOpcode()))
                .add(I->getOperand(0))
                .add(I->getOperand(1))
                .add(I->getOperand(2))
                .addImm(I->getOperand(3).getImm() + Disp * 32);
        Erace->eraseFromParent();
        Changed = true;
      }
    }
    for (auto S : BB->successors())
      WorkList.push_back({S, Disp});
    WorkList.pop_front();
    Processed[BB] = Current.Disp;
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

FunctionPass *llvm::createSyncVMAdjustStackInCallseqPass() {
  return new SyncVMAdjustStackInCallseq();
}
