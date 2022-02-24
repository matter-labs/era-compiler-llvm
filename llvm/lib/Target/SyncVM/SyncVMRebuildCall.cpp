//===---------------- SyncVMRebuildCall.cpp - Rebuild Call Pass ----------===//
//
/// \file
/// Implement the pass to build a valid CALL instruction.
//
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-rebuildcall"
#define SYNCVM_REBUILDCALL "SyncVM Rebuild CALL"

namespace {

class SyncVMRebuildCall : public MachineFunctionPass {
public:
  static char ID;
  SyncVMRebuildCall() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_REBUILDCALL; }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;

  MachineInstr* getFollowedByFlagInst(MachineInstr* MI) const;
};

char SyncVMRebuildCall::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMRebuildCall, DEBUG_TYPE, SYNCVM_REBUILDCALL, false, false)

MachineInstr* SyncVMRebuildCall::getFollowedByFlagInst(MachineInstr* MI) const {
  MachineBasicBlock* MBB = MI->getParent();
  auto MBBI = std::next(MachineBasicBlock::iterator(MI));
  for (; MBBI != MBB->end(); ++MBBI) {
    // call followed by another call
    if (MBBI->getOpcode() == SyncVM::NEAR_CALL) {
      return nullptr;
    }
    if (MBBI->isTerminator()) {
      return nullptr;
    }
    if (MBBI->getOpcode() == SyncVM::GtFlag ||
        MBBI->getOpcode() == SyncVM::LtFlag) {
      return &*MBBI;
    }
  }
  return nullptr;
}

bool SyncVMRebuildCall::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM Rebuild Calls **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr*> ToBeErased;

  bool Changed = false;

  auto isTargetInstruction = [&](unsigned opcode) {
    switch (opcode) {
      case SyncVM::NEAR_CALL:
      case SyncVM::FAR_CALL:
        return true;
      default:
        return false;
    }
  };

  // Iterate over instructions, find call instructions and subsequent GtFlag
  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      unsigned opcode = MI->getOpcode(); 
      if (isTargetInstruction(opcode)) {
        MachineInstr *flag = getFollowedByFlagInst(&*MI);
        if (flag != nullptr) {
          LLVM_DEBUG(dbgs() << "Instruction followed by Flag:"; MI->dump(); dbgs() << "\n";);
          LLVM_DEBUG(dbgs() << "Flag Instruction:"; flag->dump(); dbgs() << "\n";);

          // find the branch folder
          // lialan: fragile connection between flag and the following J. should better model
          // the connection.
          auto JI = std::next(MachineBasicBlock::iterator(flag));
          assert(JI->getOpcode() == SyncVM::J);

          // Get the landing pad branching:
          MachineOperand& eh_label = JI->getOperand(0);
          MachineOperand& cc = JI->getOperand(1);
          // must be a GT test
          assert((cc.isImm() && cc.getImm() != 0) ||
                 (cc.isCImm() && !cc.getCImm()->isZero()));

          // rebuild Call instruction
          BuildMI(MBB, MI, MI->getDebugLoc(), TII->get(opcode))
              .add(MI->getOperand(0))
              .add(MI->getOperand(1))
              .add(eh_label);

          // delete unneeded instructions:
          ToBeErased.push_back(&*MI);
          ToBeErased.push_back(flag);
          ToBeErased.push_back(&*JI);

        } else {
          LLVM_DEBUG(dbgs() << "Not followed by Flag, skipping:"; MI->dump());
        }
      }
    }



  for (auto* I : ToBeErased) {
    I->eraseFromParent();
  }

  return Changed;
}

FunctionPass *llvm::createSyncVMRebuildCallPass() { return new SyncVMRebuildCall(); }
