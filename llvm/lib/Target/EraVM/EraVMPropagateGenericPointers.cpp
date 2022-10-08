//===--- EraVMPropagateGenericPointers.cpp - Use ptr.inst for pointers ---===//
//
/// \file
/// The file contains a pass that restore correctness of work with generic
/// (addrspace 3) pointers.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineDominanceFrontier.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-propagate-generic-pointers"
#define ERAVM_PROPAGATE_GENERIC_POINTERS "EraVM propagate generic pointers"

namespace {

class EraVMPropagateGenericPointers : public MachineFunctionPass {
public:
  static char ID;
  EraVMPropagateGenericPointers() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return ERAVM_PROPAGATE_GENERIC_POINTERS;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<ReachingDefAnalysis>();
    AU.setPreservesAll();
  }

private:
  const EraVMInstrInfo *TII;
  const ReachingDefAnalysis *RDA;

  bool canTransform(MachineInstr &MI);
};

char EraVMPropagateGenericPointers::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(EraVMPropagateGenericPointers, DEBUG_TYPE,
                      ERAVM_PROPAGATE_GENERIC_POINTERS, false, false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_END(EraVMPropagateGenericPointers, DEBUG_TYPE,
                    ERAVM_PROPAGATE_GENERIC_POINTERS, false, false)

bool EraVMPropagateGenericPointers::canTransform(MachineInstr &MI) {
  assert(TII && RDA);
  // For now we only expect mov instruction to be in a wrong form
  if (!TII->isAdd(MI) || !TII->hasRROperandAddressingMode(MI))
    return false;
  if (MI.getNumDefs() != 1 || MI.getOperand(2).getReg() != EraVM::R0)
    return false;
  SmallPtrSet<MachineInstr *, 4> ReachingDefs{};
  RDA->getGlobalReachingDefs(&MI, MI.getOperand(1).getReg(), ReachingDefs);

  if (llvm::all_of(ReachingDefs,
                   [this](const MachineInstr *MI) { return TII->isNull(*MI); }))
    return false;
  return !ReachingDefs.empty() &&
         llvm::all_of(ReachingDefs, [this](const MachineInstr *MI) {
           return TII->isPtr(*MI) || TII->isNull(*MI);
         });
}

bool EraVMPropagateGenericPointers::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM Propagate Generic Pointers **********\n"
             << "********** Function: " << MF.getName() << '\n');
  bool Changed = false;

  auto &EraVMST = MF.getSubtarget<EraVMSubtarget>();
  TII = EraVMST.getInstrInfo();
  RDA = &getAnalysis<ReachingDefAnalysis>();

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (canTransform(MI)) {
        MI.setDesc(TII->get(EraVM::PTR_ADDrrr_s));
        Changed = true;
      }

  return Changed;
}

FunctionPass *llvm::createEraVMPropagateGenericPointersPass() {
  return new EraVMPropagateGenericPointers();
}
