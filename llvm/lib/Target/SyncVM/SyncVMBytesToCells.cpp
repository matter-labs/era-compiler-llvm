//===-- SyncVMBytesToCells.cpp - Replace bytes addresses with cells ones --===//
//
/// \file
/// This file contains a pass that replaces addresses in bytes with addresses
/// in cells for instructions addressing stack.
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

static cl::opt<bool>
    EarlyBytesToCells("early-bytes-to-cells-conversion", cl::init(false),
                      cl::Hidden,
                      cl::desc("Converts bytes to cells after the definition"));

STATISTIC(NumBytesToCells, "Number of bytes to cells convertions gone");

using RegMapType = DenseMap<Register, Register>;

namespace {

class SyncVMBytesToCells : public MachineFunctionPass {
public:
  static char ID;
  SyncVMBytesToCells() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_BYTES_TO_CELLS_NAME; }

private:
  void expandConst(MachineInstr &MI) const;
  void expandLoadConst(MachineInstr &MI) const;
  void expandThrow(MachineInstr &MI) const;
  const SyncVMInstrInfo *TII;
  LLVMContext *Context;

  // returns true if we should skip this instruction
  bool divideDefByCellSize(MachineInstr &MI, MachineOperand &MOReg,
                           DenseMap<Register, Register> &Map,
                           bool isStack);
  void divideGlobalOffsetByCellSize(MachineOperand &MO) const;
  void divideImmByCellSize(MachineOperand &MO) const;
  
  void convertGlobalAddressMI(MachineInstr &MI, unsigned OpNoum);
  void convertStackMI(MachineInstr &MI, unsigned OpNoum);
  llvm::MachineRegisterInfo *MRI;

  DenseMap<Register, Register> StackBytesToCellsRegs{};
  DenseMap<Register, Register> MemBytesToCellsRegs{};
};

char SyncVMBytesToCells::ID = 0;

} // namespace

void SyncVMBytesToCells::divideGlobalOffsetByCellSize(
    MachineOperand &MO) const {
  assert(MO.isGlobal() && "Expected global operand");
  MO.setOffset(MO.getOffset() / CellSizeInBytes);
}

void SyncVMBytesToCells::divideImmByCellSize(MachineOperand &Const) const {
  if (Const.isImm() || Const.isCImm())
    Const.ChangeToImmediate(getImmOrCImm(Const) / CellSizeInBytes);
}

bool SyncVMBytesToCells::divideDefByCellSize(MachineInstr &MI,
                                             MachineOperand &MOReg,
                                             DenseMap<Register, Register> &Map,
                                             bool isStack) {
  Register Reg = MOReg.getReg();
  assert(Reg.isVirtual() && "Physical registers are not expected");
  MachineInstr *DefMI = MRI->getVRegDef(Reg);
  if (isStack && DefMI->getOpcode() == SyncVM::CTXr)
    // context.sp is already in cells, so skip conversion.
    return true;
  Register NewVR;
  if (Map.count(Reg) == 1) {
    // Already converted, use value from the cache.
    NewVR = Map[Reg];
    ++NumBytesToCells;
  } else {
    // Shortcut:
    // if we insert DIV, sometimes we have the following pattern:
    // shl.s   5, r1, r1
    // div.s   32, r1, r1, r0
    // Which is redundant. We can simply remove the shift.
    if (DefMI->getOpcode() == SyncVM::SHLxrr_s &&
        getImmOrCImm(DefMI->getOperand(1)) == 5) {
      // replace all uses of the register with the second operand.
      Register UnshiftedReg = DefMI->getOperand(2).getReg();
      if (MRI->hasOneUse(UnshiftedReg)) {
        Register UseReg = MOReg.getReg();
        MRI->replaceRegWith(UseReg, UnshiftedReg);
        DefMI->eraseFromParent();
        return true;
      }
    }

    NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
    MachineBasicBlock *DefBB = [DefMI, &MI]() {
      if (EarlyBytesToCells)
        return DefMI->getParent();
      else
        return MI.getParent();
    }();
    DefMI = [DefBB, DefMI, &MI]() -> MachineInstr* {
      if (!EarlyBytesToCells)
        return &MI;
      if (!DefMI->isPHI())
        return &*std::next(DefMI->getIterator());
      return &*DefBB->getFirstNonPHI();
    }();
    assert(DefMI->getParent() == DefBB);
    BuildMI(*DefBB, DefMI, MI.getDebugLoc(), TII->get(SyncVM::DIVxrrr_s))
        .addDef(NewVR)
        .addDef(SyncVM::R0)
        .addImm(CellSizeInBytes)
        .addReg(Reg)
        .addImm(SyncVMCC::COND_NONE);
    Map[Reg] = NewVR;
  }
  MOReg.ChangeToRegister(NewVR, false);
  return false;
}

INITIALIZE_PASS(SyncVMBytesToCells, DEBUG_TYPE, SYNCVM_BYTES_TO_CELLS_NAME,
                false, false)

bool SyncVMBytesToCells::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  MRI = &MF.getRegInfo();
  assert(MRI->isSSA() && "The pass is supposed to be run on SSA form MIR");
  assert(TII && "TargetInstrInfo must be a valid object");

  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;
      if (!TII->mayHaveStackOpnd(MI))
        continue;

      for (unsigned OpNo = 0; OpNo < 3; ++OpNo) {
        if (TII->isStackOpnd(MI, OpNo)) {
          // avoid handling frame index addressing
          convertStackMI(MI, OpNo);
          Changed = true;
        } else if (TII->isCodeOpnd(MI, OpNo)) {
          convertGlobalAddressMI(MI, OpNo);
          Changed = true;
        }
      }
    }
    if (!EarlyBytesToCells) {
      StackBytesToCellsRegs.clear();
      MemBytesToCellsRegs.clear();
    }
  }

  MemBytesToCellsRegs.clear();
  StackBytesToCellsRegs.clear();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

void SyncVMBytesToCells::convertGlobalAddressMI(MachineInstr &MI,
                                                unsigned OpNum) {
  // handle the case `@label[offset]`:
  unsigned Op0Start = TII->getOpndStartLoc(MI, OpNum);
  MachineOperand &MO0Reg = MI.getOperand(Op0Start);
  MachineOperand &MO1Global = MI.getOperand(Op0Start + 1);

  if (MO1Global.isGlobal())
    divideGlobalOffsetByCellSize(MO1Global);
  if (MO0Reg.isReg())
    divideDefByCellSize(MI, MO0Reg, MemBytesToCellsRegs, false);
}

void SyncVMBytesToCells::convertStackMI(MachineInstr &MI,
                                        unsigned OpNum) {
  unsigned Op0Start = TII->getOpndStartLoc(MI, OpNum);
  MachineOperand &MO0Reg = MI.getOperand(Op0Start + 1);
  MachineOperand &MO1Global = MI.getOperand(Op0Start + 2);
  if (MO1Global.isGlobal())
    divideGlobalOffsetByCellSize(MO1Global);
  if (MO0Reg.isReg() &&
      divideDefByCellSize(MI, MO0Reg, StackBytesToCellsRegs, true))
    return;
  MachineOperand &Const = MI.getOperand(Op0Start + 2);
  divideImmByCellSize(Const);
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMBytesToCellsPass() {
  return new SyncVMBytesToCells();
}
