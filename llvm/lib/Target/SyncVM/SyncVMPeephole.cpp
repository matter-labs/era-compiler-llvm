//===---------------- SyncVMPeephole.cpp - Peephole optimization ----------===//
//
/// \file
/// Implement peephole optimization pass
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-peephole"
#define SYNCVM_PEEPHOLE "SyncVM peephole optimization"

namespace {

class SyncVMPeephole : public MachineFunctionPass {
public:
  static char ID;
  SyncVMPeephole() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_PEEPHOLE; }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMPeephole::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMPeephole, DEBUG_TYPE, SYNCVM_PEEPHOLE, false, false)

bool SyncVMPeephole::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM Peephole **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  bool Changed = false;
  std::vector<MachineInstr *> ToErase;

  // This is to try to eliminate unhandled expansion of PTR_TO_INT instruction
  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      // eliminate ADDrrr_s if possible
      if (MI->getOpcode() == SyncVM::PTR_ADDrrr_s) {
        if (MI->getOperand(0).getReg() == MI->getOperand(1).getReg() &&
            MI->getOperand(2).getReg() == SyncVM::R0) {
          LLVM_DEBUG(dbgs() << "eliminated ADDrrr_s: "; MI->dump();
                     dbgs() << '\n');
          ToErase.push_back(&*MI);
          Changed = true;
        }
      }
    }
  for (auto MI : ToErase)
    MI->eraseFromParent();
  return Changed;
}

FunctionPass *llvm::createSyncVMPeepholePass() { return new SyncVMPeephole(); }
