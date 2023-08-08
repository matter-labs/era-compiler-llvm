//===-- SyncVMBytesToCells.cpp - Replace bytes addresses with cells ones --===//
//
/// \file
/// This file contains a pass that corrects stack addressing to replace
/// addresses in bytes with addresses in cells for instructions addressing
/// stack.
/// This pass is necessary for SyncVM target to ensure stack accesses, because
/// LLVM will emit byte addressing for stack accesses, and on SyncVM the stack
/// space is addressed in cells (32 bytes per cell). Without this pass the
/// generated code would not work correctly on SyncVM.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-bytes-to-cells"
#define SYNCVM_BYTES_TO_CELLS_NAME "SyncVM bytes to cells"

static constexpr unsigned CellSizeInBytes = 32;
static constexpr unsigned Log2CellSizeInBytes = 5;

STATISTIC(NumBytesToCells, "Number of bytes to cells conversions gone");

namespace {

class SyncVMBytesToCells : public MachineFunctionPass {
public:
  static char ID;
  SyncVMBytesToCells() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool convertStackAccesses(MachineFunction &MF);

  bool runOnMachineFunction(MachineFunction &Fn) override;
  bool convertStackMachineInstr(MachineInstr::mop_iterator OpIt);

  StringRef getPassName() const override { return SYNCVM_BYTES_TO_CELLS_NAME; }

private:
  const TargetInstrInfo *TII;
  MachineRegisterInfo *MRI;

  DenseMap<Register, Register> BytesToCellsRegs;
};

char SyncVMBytesToCells::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMBytesToCells, DEBUG_TYPE, SYNCVM_BYTES_TO_CELLS_NAME,
                false, false)

bool SyncVMBytesToCells::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MRI = &MF.getRegInfo();
  assert(MRI && MRI->isSSA() &&
         "The pass is supposed to be run on SSA form MIR");

  bool Changed = false;
  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;

      auto StackIt = SyncVM::getStackAccess(MI);
      if (!StackIt)
        continue;
      Changed |= convertStackMachineInstr(StackIt);

      StackIt = SyncVM::getSecondStackAccess(MI);
      if (StackIt)
        Changed |= convertStackMachineInstr(StackIt);
    }
    BytesToCellsRegs.clear();
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

/// Convert an operand of a stack access instruction to cell addressing.
bool SyncVMBytesToCells::convertStackMachineInstr(
    MachineInstr::mop_iterator OpIt) {
  MachineOperand &MO0Reg = *(OpIt + 1);
  MachineOperand &MO1Global = *(OpIt + 2);
  MachineInstr &MI = *OpIt->getParent();
  if (MO0Reg.isReg()) {
    Register Reg = MO0Reg.getReg();
    assert(Reg.isVirtual() && "Physical registers are not expected");
    MachineInstr *DefMI = MRI->getVRegDef(Reg);
    // context.sp is already in cells
    if (DefMI->getOpcode() == SyncVM::CTXr)
      return false;

    // Shortcut:
    // if we insert shr.s, sometimes we have the following pattern:
    // shl.s   5, r1, r1
    // shr.s   5, r1, r1
    // Which is redundant. We can simply remove the shift.
    if (DefMI->getOpcode() == SyncVM::SHLxrr_s &&
        getImmOrCImm(*SyncVM::in0Iterator(*DefMI)) == Log2CellSizeInBytes) {
      // replace all uses of the register with the second operand.
      Register UnshiftedReg = SyncVM::in1Iterator(*DefMI)->getReg();
      if (MRI->hasOneUse(UnshiftedReg)) {
        Register UseReg = MO0Reg.getReg();
        MRI->replaceRegWith(UseReg, UnshiftedReg);
        DefMI->eraseFromParent();
        return true;
      }
    }

    Register NewVR;
    if (BytesToCellsRegs.count(Reg) == 1) {
      // Already converted, use value from the cache.
      NewVR = BytesToCellsRegs[Reg];
      ++NumBytesToCells;
    } else {
      NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
      MachineBasicBlock *DefBB = MI.getParent();
      auto DefIt = [DefBB, &MI]() {
        return find_if(*DefBB, [&MI](const MachineInstr &CurrentMI) {
          return &MI == &CurrentMI;
        });
      }();
      assert(DefIt->getParent() == DefBB);

      // convert bytes to cells by right shifting
      BuildMI(*DefBB, DefIt, MI.getDebugLoc(), TII->get(SyncVM::SHRxrr_s))
          .addDef(NewVR)
          .addImm(Log2CellSizeInBytes)
          .addReg(Reg)
          .addImm(SyncVMCC::COND_NONE);
    }
    BytesToCellsRegs[Reg] = NewVR;
    LLVM_DEBUG(dbgs() << "Adding Reg to Stack access list: "
                      << Reg.virtRegIndex() << '\n');
    MO0Reg.ChangeToRegister(NewVR, false);
  }
  if (MO1Global.isGlobal()) {
    unsigned Offset = MO1Global.getOffset();
    MO1Global.setOffset(Offset /= CellSizeInBytes);
  }
  MachineOperand &Const = *(OpIt + 2);
  if (Const.isImm() || Const.isCImm())
    Const.ChangeToImmediate(getImmOrCImm(Const) / CellSizeInBytes);
  return true;
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMBytesToCellsPass() {
  return new SyncVMBytesToCells();
}
