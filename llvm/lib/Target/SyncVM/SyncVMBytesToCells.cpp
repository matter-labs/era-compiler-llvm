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

  void multiplyByCellSize(MachineInstr::mop_iterator Mop) const;
  void divideByCellSize(MachineInstr::mop_iterator Mop) const;

  void collectArgumentsAsRegisters(MachineFunction &MF);

  // Returns true if the instruction is a return with argument.
  bool isCopyReturnValue(MachineInstr &MI) const;
  bool isPassedInCells(Register) const;
  bool scalePointerArithmeticIfNeeded(MachineInstr *MI) const;

  bool mayContainCells(Register Reg) const;
  bool isUsedAsStackAddress(Register Reg) const;

  const TargetInstrInfo *TII;
  MachineRegisterInfo *MRI;

  std::vector<unsigned> MayContainCells;
  std::vector<unsigned> VRegsUsedInStackAddressing;
};

char SyncVMBytesToCells::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMBytesToCells, DEBUG_TYPE, SYNCVM_BYTES_TO_CELLS_NAME,
                false, false)

void SyncVMBytesToCells::multiplyByCellSize(MachineInstr::mop_iterator Mop) const {
  assert(Mop->isReg() && "Expected register operand");
  Register Reg = Mop->getReg();
  assert(Reg.isVirtual() && "Physical registers are not expected");

  MachineInstr *MI = Mop->getParent();
  assert(MI);

  auto NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
  BuildMI(*MI->getParent(), MI->getIterator(), MI->getDebugLoc(),
          TII->get(SyncVM::MULirrr_s))
      .addDef(NewVR)
      .addDef(SyncVM::R0)
      .addImm(CellSizeInBytes)
      .addReg(Reg)
      .addImm(SyncVMCC::COND_NONE);
  Mop->ChangeToRegister(NewVR, false);
}

void SyncVMBytesToCells::divideByCellSize(
    MachineInstr::mop_iterator Mop) const {
  assert(Mop->isReg() && "Expected register operand");
  Register Reg = Mop->getReg();
  assert(Reg.isVirtual() && "Physical registers are not expected");

  MachineInstr *MI = Mop->getParent();
  assert(MI);

  auto NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
  BuildMI(*MI->getParent(), MI->getIterator(), MI->getDebugLoc(),
          TII->get(SyncVM::DIVxrrr_s))
      .addDef(NewVR)
      .addDef(SyncVM::R0)
      .addImm(CellSizeInBytes)
      .addReg(Reg)
      .addImm(SyncVMCC::COND_NONE);
  Mop->ChangeToRegister(NewVR, false);
}

