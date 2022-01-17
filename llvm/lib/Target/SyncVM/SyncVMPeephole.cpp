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

  DenseMap<unsigned, unsigned> Mapping = {
      {SyncVM::MULrrrr, SyncVM::MULrrsr}, {SyncVM::MULirrr, SyncVM::MULirsr},
      {SyncVM::MULcrrr, SyncVM::MULcrsr}, {SyncVM::MULsrrr, SyncVM::MULsrsr},
      {SyncVM::DIVrrrr, SyncVM::DIVrrsr}, {SyncVM::DIVirrr, SyncVM::DIVirsr},
      {SyncVM::DIVxrrr, SyncVM::DIVxrsr}, {SyncVM::DIVcrrr, SyncVM::DIVcrsr},
      {SyncVM::DIVyrrr, SyncVM::DIVyrsr}, {SyncVM::DIVsrrr, SyncVM::DIVsrsr},
      {SyncVM::DIVzrrr, SyncVM::DIVzrsr}};

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      if (Mapping.count(MI->getOpcode())) {
        auto StoreI = std::next(MI);
        if (StoreI->getOpcode() != SyncVM::ADDirs ||
            StoreI->getOperand(0).getReg() != MI->getOperand(0).getReg())
          continue;
        DebugLoc DL = MI->getDebugLoc();

        auto NewMI = BuildMI(MBB, MI, DL, TII->get(Mapping[MI->getOpcode()]));
        for (unsigned i = 1, e = MI->getNumOperands(); i < e; ++i)
          NewMI.add(MI->getOperand(i));
        for (unsigned i = 2; i < 5; ++i)
          NewMI.add(StoreI->getOperand(i));
        StoreI->eraseFromParent();
        MI->eraseFromParent();
        MI = NewMI;
        Changed = true;
      }
    }
  return Changed;
}

FunctionPass *llvm::createSyncVMPeepholePass() { return new SyncVMPeephole(); }
