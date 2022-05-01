//===-- SyncVMBytesToCells.cpp - Replace bytes addresses with cells ones --===//
//
/// \file
/// This file contains a pass that replaces addresses in bytes with addresses
/// in cells for instructions addressing stack.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-bytes-to-cells"
#define SYNCVM_BYTES_TO_CELLS_NAME "SyncVM bytes to cells"

namespace {

class SyncVMBytesToCells : public MachineFunctionPass {
public:
  static char ID;
  SyncVMBytesToCells() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_BYTES_TO_CELLS_NAME; }

private:
  bool isStackOp(const MachineInstr &MI, unsigned OpNum);
  unsigned opStart(const MachineInstr &MI, unsigned OpNum);
  bool mayHaveStackOperands(const MachineInstr &MI);
  void expandConst(MachineInstr &MI) const;
  void expandLoadConst(MachineInstr &MI) const;
  void expandThrow(MachineInstr &MI) const;
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMBytesToCells::ID = 0;

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

bool SyncVMBytesToCells::mayHaveStackOperands(const MachineInstr &MI) {
  StringRef InstName = TII->getName(MI.getOpcode());
  auto Pos = llvm::find_if(InstName, islower);
  std::string InstRoot(InstName.begin(), Pos);
  return llvm::find(BinaryIO, InstRoot) != BinaryIO.end() ||
         llvm::find(BinaryI, InstRoot) != BinaryI.end() || InstRoot == "SEL";
}

bool SyncVMBytesToCells::isStackOp(const MachineInstr &MI, unsigned OpNum) {
  StringRef InstName = TII->getName(MI.getOpcode());
  auto Pos = llvm::find_if(InstName, islower);
  return *(Pos + OpNum) == Stack || *(Pos + OpNum) == StackR;
}

unsigned SyncVMBytesToCells::opStart(const MachineInstr &MI, unsigned OpNum) {
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

INITIALIZE_PASS(SyncVMBytesToCells, DEBUG_TYPE, SYNCVM_BYTES_TO_CELLS_NAME,
                false, false)

bool SyncVMBytesToCells::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  bool Changed = false;

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  auto &MRI = MF.getRegInfo();
  assert(TII && "TargetInstrInfo must be a valid object");
  for (auto &BB : MF)
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MachineInstr &MI = *II;
      if (!mayHaveStackOperands(MI))
        continue;

      auto ConvertMI = [&](unsigned OpNum) {
        unsigned Op0Start = opStart(MI, OpNum);
        MachineOperand &MO0Reg = MI.getOperand(Op0Start + 1);
        if (MO0Reg.isReg()) {
          Register NewVR = MRI.createVirtualRegister(&SyncVM::GR256RegClass);
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                  TII->get(SyncVM::DIVxrrr_s))
              .addDef(NewVR)
              .addDef(SyncVM::R0)
              .addImm(32)
              .add(MO0Reg)
              .addImm(SyncVMCC::COND_NONE);
          MO0Reg.ChangeToRegister(NewVR, false);
        }
        MachineOperand &Const = MI.getOperand(Op0Start + 2);
        Const.ChangeToImmediate(Const.getImm() / 32);
      };

      auto isFrameIndexAddressing = [&](MachineInstr& MI, unsigned OpNum) {
        MachineOperand MO = MI.getOperand(MI.getNumOperands() - 3);
        if (MO.isReg()) {
          // specifically match frame indexing addressing mode
          // TODO: we should revamp the stack addressing in the backend, it is not
          // pretty as of now.
          Register reg = MO.getReg();
          assert(reg.isVirtual ());
          assert(RegInfo.isSSA());
          MachineInstr* defMI = RegInfo.getVRegDef(reg);
          if (defMI->getOpcode() == SyncVM::CTXr) {
            return true;
          }
        }

        return false;
      };

      for (unsigned OpNo = 0; OpNo < 3; ++OpNo)
        if (isStackOp(MI, OpNo)) {
          // avoid handling frame index addressing
          if (!isFrameIndexAddressing(MI, OpNo)) {
            ConvertMI(OpNo);
            Changed = true;
          }
        }
    }
  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMBytesToCellsPass() {
  return new SyncVMBytesToCells();
}
