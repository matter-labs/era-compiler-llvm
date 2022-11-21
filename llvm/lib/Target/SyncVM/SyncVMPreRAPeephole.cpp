//===------ SyncVMPreRAPeephole.cpp - Peephole optimization ---------------===//
//
/// \file
/// Implement Pre-RA peephole optimization pass
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-prera-peephole"
#define SYNCVM_PRERA_PEEPHOLE "SyncVM pre-RA peephole optimizations"

namespace {

class SyncVMPreRAPeephole : public MachineFunctionPass {
public:
  static char ID;
  SyncVMPreRAPeephole() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  bool combineStackToStackMoves(MachineFunction &);
  bool combineStoreToStack(MachineFunction &);

  StringRef getPassName() const override { return SYNCVM_PRERA_PEEPHOLE; }

private:
  const TargetInstrInfo *TII;
  MachineRegisterInfo *MRI;
  LLVMContext *Context;
};

char SyncVMPreRAPeephole::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMPreRAPeephole, DEBUG_TYPE, SYNCVM_PRERA_PEEPHOLE, false,
                false)

// Combine mem/stack to stack moves. For example:
//
// add @val[0], r0, %reg
// add %reg, r0, stack-[1]
//
// can be combined to:
//
// add @val[0], r0, stack-[1]
//
// with condition that %reg has only one use, which is the 2nd add instruction.
//
// We are doing it post-ISEL because tablegen-based ISEL is not able to emit
// stack-reg-stack addressing instruction. The reason is unknown yet.
bool SyncVMPreRAPeephole::combineStackToStackMoves(MachineFunction &MF) {
  // Checks if CC is unconditional
  auto isUnconditional = [](const MachineInstr &MI) {
    return getImmOrCImm(MI.getOperand(MI.getNumOperands() - 1)) ==
           SyncVMCC::COND_NONE;
  };
  auto isMoveRegToStack = [&](MachineInstr &MI) {
    return (MI.getOpcode() == SyncVM::ADDrrs_s ||
            MI.getOpcode() == SyncVM::PTR_ADDrrs_s) &&
           MI.getOperand(1).getReg() == SyncVM::R0 && isUnconditional(MI);
  };
  auto isMoveStackToReg = [&](MachineInstr &MI) {
    return (MI.getOpcode() == SyncVM::ADDsrr_s ||
            MI.getOpcode() == SyncVM::PTR_ADDsrr_s) &&
           MI.getOperand(4).getReg() == SyncVM::R0 && isUnconditional(MI);
  };
  auto isMoveCodeToReg = [&](MachineInstr &MI) {
    return MI.getOpcode() == SyncVM::ADDcrr_s &&
           MI.getOperand(3).getReg() == SyncVM::R0 && isUnconditional(MI);
  };

  std::vector<MachineInstr *> ToRemove;

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      bool isMoveCode = isMoveCodeToReg(*MI);
      bool isMoveStack = isMoveStackToReg(*MI);
      if (isMoveCode || isMoveStack) {
        LLVM_DEBUG(dbgs() << " . Found Move to Reg instruction: "; MI->dump());
        auto reg = MI->getOperand(0).getReg();
        // TODO: CPR-895 relax this condition
        // can only have one use
        if (!MRI->hasOneNonDBGUse(reg)) {
          continue;
        }
        auto UseMI = MRI->use_nodbg_instructions(reg).begin();
        if (std::next(MI) != &*UseMI) {
          // if we combine non-adjacent instructions, we could hit bugs.
          // Consider this example:
          /*
           add     stack-[9], r0, r1
           add     stack-[11], r0, r2
           add     r2, r0, stack-[9]
           add     r1, r0, stack-[11]
          */
          // if we combine the 1st and the 3rd, as well as the 2nd and 4th
          // instruction, we will emit incorrect code.
          LLVM_DEBUG(
              dbgs()
              << " . Use is not the exact following instruction. Must bail.\n");
          continue;
        }
        if (!isMoveRegToStack(*UseMI)) {
          continue;
        }
        LLVM_DEBUG(
            dbgs() << "   Found its use is a Move to Stack instruction: ";
            UseMI->dump());
        // now we can combine the two
        int opcode = isMoveCode ? SyncVM::ADDcrs_s : SyncVM::ADDsrs_s;
        auto NewMI =
            BuildMI(MBB, *UseMI, UseMI->getDebugLoc(), TII->get(opcode));
        if (isMoveCode) {
          NewMI.add(MI->getOperand(1));
          NewMI.add(MI->getOperand(2));
        } else {
          NewMI.add(MI->getOperand(1));
          NewMI.add(MI->getOperand(2));
          NewMI.add(MI->getOperand(3));
        }

        for (unsigned index = 1; index < UseMI->getNumOperands(); ++index) {
          NewMI.add(UseMI->getOperand(index));
        }
        LLVM_DEBUG(dbgs() << "   Combined to: "; NewMI->dump());
        ToRemove.push_back(&*UseMI);
        ToRemove.push_back(&*MI);
      }
    }

  for (auto MI : ToRemove) {
    MI->eraseFromParent();
  }
  return false;
}

bool SyncVMPreRAPeephole::combineStoreToStack(MachineFunction &MF) {
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

bool SyncVMPreRAPeephole::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM PreRA Peephole **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();
  MRI = &MF.getRegInfo();
  assert(MRI->isSSA() && "This pass requires MachineFunction to be SSA");
  assert(MRI->tracksLiveness() && "This pass requires MachineFunction to track "
                                  "liveness");

  bool Changed = false;

  Changed = combineStackToStackMoves(MF);
  // Changed = combineStoreToStack(MF);
  return Changed;
}

FunctionPass *llvm::createSyncVMPreRAPeepholePass() {
  return new SyncVMPreRAPeephole();
}
