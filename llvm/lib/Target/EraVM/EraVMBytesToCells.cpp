//===-- EraVMBytesToCells.cpp - EraVM Bytes To Cells conversion -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// LLVM emits byte addressing for stack accesses, and on EraVM the stack
// space is addressed in cells (32 bytes per cell). To make things more
// complicated, some instruction generated stack pointers are already
// cell-addressed, for example: `context.sp` returns stack pointer in cells.
//
// To handle such inconsistencies, This pass ensures that after the pass, all
// bytes addressings of stack are converted to cells addressings. This is
// necessary for EraVM target to ensure stack accesses.
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

static cl::opt<bool>
    EarlyBytesToCells("early-bytes-to-cells-conversion", cl::init(false),
                      cl::Hidden,
                      cl::desc("Converts bytes to cells after the definition"));

STATISTIC(NumBytesToCells, "Number of bytes to cells convertions gone");

namespace {

class EraVMBytesToCells : public MachineFunctionPass {
public:
  static char ID;
  EraVMBytesToCells() : MachineFunctionPass(ID) {
    initializeEraVMBytesToCellsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_BYTES_TO_CELLS_NAME; }

private:
  bool isStackOp(const MachineInstr &MI, unsigned OpNum);
  unsigned opStart(const MachineInstr &MI, unsigned OpNum);
  bool mayHaveStackOperands(const MachineInstr &MI);
  void expandConst(MachineInstr &MI) const;
  void expandLoadConst(MachineInstr &MI) const;
  void expandThrow(MachineInstr &MI) const;

  const EraVMInstrInfo *TII{};
  LLVMContext *Context{};
};

char EraVMBytesToCells::ID = 0;

} // namespace

static const char Reg = 'r';
static const char Stack = 's';
static const char StackR = 'z';
static const char Code = 'c';
static const char CodeR = 'y';
static const char Immediate = 'i';
static const char ImmediateR = 'x';
static const std::vector<std::string> BinaryIO = {"MUL", "DIV"};
static const std::vector<std::string> BinaryI = {
    "ADD", "SUB", "AND", "OR", "XOR", "SHL", "SHR", "ROL", "ROR"};

bool EraVMBytesToCells::mayHaveStackOperands(const MachineInstr &MI) {
  StringRef InstName = TII->getName(MI.getOpcode());
  auto Pos = llvm::find_if(InstName, islower);
  std::string InstRoot(InstName.begin(), Pos);
  return llvm::find(BinaryIO, InstRoot) != BinaryIO.end() ||
         llvm::find(BinaryI, InstRoot) != BinaryI.end() || InstRoot == "SEL";
}

bool EraVMBytesToCells::isStackOp(const MachineInstr &MI, unsigned OpNum) {
  StringRef InstName = TII->getName(MI.getOpcode());
  auto Pos = llvm::find_if(InstName, islower);
  return *(Pos + OpNum) == Stack || *(Pos + OpNum) == StackR;
}

unsigned EraVMBytesToCells::opStart(const MachineInstr &MI, unsigned OpNum) {
  StringRef InstName = TII->getName(MI.getOpcode());
  auto Pos = llvm::find_if(InstName, islower);
  std::string InstRoot(InstName.begin(), Pos);
  if (OpNum == 2 && *(Pos + OpNum) == Reg)
    return 0;
  unsigned Result = 0;
  for (unsigned I = 0; I < OpNum; ++I) {
    char Opnd = *(Pos + I);
    Result += (Opnd == Reg || Opnd == Immediate || Opnd == ImmediateR) ? 1
              : (Opnd == Code || Opnd == CodeR)                        ? 2
                                                                       : 3;
  }
  if (OpNum < 2 && *(Pos + 2) == Reg)
    ++Result;
  if (OpNum < 3 && llvm::find(BinaryIO, InstRoot) != BinaryIO.end())
    ++Result;
  if (OpNum == 2 && InstRoot == "SEL")
    ++Result;
  return Result;
}

INITIALIZE_PASS(EraVMBytesToCells, DEBUG_TYPE, ERAVM_BYTES_TO_CELLS_NAME, false,
                false)

bool EraVMBytesToCells::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  assert(RegInfo.isSSA() && "The pass is supposed to be run on SSA form MIR");

  bool Changed = false;
  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  auto &MRI = MF.getRegInfo();
  assert(TII && "TargetInstrInfo must be a valid object");
  DenseMap<Register, Register> BytesToCellsRegs{};

  for (auto &BB : MF) {
    for (auto &MI : BB) {
      if (!mayHaveStackOperands(MI))
        continue;
      auto ConvertMI = [&](unsigned OpNum) {
        unsigned Op0Start = opStart(MI, OpNum);
        MachineOperand &MO0Reg = MI.getOperand(Op0Start + 1);
        if (MO0Reg.isReg()) {
          Register Reg = MO0Reg.getReg();
          assert(Reg.isVirtual() && "Physical registers are not expected");
          MachineInstr *DefMI = RegInfo.getVRegDef(Reg);
          if (DefMI->getOpcode() == EraVM::CTXr)
            // context.sp is already in cells.
            return;
          Register NewVR;
          if (BytesToCellsRegs.count(Reg) == 1) {
            // Already converted, use value from the cache.
            NewVR = BytesToCellsRegs[Reg];
            ++NumBytesToCells;
          } else {
            NewVR = MRI.createVirtualRegister(&EraVM::GR256RegClass);
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
            BuildMI(*DefBB, DefIt, MI.getDebugLoc(), TII->get(EraVM::DIVxrrr_s))
                .addDef(NewVR)
                .addDef(EraVM::R0)
                .addImm(32)
                .addReg(Reg)
                .addImm(EraVMCC::COND_NONE);
            BytesToCellsRegs[Reg] = NewVR;
          }
          MO0Reg.ChangeToRegister(NewVR, false);
        }
        MachineOperand &Const = MI.getOperand(Op0Start + 2);
        if (Const.isImm() || Const.isCImm())
          Const.ChangeToImmediate(getImmOrCImm(Const) / 32);
      };
      for (unsigned OpNo = 0; OpNo < 3; ++OpNo)
        if (isStackOp(MI, OpNo)) {
          // avoid handling frame index addressing
          ConvertMI(OpNo);
          Changed = true;
        }
    }
    if (!EarlyBytesToCells)
      BytesToCellsRegs.clear();
  }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

/// createEraVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createEraVMBytesToCellsPass() {
  return new EraVMBytesToCells();
}
