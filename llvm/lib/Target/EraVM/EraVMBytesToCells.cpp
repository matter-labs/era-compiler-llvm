//===-- EraVMBytesToCells.cpp - Replace bytes addresses with cells ones ---===//
//
/// \file
/// LLVM emits byte addressing for stack accesses, and on EraVM the stack
/// space is addressed in cells (32 bytes per cell). To make things more
/// complicated, some instruction generated stack pointers are already
/// cell-addressed, for example: `context.sp` returns stack pointer in cells.
///
/// To handle such inconsistencies, This pass ensures that after the pass, all
/// bytes addressings of stack are converted to cells addressings. This is
/// necessary for EraVM target to ensure stack accesses.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-bytes-to-cells"
#define ERAVM_BYTES_TO_CELLS_NAME "EraVM bytes to cells"

static constexpr unsigned CellSizeInBytes = 32;
static constexpr unsigned Log2CellSizeInBytes = 5;

STATISTIC(NumBytesToCells, "Number of bytes to cells conversions gone");

namespace {

class EraVMBytesToCells : public MachineFunctionPass {
public:
  static char ID;
  EraVMBytesToCells() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI{};

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_BYTES_TO_CELLS_NAME; }

private:
  /// Convert an operand of a stack access instruction to cell addressing.
  /// Return true if a conversion has been done.
  bool convertStackMachineInstr(MachineInstr::mop_iterator OpIt);

  /// Convert an operand of a code page accessing to cell addressing. Return
  /// true if a conversion has been done.
  void convertCodeAddressMachineInstr(MachineInstr::mop_iterator OpIt);

  /// return iterator to the stack access operand of \p MI. If there is no stack
  /// accesses, return default constructed iterator.
  MachineInstr::mop_iterator getStackAccess(MachineInstr &MI);

  /// return iterator to the second stack access operand of \p MI. If there is
  /// no second stack access, return default constructed iterator.
  MachineInstr::mop_iterator getSecondStackAccess(MachineInstr &MI);

  /// return iterator to the code operand of \p MI. If there is no code operand
  /// return default constructed iterator.
  MachineInstr::mop_iterator getCodeAccess(MachineInstr &MI);

  /// Fold conversion to cells with shift left: If the source of the
  /// byte-addressed pointer is from a shift left, we can simply remove the
  /// shift and use the un-shifted register instead because the un-shifted
  /// register is already cell-addressed. Return the cell addressing pointer if
  /// successful.
  std::optional<Register> foldWithLeftShift(Register);

  /// Convert a register machine operand which contains byte addressing pointer
  /// to cell addressing pointer. Return the converted, cell-addressing
  /// register.
  Register convertRegisterPointerToCells(MachineOperand &MOReg);

  /// Convert the byte-addressing stack operands of an instruction to
  /// cell-addressing stack operands.
  bool convertStackAccesses(MachineInstr &MI);

  /// Convert the byte-addressing code operands of an instruction to
  /// cell-addressing code operands.
  bool convertCodeAccess(MachineInstr &MI);

  const EraVMInstrInfo *TII{};
  MachineRegisterInfo *MRI{};

  DenseMap<Register, Register> BytesToCellsRegs;
};

char EraVMBytesToCells::ID = 0;

} // namespace

INITIALIZE_PASS(EraVMBytesToCells, DEBUG_TYPE, ERAVM_BYTES_TO_CELLS_NAME, false,
                false)

std::optional<Register> EraVMBytesToCells::foldWithLeftShift(Register Reg) {
  assert(Reg.isVirtual() && "Physical registers are not expected");
  MachineInstr *DefMI = MRI->getVRegDef(Reg);
  if (!DefMI)
    return std::nullopt;

  if (DefMI->getOpcode() != EraVM::SHLxrr_s ||
      getImmOrCImm(*EraVM::in0Iterator(*DefMI)) != Log2CellSizeInBytes)
    return std::nullopt;

  if (MRI->hasOneUse(Reg))
    DefMI->eraseFromParent();
  return EraVM::in1Iterator(*DefMI)->getReg();
}

bool EraVMBytesToCells::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MRI = &MF.getRegInfo();
  assert(MRI && MRI->isSSA() &&
         "The pass is supposed to be run on SSA form MIR");

  bool Changed = false;
  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;
      Changed |= convertStackAccesses(MI);
      Changed |= convertCodeAccess(MI);
    }
    BytesToCellsRegs.clear();
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

Register
EraVMBytesToCells::convertRegisterPointerToCells(MachineOperand &MOReg) {
  MachineInstr &MI = *MOReg.getParent();
  const Register Reg = MOReg.getReg();
  assert(Reg.isVirtual() && "Expecting virtual register");

  MachineInstr *DefMI = MRI->getVRegDef(Reg);
  assert(DefMI);

  if (auto FoldedReg = foldWithLeftShift(Reg))
    return *FoldedReg;

  Register NewVR;
  if (BytesToCellsRegs.count(Reg) == 1) {
    // Already converted, use value from the cache.
    NewVR = BytesToCellsRegs[Reg];
    ++NumBytesToCells;
    return NewVR;
  }
  NewVR = MRI->createVirtualRegister(&EraVM::GR256RegClass);
  MachineBasicBlock *DefBB = MI.getParent();
  auto DefIt = MI.getIterator();

  // convert bytes to cells by right shifting
  BuildMI(*DefBB, DefIt, MI.getDebugLoc(), TII->get(EraVM::SHRxrr_s))
      .addDef(NewVR)
      .addImm(Log2CellSizeInBytes)
      .addReg(Reg)
      .addImm(EraVMCC::COND_NONE);
  return NewVR;
}

