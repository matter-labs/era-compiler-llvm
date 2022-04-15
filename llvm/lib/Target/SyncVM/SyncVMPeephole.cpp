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
      {SyncVM::MULrrrr_s, SyncVM::MULrrsr_s},
      {SyncVM::MULirrr_s, SyncVM::MULirsr_s},
      {SyncVM::MULcrrr_s, SyncVM::MULcrsr_s},
      {SyncVM::MULsrrr_s, SyncVM::MULsrsr_s},
      {SyncVM::DIVrrrr_s, SyncVM::DIVrrsr_s},
      {SyncVM::DIVirrr_s, SyncVM::DIVirsr_s},
      {SyncVM::DIVxrrr_s, SyncVM::DIVxrsr_s},
      {SyncVM::DIVcrrr_s, SyncVM::DIVcrsr_s},
      {SyncVM::DIVyrrr_s, SyncVM::DIVyrsr_s},
      {SyncVM::DIVsrrr_s, SyncVM::DIVsrsr_s},
      {SyncVM::DIVzrrr_s, SyncVM::DIVzrsr_s},
      {SyncVM::MULrrrr_v, SyncVM::MULrrsr_v},
      {SyncVM::MULirrr_v, SyncVM::MULirsr_v},
      {SyncVM::MULcrrr_v, SyncVM::MULcrsr_v},
      {SyncVM::MULsrrr_v, SyncVM::MULsrsr_v},
      {SyncVM::DIVrrrr_v, SyncVM::DIVrrsr_v},
      {SyncVM::DIVirrr_v, SyncVM::DIVirsr_v},
      {SyncVM::DIVxrrr_v, SyncVM::DIVxrsr_v},
      {SyncVM::DIVcrrr_v, SyncVM::DIVcrsr_v},
      {SyncVM::DIVyrrr_v, SyncVM::DIVyrsr_v},
      {SyncVM::DIVsrrr_v, SyncVM::DIVsrsr_v},
      {SyncVM::DIVzrrr_v, SyncVM::DIVzrsr_v}};

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      if (Mapping.count(MI->getOpcode())) {
        auto StoreI = std::next(MI);
        if (StoreI == MBB.end() || StoreI->getOpcode() != SyncVM::ADDrrs_s ||
            StoreI->getOperand(0).getReg() != MI->getOperand(0).getReg() ||
            // CC must be equals
            StoreI->getOperand(StoreI->getNumOperands() - 1).getImm() !=
                MI->getOperand(MI->getNumOperands() - 1).getImm())
          continue;
        DebugLoc DL = MI->getDebugLoc();

        auto NewMI = BuildMI(MBB, MI, DL, TII->get(Mapping[MI->getOpcode()]));
        NewMI.addDef(MI->getOperand(1).getReg());
        for (unsigned i = 2, e = MI->getNumOperands() - 1; i < e; ++i)
          NewMI.add(MI->getOperand(i));
        for (unsigned i = 2; i < 5; ++i)
          NewMI.add(StoreI->getOperand(i));
        NewMI.add(MI->getOperand(MI->getNumOperands() - 1));
        StoreI->eraseFromParent();
        MI->eraseFromParent();
        MI = NewMI;
        Changed = true;
      }
    }
  return Changed;
}

FunctionPass *llvm::createSyncVMPeepholePass() { return new SyncVMPeephole(); }
