//===-- SyncVMBytesToCells.cpp - Replace bytes addresses with cells ones --===//
//
/// \file
/// This file contains a pass that corrects stack addressing:
/// 1. generally, replaces addresses in bytes with addresses in cells for
/// instructions addressing stack.
/// 2. make sure when we are passing stack pointers to a function, we pass not
/// in bytes but in cells.
/// This pass is necessary for SyncVM target to ensure stack accesses, because
/// LLVM will emit byte addressing for stack accesses, and on SyncVM the stack
/// space is addressed in cells (32 bytes per cell). Without this pass the
/// generated code wouldn't work correctly.
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

STATISTIC(NumBytesToCells, "Number of bytes to cells conversions gone");

namespace {

class SyncVMBytesToCells : public MachineFunctionPass {
public:
  static char ID;
  SyncVMBytesToCells() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool handleCopyToPhyReg(MachineInstr &MI) const;
  bool convertStackAccesses(MachineFunction &MF);
  bool convertStackPointerArithmeticsAndReturns(MachineFunction &MF);

  bool runOnMachineFunction(MachineFunction &Fn) override;
  bool convertStackMachineInstr(MachineInstr::mop_iterator OpIt);

  StringRef getPassName() const override { return SYNCVM_BYTES_TO_CELLS_NAME; }

private:
  void multiplyByCellSize(MachineInstr::mop_iterator Mop) const;
  void divideByCellSize(MachineInstr::mop_iterator Mop) const;
  void scalingHelper(MachineInstr::mop_iterator Mop, unsigned Opcode) const;

  void collectArgumentsAsRegisters(MachineFunction &MF);

  bool isCopyReturnValue(MachineInstr &MI) const;
  bool isPassedInCells(Register) const;
  bool scalePointerArithmeticIfNeeded(MachineInstr *MI) const;

  bool mayContainCells(Register Reg) const;
  bool isUsedAsStackAddress(Register Reg) const;

  const TargetInstrInfo *TII;
  MachineRegisterInfo *MRI;

  std::vector<unsigned> MayContainCells;
  std::vector<unsigned> VRegsUsedInStackAddressing;
  DenseMap<Register, Register> BytesToCellsRegs;
};

char SyncVMBytesToCells::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMBytesToCells, DEBUG_TYPE, SYNCVM_BYTES_TO_CELLS_NAME,
                false, false)

void SyncVMBytesToCells::scalingHelper(MachineInstr::mop_iterator Mop,
                                       unsigned Opcode) const {
  assert(Mop->isReg() && "Expected register operand");
  Register Reg = Mop->getReg();
  assert(Reg.isVirtual() && "Physical registers are not expected");

  MachineInstr *MI = Mop->getParent();
  assert(MI);

  auto NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
  BuildMI(*MI->getParent(), MI->getIterator(), MI->getDebugLoc(),
          TII->get(Opcode))
      .addDef(NewVR)
      .addDef(SyncVM::R0)
      .addImm(CellSizeInBytes)
      .addReg(Reg)
      .addImm(SyncVMCC::COND_NONE);
  Mop->ChangeToRegister(NewVR, false);
}

void SyncVMBytesToCells::multiplyByCellSize(
    MachineInstr::mop_iterator Mop) const {
  scalingHelper(Mop, SyncVM::MULirrr_s);
}

void SyncVMBytesToCells::divideByCellSize(
    MachineInstr::mop_iterator Mop) const {
  scalingHelper(Mop, SyncVM::DIVxrrr_s);
}

/// Given an instruction, if the instruction is about pointer arithmetic,
/// then scale the offset by cell size; otherwise, do nothing.
bool SyncVMBytesToCells::scalePointerArithmeticIfNeeded(
    MachineInstr *MI) const {
  assert(MI && "Input must not be a nullptr");

  auto Opc = MI->getOpcode();
  // Stack pointer arithmetics uses ADDrrr_s and ADDirr_s, as they come
  // from GEP. For constant operations, the base reg is the second operand;
  // For variable operation, the base reg is in the first operand.
  if (Opc != SyncVM::ADDrrr_s && Opc != SyncVM::ADDirr_s)
    return false;
  auto BaseOpnd = (Opc == SyncVM::ADDrrr_s) ? SyncVM::in0Iterator(*MI)
                                            : SyncVM::in1Iterator(*MI);
  Register Base = BaseOpnd->getReg();
  if (!Base.isVirtual() || !mayContainCells(Base))
    return false;

  multiplyByCellSize(BaseOpnd);
  return true;
}

/// Identify registers that are used as stack pointers and is coming from
/// arguments. If so, mark them as do not scale.
void SyncVMBytesToCells::collectArgumentsAsRegisters(MachineFunction &MF) {
  auto &BB = MF.front();
  for (MachineInstr &MI : BB) {
    // we are collecting arguments on machine level. To identify arguments,
    // look for COPY instructions in the entry block where it copies physical
    // register to a virtual register. Those copies are generated from ISel's
    // CopyFromReg, and are in entry block. However, CALLs in the entry block
    // will also be followed by same COPY instructions. We will stop
    // collecting when we see a CALL as all arguments copies should appear
    // before CALL due to dependency.
    if (MI.isCall())
      break;
    if (!MI.isCopy())
      continue;
    Register OutReg = SyncVM::out0Iterator(MI)->getReg();
    if (!OutReg.isVirtual())
      continue;
    MayContainCells.push_back(OutReg.virtRegIndex());
  }
}