/// Convert an operand of a stack access instruction to cell addressing.
bool EraVMBytesToCells::convertStackMachineInstr(
    MachineInstr::mop_iterator OpIt) {
  MachineOperand &MO0Reg = *(OpIt + 1);
  MachineOperand &MO1Global = *(OpIt + 2);

  // To convert to cell addressing, we need to convert the followings:
  // 1. stack pointer, if in stack pointer relative addressing mode
  // 2. immediate offsets, if any
  // 3. offsets in global addresses, if any

  // If the second operand is a register, this stack access is in stack pointer
  // relative addressing mode.
  if (MO0Reg.isReg()) {
    const Register Reg = MO0Reg.getReg();
    assert(Reg.isVirtual() && "Physical registers are not expected");
    MachineInstr *DefMI = MRI->getVRegDef(Reg);

    // corner case: as per spec, stack pointer retrieved from context.sp is
    // cell-addressed, while everything else is byte-addressed.
    if (DefMI->getOpcode() == EraVM::CTXr)
      return false;

    // FRAMEirrr is doing sp + reg + imm, and since sp is cell-addressed we only
    // need to remove left shift from the reg to get correct calculation.
    if (DefMI->getOpcode() == EraVM::FRAMEirrr) {
      MachineOperand &MOReg = DefMI->getOperand(2 /* reg */);
      if (auto FoldedReg = foldWithLeftShift(MOReg.getReg()))
        MOReg.ChangeToRegister(*FoldedReg, false);
    } else {
      const Register NewVR = convertRegisterPointerToCells(MO0Reg);
      BytesToCellsRegs[Reg] = NewVR;
      LLVM_DEBUG(dbgs() << "Adding Reg to Stack access list: "
                        << Reg.virtRegIndex() << '\n');
      MO0Reg.ChangeToRegister(NewVR, false);
    }
  }

  // convert global and immediate offsets to cell addressing
  if (MO1Global.isGlobal())
    MO1Global.setOffset(MO1Global.getOffset() / CellSizeInBytes);

  MachineOperand &Const = *(OpIt + 2);
  if (Const.isImm() || Const.isCImm())
    Const.ChangeToImmediate(getImmOrCImm(Const) / CellSizeInBytes);
  return true;
}

MachineInstr::mop_iterator EraVMBytesToCells::getStackAccess(MachineInstr &MI) {
  // check if the stack access is in input operands
  if (EraVM::hasSRInAddressingMode(MI))
    return EraVM::in0Iterator(MI);

  // check if the stack access is in output operands
  if (EraVM::hasSROutAddressingMode(MI))
    return EraVM::out0Iterator(MI);

  return {};
}

MachineInstr::mop_iterator
EraVMBytesToCells::getSecondStackAccess(MachineInstr &MI) {
  if (EraVM::hasSRInAddressingMode(MI) && EraVM::hasSROutAddressingMode(MI))
    return EraVM::out0Iterator(MI);
  return {};
}

MachineInstr::mop_iterator EraVMBytesToCells::getCodeAccess(MachineInstr &MI) {
  if (EraVM::hasCRInAddressingMode(MI))
    return EraVM::in0Iterator(MI);
  return {};
}

void EraVMBytesToCells::convertCodeAddressMachineInstr(
    MachineInstr::mop_iterator OpIt) {
  MachineOperand &MO0Reg = *OpIt;
  MachineOperand &MO1Global = *(OpIt + 1);

  // global address' offset is in bytes
  if (MO1Global.isGlobal())
    MO1Global.setOffset(MO1Global.getOffset() / CellSizeInBytes);

  // pointer in register is in bytes
  if (MO0Reg.isReg()) {
    const Register NewVR = convertRegisterPointerToCells(MO0Reg);
    BytesToCellsRegs[MO0Reg.getReg()] = NewVR;
    MO0Reg.ChangeToRegister(NewVR, false);
  }

  // if we have this kinds of pattern: `code[@var + 1 + r1]`
  // note that assembler cannot parse and combine (@var + 1), we need to
  // combine r1 with immediate value. To address this, insert an instruction
  // to add the immediate value to r1 before code page addressing.
  // CPR-1280: update assembly to support this pattern, and remove this handling
  if (MO0Reg.isReg() && MO1Global.isGlobal() && MO1Global.getOffset() != 0) {
    MachineInstr &MI = *MO0Reg.getParent();
    // combine register with immediate value
    const Register NewVR = MRI->createVirtualRegister(&EraVM::GR256RegClass);
    BuildMI(*MI.getParent(), MI.getIterator(), MI.getDebugLoc(),
            TII->get(EraVM::ADDirr_s))
        .addDef(NewVR)
        .addImm(MO1Global.getOffset())
        .addReg(MO0Reg.getReg())
        .addImm(EraVMCC::COND_NONE);
    MO0Reg.ChangeToRegister(NewVR, false);
    MO1Global.setOffset(0);
  }
}

bool EraVMBytesToCells::convertCodeAccess(MachineInstr &MI) {
  auto *CodeIt = getCodeAccess(MI);
  if (!CodeIt)
    return false;
  convertCodeAddressMachineInstr(CodeIt);
  return true;
}

bool EraVMBytesToCells::convertStackAccesses(MachineInstr &MI) {
  auto *StackIt = getStackAccess(MI);
  if (!StackIt)
    return false;

  convertStackMachineInstr(StackIt);
  StackIt = getSecondStackAccess(MI);
  if (StackIt)
    convertStackMachineInstr(StackIt);
  return true;
}

/// createEraVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createEraVMBytesToCellsPass() {
  return new EraVMBytesToCells();
}
