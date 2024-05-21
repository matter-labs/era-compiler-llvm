#ifndef LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H
#define LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H

#include "EVM.h"

#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class MachineInstr;
class MCSymbol;

class EVMAssembly {
private:
  using MIIter = MachineBasicBlock::iterator;

  MachineFunction *MF;
  const EVMInstrInfo *TII;

  int StackHeight = 0;
  MIIter CurMIIt;
  MachineBasicBlock *CurMBB;
  DenseSet<const MachineInstr *> AssemblyInstrs;

public:
  EVMAssembly(MachineFunction *MF, const EVMInstrInfo *TII)
      : MF(MF), TII(TII) {}

  // Retrieve the current height of the stack.
  // This does not have to be zero at the beginning.
  int getStackHeight() const;

  void setStackHeight(int Height);

  void setCurrentLocation(MachineBasicBlock *MBB);

  void appendInstruction(MachineInstr *MI);

  void appendSWAPInstruction(unsigned Depth);

  void appendDUPInstruction(unsigned Depth);

  void appendPOPInstruction();

  void appendConstant(const llvm::APInt &Val);

  void appendConstant(uint64_t Val);

  void appendLabel();

  void appendFuncCall(const MachineInstr *MI, const llvm::GlobalValue *Func,
                      int stackAdj, MCSymbol *RetSym = nullptr);

  void appendJump(int stackAdj);

  void appendLabelReference(MCSymbol *Label);

  // Create a function return symbol
  MCSymbol *createFuncRetSymbol();

  void removeUnusedInstrs();

  void dumpInst(const MachineInstr *MI) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMASSEMBLY_H
