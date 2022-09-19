//===--- SyncVMPropagateGenericPointers.cpp - Use ptr.inst for pointers ---===//
//
/// \file
/// The file contains a pass that restore correctness of work with generic
/// (addrspace 3) pointers.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineDominanceFrontier.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-propagate-generic-pointers"
#define SYNCVM_PROPAGATE_GENERIC_POINTERS "SyncVM propagate generic pointers"

namespace {

class SyncVMPropagateGenericPointers : public MachineFunctionPass {
public:
  static char ID;
  SyncVMPropagateGenericPointers() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_PROPAGATE_GENERIC_POINTERS;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<ReachingDefAnalysis>();
    AU.setPreservesAll();
  }

private:
  const SyncVMInstrInfo *TII;
  const ReachingDefAnalysis *RDA;

  bool canTransform(MachineInstr &MI);
};

char SyncVMPropagateGenericPointers::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(SyncVMPropagateGenericPointers, DEBUG_TYPE,
                      SYNCVM_PROPAGATE_GENERIC_POINTERS, false, false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_END(SyncVMPropagateGenericPointers, DEBUG_TYPE,
                    SYNCVM_PROPAGATE_GENERIC_POINTERS, false, false)

bool SyncVMPropagateGenericPointers::canTransform(MachineInstr &MI) {
  assert(TII && RDA);
  // For now we only expect mov instruction to be in a wrong form
  if (!TII->isAdd(MI) || !TII->hasRROperandAddressingMode(MI))
    return false;
  if (MI.getNumDefs() != 1 || MI.getOperand(2).getReg() != SyncVM::R0)
    return false;
  SmallPtrSet<MachineInstr *, 4> ReachingDefs{};
  RDA->getGlobalReachingDefs(&MI, MI.getOperand(1).getReg(), ReachingDefs);
  // If the register is not defined iside the function, it must be its argument
  // or 0.
  if (ReachingDefs.empty()) {
    Register Arg = MI.getOperand(1).getReg();
    if (Arg == SyncVM::R0)
      return false;
    // FIXME: At the moment FE doesn't pass aggregates by value, so the number
    // of the register must be equal to its position in the argument list.
    unsigned ArgNo = Arg - SyncVM::R0 - 1;
    auto *FTy = MI.getParent()->getParent()->getFunction().getFunctionType();
    assert(FTy->getFunctionNumParams() > ArgNo);
    auto *ArgTy = dyn_cast<PointerType>(FTy->getParamType(ArgNo));
    if (ArgTy && ArgTy->getAddressSpace() == SyncVMAS::AS_GENERIC)
      return true;
    return false;
  }
  return llvm::all_of(ReachingDefs, [this](const MachineInstr *MI) {
    return TII->isPtr(*MI) || TII->isNull(*MI);
  });
}

bool SyncVMPropagateGenericPointers::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM Propagate Generic Pointers **********\n"
             << "********** Function: " << MF.getName() << '\n');
  bool Changed = false;

  auto &SyncVMST = MF.getSubtarget<SyncVMSubtarget>();
  TII = SyncVMST.getInstrInfo();
  RDA = &getAnalysis<ReachingDefAnalysis>();

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (canTransform(MI)) {
        MI.setDesc(TII->get(SyncVM::PTR_ADDrrr_s));
        Changed = true;
      }

  return Changed;
}

FunctionPass *llvm::createSyncVMPropagateGenericPointersPass() {
  return new SyncVMPropagateGenericPointers();
}