bool SyncVMBytesToCells::scalePointerArithmeticIfNeeded(MachineInstr *MI) const {
  if (!MI)
    return false;

  auto Opc = MI->getOpcode();
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

void SyncVMBytesToCells::collectArgumentsAsRegisters(MachineFunction &MF) {
  // identify registers that are used as stack pointers and is coming from
  // arguments. If so, need to mark them as do not scale.
  auto &BB = MF.front();
  // from top to bottom:
  for (auto II = BB.begin(); II != BB.end(); ++II) {
    MachineInstr &MI = *II;
    // does not appear after call. And a call in Entry will
    // be followed by a COPY from r1.
    if (MI.getOpcode() == SyncVM::CALL)
      break;
    if (MI.getOpcode() != SyncVM::COPY)
      continue;
    Register OutReg = SyncVM::out0Iterator(MI)->getReg();
    if (!OutReg.isVirtual())
      continue;
    MayContainCells.push_back(OutReg.virtRegIndex());
  }
}

bool SyncVMBytesToCells::mayContainCells(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  return llvm::find(MayContainCells, Reg.virtRegIndex()) !=
             MayContainCells.end();
}
bool SyncVMBytesToCells::isUsedAsStackAddress(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  return llvm::find(VRegsUsedInStackAddressing, Reg.virtRegIndex()) !=
         VRegsUsedInStackAddressing.end();
}
bool SyncVMBytesToCells::isPassedInCells(Register Reg) const {
  if (!Reg.isVirtual())
    return false;
  return mayContainCells(Reg) && isUsedAsStackAddress(Reg);
}

// Returns true if the instruction is a copy to a physical register from a
// virtual register.
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
  DenseMap<Register, Register> BytesToCellsRegs{};

  MayContainCells.clear();
  VRegsUsedInStackAddressing.clear();
  collectArgumentsAsRegisters(MF);

  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;
      // If a stack pointer is used as an argument to a call, its value must be
      // in cells not bytes.
      if (isCopyToPhyReg(MI)) {
        auto SrcReg = SyncVM::in0Iterator(MI)->getReg();
        auto DefInstr = MRI->getVRegDef(SrcReg);
        if (DefInstr->getOpcode() == SyncVM::ADDframe) {
          // convert from bytes to cells
          divideByCellSize(SyncVM::in0Iterator(MI));
        }
      }

      auto ConvertMI = [&](MachineInstr::mop_iterator OpIt) {
        MachineOperand &MO0Reg = *(OpIt + 1);
        MachineOperand &MO1Global = *(OpIt + 2);
        if (MO0Reg.isReg()) {
          Register Reg = MO0Reg.getReg();
          assert(Reg.isVirtual() && "Physical registers are not expected");
          MachineInstr *DefMI = MRI->getVRegDef(Reg);
          if (DefMI->getOpcode() == SyncVM::CTXr)
            // context.sp is already in cells.
            return;

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
              Changed = true;
              return;
            }
          }

          Register NewVR;
          if (BytesToCellsRegs.count(Reg) == 1) {
            // Already converted, use value from the cache.
            NewVR = BytesToCellsRegs[Reg];
            ++NumBytesToCells;
          } else {
            NewVR = MRI->createVirtualRegister(&SyncVM::GR256RegClass);
            MachineBasicBlock *DefBB = [DefMI, &MI]() {
              if (EarlyBytesToCells)
                return DefMI->getParent();
              else
                return MI.getParent();
            }();
            auto DefIt = [DefBB, DefMI, &MI]() {
              if (!EarlyBytesToCells)
                return find_if(*DefBB, [&MI](const MachineInstr &CurrentMI) {
                  return &MI == &CurrentMI;
                });
              if (!DefMI->isPHI())
                return std::next(
                    find_if(*DefBB, [DefMI](const MachineInstr &CurrentMI) {
                      return DefMI == &CurrentMI;
                    }));
              return DefBB->getFirstNonPHI();
            }();
            assert(DefIt->getParent() == DefBB);
            
            // if the Reg is coming from argument list, then it is already in cells.
            // so we will skip DIV insertion.
            if (mayContainCells(Reg)) {
              NewVR = Reg;
            } else {
              // check that if the base register is already in cells.
              // if so, we need not to insert DIV, but descale the offset.
              auto DefInstr = MRI->getVRegDef(Reg);
              // pointer arithmetic
              scalePointerArithmeticIfNeeded(DefInstr);

              // Insert DIV before the instruction.
              BuildMI(*DefBB, DefIt, MI.getDebugLoc(),
                      TII->get(SyncVM::DIVxrrr_s))
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
      };

      auto StackIt = SyncVM::getStackAccess(MI);
      if (!StackIt)
        continue;
      ConvertMI(StackIt);
      
      StackIt = SyncVM::getSecondStackAccess(MI);
      if (StackIt)
        ConvertMI(StackIt);

      Changed = true;
    }
    if (!EarlyBytesToCells)
      BytesToCellsRegs.clear();
  }

  // handling pointer arithmetic, returned stack pointer.  We have to do it
  // separately in a second pass is because it needs the information collected
  // by the first pass.
  for (auto &BB : MF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;
      if (MI.getOpcode() == SyncVM::ADDirr_s) {
        auto SPOpnd = SyncVM::in1Iterator(MI);
        if (isPassedInCells(SPOpnd->getReg())) {
          multiplyByCellSize(SPOpnd);
        }
      } else if (MI.getOpcode() == SyncVM::RET) {
        auto ReverseIt = std::next(MI.getReverseIterator());
        while (ReverseIt != BB.rend() && isCopyReturnValue(*ReverseIt)) {
          auto RegOpnd = SyncVM::in0Iterator(*ReverseIt);
          if (isPassedInCells(RegOpnd->getReg()) &&
              mayContainCells(RegOpnd->getReg()))
            multiplyByCellSize(RegOpnd);
          ++ReverseIt;
        }
      }
    }
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

// match instructions with format: $r1 = COPY %0
// which are used to copy return value from a virtual to a physical
bool SyncVMBytesToCells::isCopyReturnValue(MachineInstr &MI) const {
  if (!(MI.getOpcode() == SyncVM::COPY &&
        SyncVM::out0Iterator(MI)->getReg().isPhysical()))
    return false;
  Register PReg = SyncVM::out0Iterator(MI)->getReg();
  Register Reg = SyncVM::in0Iterator(MI)->getReg();
  if (!PReg.isPhysical() || !Reg.isVirtual())
    return false;

  // look for the copy instruction immediately before RET
  auto NextMI = std::next(MI.getIterator());
  auto *BB = MI.getParent();
  if (NextMI == BB->end() || NextMI->getOpcode() != SyncVM::RET)
    return false;

  return true;
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMBytesToCellsPass() {
  return new SyncVMBytesToCells();
}
