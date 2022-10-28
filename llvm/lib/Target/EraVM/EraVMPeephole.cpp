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
  std::vector<MachineInstr *> ToErase;

  // This is to try to eliminate unhandled expansion of PTR_TO_INT instruction
  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      // eliminate ADDrrr_s if possible
      if (MI->getOpcode() == EraVM::PTR_ADDrrr_s) {
        if (MI->getOperand(0).getReg() == MI->getOperand(1).getReg() &&
            MI->getOperand(2).getReg() == EraVM::R0) {
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

FunctionPass *llvm::createEraVMPeepholePass() { return new EraVMPeephole(); }