/// Return if a register might contain cell addressing info.
bool SyncVMBytesToCells::mayContainCells(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  return llvm::find(MayContainCells, Reg.virtRegIndex()) !=
         MayContainCells.end();
}

/// Returns if a register is used as stack address.
bool SyncVMBytesToCells::isUsedAsStackAddress(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  return llvm::find(VRegsUsedInStackAddressing, Reg.virtRegIndex()) !=
         VRegsUsedInStackAddressing.end();
}

/// Returns if a register definitely contains cell addressing info.
bool SyncVMBytesToCells::isPassedInCells(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  return mayContainCells(Reg) && isUsedAsStackAddress(Reg);
}

/// Returns true if the instruction is a copy to a physical register from a
/// virtual register.
static bool isCopyToPhyReg(MachineInstr &MI) {
  return MI.getOpcode() == SyncVM::COPY &&
         !SyncVM::out0Iterator(MI)->getReg().isVirtual() &&
         SyncVM::in0Iterator(MI)->getReg().isVirtual();
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

  MayContainCells.clear();
  VRegsUsedInStackAddressing.clear();
  BytesToCellsRegs.clear();

  collectArgumentsAsRegisters(MF);
  Changed |= convertStackAccesses(MF);
  Changed |= convertStackPointerArithmeticsAndReturns(MF);

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
    if (DefMI->getOpcode() == SyncVM::CTXr)
      // context.sp is already in cells.
      return false;

    // Shortcut:
    // if we insert DIV, sometimes we have the following pattern:
    // shl.s   5, r1, r1
    // div.s   32, r1, r1, r0
    // Which is redundant. We can simply remove the shift.
    if (DefMI->getOpcode() == SyncVM::SHLxrr_s &&
        getImmOrCImm(*SyncVM::in0Iterator(*DefMI)) == 5) {
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

      // if the Reg is coming from argument list, then it is already in cells.
      // so we will skip DIV insertion.
      if (mayContainCells(Reg))
        NewVR = Reg;
      else {
        // check that if the base register is already in cells.
        // if so, we need not to insert DIV, but descale the offset.
        auto DefInstr = MRI->getVRegDef(Reg);
        // pointer arithmetic
        if (DefInstr)
          scalePointerArithmeticIfNeeded(DefInstr);

        // Insert DIV before the instruction.
        BuildMI(*DefBB, DefIt, MI.getDebugLoc(), TII->get(SyncVM::DIVxrrr_s))
            .addDef(NewVR)
            .addDef(SyncVM::R0)
            .addImm(CellSizeInBytes)
            .addReg(Reg)
            .addImm(SyncVMCC::COND_NONE);
      }
      BytesToCellsRegs[Reg] = NewVR;
      VRegsUsedInStackAddressing.push_back(Reg.virtRegIndex());
      LLVM_DEBUG(dbgs() << "Adding Reg to Stack access list: "
                        << Reg.virtRegIndex() << '\n');
    }
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

/// If a stack pointer in current frame is used as an argument to a call,
/// convert its byte value intoto cell value.
bool SyncVMBytesToCells::handleCopyToPhyReg(MachineInstr &MI) const {
  auto SrcReg = SyncVM::in0Iterator(MI)->getReg();
  auto DefInstr = MRI->getVRegDef(SrcReg);
  if (DefInstr && DefInstr->getOpcode() == SyncVM::ADDframe) {
    divideByCellSize(SyncVM::in0Iterator(MI));
    return true;
  }
  return false;
}

bool SyncVMBytesToCells::convertStackAccesses(MachineFunction &MF) {
  bool Changed = false;
  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;
      if (isCopyToPhyReg(MI)) {
        Changed |= handleCopyToPhyReg(MI);
      }

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
  return Changed;
}

/// Handle pointer arithmetics and returned stack pointer.  We have to do it
/// separately in a second pass is because it needs the information collected
/// by the first pass.
bool SyncVMBytesToCells::convertStackPointerArithmeticsAndReturns(
    MachineFunction &MF) {
  bool Changed = false;
  for (auto &BB : MF) {
    for (MachineInstr &MI : BB) {
      if (MI.getOpcode() == SyncVM::ADDirr_s) {
        auto SPOpnd = SyncVM::in1Iterator(MI);
        if (isPassedInCells(SPOpnd->getReg())) {
          multiplyByCellSize(SPOpnd);
          Changed = true;
        }
      } else if (MI.isReturn()) {
        auto ReverseIt = std::next(MI.getReverseIterator());
        while (ReverseIt != BB.rend() && isCopyReturnValue(*ReverseIt)) {
          auto RegOpnd = SyncVM::in0Iterator(*ReverseIt);
          if (isPassedInCells(RegOpnd->getReg())) {
            multiplyByCellSize(RegOpnd);
            Changed = true;
          }
          ++ReverseIt;
        }
      }
    }
  }
  return Changed;
}

/// match instructions with format: $r1 = COPY %0
/// which are used to copy return value from a virtual to a physical
bool SyncVMBytesToCells::isCopyReturnValue(MachineInstr &MI) const {
  if (!isCopyToPhyReg(MI))
    return false;
  // look for the copy instruction before RET. Those instructions are generated
  // by CopyToReg, and are in the same block as RET.
  auto NextMI = std::next(MI.getIterator());
  auto *BB = MI.getParent();
  while (NextMI != BB->end() && NextMI->isCopy())
    ++NextMI;
  if (NextMI == BB->end() || !NextMI->isReturn())
    return false;
  return true;
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMBytesToCellsPass() {
  return new SyncVMBytesToCells();
}
