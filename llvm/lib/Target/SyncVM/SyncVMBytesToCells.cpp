//===-- SyncVMBytesToCells.cpp - Replace bytes addresses with cells ones --===//
//
/// \file
/// LLVM emits byte addressing for stack accesses, and on SyncVM the stack
/// space is addressed in cells (32 bytes per cell). To make things more
/// complicated, some instruction generated stack pointers are already
/// cell-addressed, for example: `context.sp` returns stack pointer in cells.
///
/// To handle such inconsistencies, This pass ensures that after the pass, all
/// bytes addressings of stack are converted to cells addressings. This is
/// necessary for SyncVM target to ensure stack accesses.
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

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_BYTES_TO_CELLS_NAME; }

private:
  /// Convert an operand of a stack access instruction to cell addressing.
  /// Return true of a conversion has been done.
  bool convertStackMachineInstr(MachineInstr::mop_iterator OpIt);

  /// return iterator to the stack access operand of \p MI. If there is no stack
  /// accesses, return an empty iterator.
  MachineInstr::mop_iterator getStackAccess(MachineInstr &MI);

  /// return iterator to the second stack access operand of \p MI. If there is
  /// no second stack access, return an empty iterator.
  MachineInstr::mop_iterator getSecondStackAccess(MachineInstr &MI);

  /// Fold conversion to cells with shift left: If the source of the
  /// byte-addressed pointer is from a shift left, we can simply remove the
  /// shift and use the un-shifted register instead because the un-shifted
  /// register is already cell-addressed. Return true if the folding is
  /// successful.
  bool foldWithLeftShift(Register);

  const TargetInstrInfo *TII;
  MachineRegisterInfo *MRI;

  DenseMap<Register, Register> BytesToCellsRegs;
};

char SyncVMBytesToCells::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMBytesToCells, DEBUG_TYPE, SYNCVM_BYTES_TO_CELLS_NAME,
                false, false)

bool SyncVMBytesToCells::foldWithLeftShift(Register Reg) {
  assert(Reg.isVirtual() && "Physical registers are not expected");
  MachineInstr *DefMI = MRI->getVRegDef(Reg);
  if (!DefMI)
    return false;

  if (DefMI->getOpcode() != SyncVM::SHLxrr_s ||
      getImmOrCImm(*SyncVM::in0Iterator(*DefMI)) != Log2CellSizeInBytes)
    return false;

  Register UnshiftedReg = SyncVM::in1Iterator(*DefMI)->getReg();
  if (!MRI->hasOneUse(UnshiftedReg))
    return false;

  MRI->replaceRegWith(Reg, UnshiftedReg);
  DefMI->eraseFromParent();
  return true;
}

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

      auto StackIt = getStackAccess(MI);
      if (!StackIt)
        continue;
      Changed |= convertStackMachineInstr(StackIt);

      StackIt = getSecondStackAccess(MI);
      if (StackIt)
        Changed |= convertStackMachineInstr(StackIt);
    }
    BytesToCellsRegs.clear();
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

bool SyncVMBytesToCells::convertStackMachineInstr(
    MachineInstr::mop_iterator OpIt) {
  MachineOperand &MO0Reg = *(OpIt + 1);
  MachineOperand &MO1Global = *(OpIt + 2);
  MachineInstr &MI = *OpIt->getParent();
  if (MO0Reg.isReg()) {
    Register Reg = MO0Reg.getReg();
    assert(Reg.isVirtual() && "Physical registers are not expected");
    MachineInstr *DefMI = MRI->getVRegDef(Reg);

    // corner case: as per spec, stack pointer retrieved from context.sp is
    // cell-addressed, while everything else is byte-addressed.
    if (DefMI->getOpcode() == SyncVM::CTXr)
      return false;

    if (foldWithLeftShift(Reg))
      return true;

    Register NewVR;
    if (BytesToCellsRegs.count(Reg) == 1) {
      // Already converted, use value from the cache.
      NewVR = BytesToCellsRegs[Reg];
      ++NumBytesToCells;
    } else {
      NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
      MachineBasicBlock *DefBB = MI.getParent();
      auto DefIt = MI.getIterator();

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

MachineInstr::mop_iterator
SyncVMBytesToCells::getStackAccess(MachineInstr &MI) {
  // check if the stack access is in input operands
  if (SyncVM::hasSRInAddressingMode(MI))
    return SyncVM::in0Iterator(MI);

  // check if the stack access is in output operands
  if (SyncVM::hasSROutAddressingMode(MI))
    return SyncVM::out0Iterator(MI);

  return {};
}

MachineInstr::mop_iterator
SyncVMBytesToCells::getSecondStackAccess(MachineInstr &MI) {
  if (SyncVM::hasSRInAddressingMode(MI) && SyncVM::hasSROutAddressingMode(MI))
    return SyncVM::out0Iterator(MI);
  return {};
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMBytesToCellsPass() {
  return new SyncVMBytesToCells();
}
