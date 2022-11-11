//===---------------- EraVMPeephole.cpp - Peephole optimization ----------===//
//
/// \file
/// Implement peephole optimization pass
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-peephole"
#define ERAVM_PEEPHOLE "EraVM peephole optimization"

namespace {

class EraVMPeephole : public MachineFunctionPass {
public:
  static char ID;
  EraVMPeephole() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return ERAVM_PEEPHOLE; }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char EraVMPeephole::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMPeephole, DEBUG_TYPE, ERAVM_PEEPHOLE, false, false)

bool EraVMPeephole::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM Peephole **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  bool Changed = false;

  DenseMap<unsigned, unsigned> Mapping = {{EraVM::MULrrrr_s, EraVM::MULrrsr_s},
                                          {EraVM::MULirrr_s, EraVM::MULirsr_s},
                                          {EraVM::MULcrrr_s, EraVM::MULcrsr_s},
                                          {EraVM::MULsrrr_s, EraVM::MULsrsr_s},
                                          {EraVM::DIVrrrr_s, EraVM::DIVrrsr_s},
                                          {EraVM::DIVirrr_s, EraVM::DIVirsr_s},
                                          {EraVM::DIVxrrr_s, EraVM::DIVxrsr_s},
                                          {EraVM::DIVcrrr_s, EraVM::DIVcrsr_s},
                                          {EraVM::DIVyrrr_s, EraVM::DIVyrsr_s},
                                          {EraVM::DIVsrrr_s, EraVM::DIVsrsr_s},
                                          {EraVM::DIVzrrr_s, EraVM::DIVzrsr_s},
                                          {EraVM::MULrrrr_v, EraVM::MULrrsr_v},
                                          {EraVM::MULirrr_v, EraVM::MULirsr_v},
                                          {EraVM::MULcrrr_v, EraVM::MULcrsr_v},
                                          {EraVM::MULsrrr_v, EraVM::MULsrsr_v},
                                          {EraVM::DIVrrrr_v, EraVM::DIVrrsr_v},
                                          {EraVM::DIVirrr_v, EraVM::DIVirsr_v},
                                          {EraVM::DIVxrrr_v, EraVM::DIVxrsr_v},
                                          {EraVM::DIVcrrr_v, EraVM::DIVcrsr_v},
                                          {EraVM::DIVyrrr_v, EraVM::DIVyrsr_v},
                                          {EraVM::DIVsrrr_v, EraVM::DIVsrsr_v},
                                          {EraVM::DIVzrrr_v, EraVM::DIVzrsr_v}};

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      if (Mapping.count(MI->getOpcode())) {
        auto StoreI = std::next(MI);
        if (StoreI == MBB.end()) {
          continue;
        }
        if (StoreI->getOpcode() != EraVM::ADDirs_s ||
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

FunctionPass *llvm::createEraVMPeepholePass() { return new EraVMPeephole(); }
