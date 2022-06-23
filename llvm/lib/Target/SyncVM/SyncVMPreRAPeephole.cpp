//===---------------- SyncVMPreRAPeephole.cpp - Peephole optimization ----------===//
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

#define DEBUG_TYPE "syncvm-prera-peephole"
#define SYNCVM_PRERA_PEEPHOLE "SyncVM pre-RA peephole optimization"

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

  bool isMoveRegToStack(MachineInstr &MI) const;
};

char SyncVMPreRAPeephole::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMPreRAPeephole, DEBUG_TYPE, SYNCVM_PRERA_PEEPHOLE, false, false)

bool SyncVMPreRAPeephole::isMoveRegToStack(MachineInstr &MI) const {
  return MI.getOpcode() == SyncVM::ADDrrs_s &&
         MI.getOperand(1).getReg() == SyncVM::R0 &&
         getImmOrCImm(MI.getOperand(MI.getNumOperands() - 1)) == 0;
}

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
bool SyncVMPreRAPeephole::combineStackToStackMoves(MachineFunction &MF) {
  auto isMoveStackToReg = [](MachineInstr &MI) {
    return MI.getOpcode() == SyncVM::ADDsrr_s &&
           MI.getOperand(4).getReg() == SyncVM::R0 &&
           getImmOrCImm(MI.getOperand(MI.getNumOperands() - 1)) == 0;
  };
  auto isMoveCodeToReg = [](MachineInstr &MI) {
    return MI.getOpcode() == SyncVM::ADDcrr_s &&
           MI.getOperand(3).getReg() == SyncVM::R0 &&
           getImmOrCImm(MI.getOperand(MI.getNumOperands() - 1)) == 0;
  };

  std::vector<MachineInstr *> ToRemove;

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      bool isMoveCode = isMoveCodeToReg(*MI);
      bool isMoveStack = isMoveStackToReg(*MI);
      if (isMoveCode || isMoveStack) {
        LLVM_DEBUG(dbgs() << " . Found Move to Reg instruction: "; MI->dump());
        auto reg = MI->getOperand(0).getReg();
        // can only have one use
        if (!MRI->hasOneUse(reg)) {
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
          // the program will be incorrect in this case
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
  return ToRemove.size() > 0;
}

bool SyncVMPreRAPeephole::combineStoreToStack(MachineFunction &MF) {
  std::vector<MachineInstr *> ToRemove;
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
        if (!isMoveRegToStack(*StoreI)) {
          continue;
        }
        auto reg = MI->getOperand(0).getReg();
        if (!MRI->hasOneUse(reg)) {
          continue;
        }
        auto UseMI = MRI->use_nodbg_instructions(reg).begin();
        if (StoreI != &*UseMI || StoreI->getOperand(0).getReg() != reg) {
          continue;
        }
        LLVM_DEBUG(dbgs() << "Found candidate for combining: "; MI->dump();
                   StoreI->dump());
        DebugLoc DL = MI->getDebugLoc();

        auto NewMI =
            BuildMI(MBB, StoreI, DL, TII->get(Mapping[MI->getOpcode()]));
        NewMI.addDef(MI->getOperand(1).getReg());
        for (unsigned i = 2, e = MI->getNumOperands() - 1; i < e; ++i)
          NewMI.add(MI->getOperand(i));
        for (unsigned i = 2; i < 5; ++i)
          NewMI.add(StoreI->getOperand(i));
        NewMI.add(MI->getOperand(MI->getNumOperands() - 1));

        LLVM_DEBUG(dbgs() << "  Combined to: "; NewMI->dump());

        ToRemove.push_back(&*MI);
        ToRemove.push_back(&*StoreI);
      }
    }

  for (auto MI : ToRemove) {
    MI->eraseFromParent();
  }
  return ToRemove.size() > 0;
}

bool SyncVMPreRAPeephole::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM Peephole **********\n"
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
  //Changed = combineStoreToStack(MF);
  return Changed;
}

FunctionPass *llvm::createSyncVMPreRAPeepholePass() { return new SyncVMPreRAPeephole(); }
