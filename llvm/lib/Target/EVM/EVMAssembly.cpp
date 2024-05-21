#include "EVM.h"

#include "EVMAssembly.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "evm-assembly"

void EVMAssembly::dumpInst(const MachineInstr *MI) const {
  LLVM_DEBUG(dbgs() << "Adding: " << *MI << "stack height: " << StackHeight
                    << "\n");
}

int EVMAssembly::getStackHeight() const { return StackHeight; }

void EVMAssembly::setStackHeight(int Height) {
  StackHeight = Height;
  LLVM_DEBUG(dbgs() << "Set stack height: " << StackHeight << "\n");
}

void EVMAssembly::setCurrentLocation(MachineBasicBlock *MBB) {
  CurMBB = MBB;
  CurMIIt = MBB->begin();
}

void EVMAssembly::appendInstruction(MachineInstr *MI) {
  unsigned Opc = MI->getOpcode();
  assert(Opc != EVM::JUMP && Opc != EVM::JUMPI && Opc != EVM::ARGUMENT &&
         Opc != EVM::RET && Opc != EVM::CONST_I256 && Opc != EVM::COPY_I256 &&
         Opc != EVM::FCALL);

  auto Ret = AssemblyInstrs.insert(MI);
  assert(Ret.second);
  int StackAdj = (2 * static_cast<int>(MI->getNumExplicitDefs())) -
                 static_cast<int>(MI->getNumExplicitOperands());
  StackHeight += StackAdj;
  dumpInst(MI);
  CurMIIt = MIIter(MI);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendSWAPInstruction(unsigned Depth) {
  unsigned Opc = EVM::getSWAPOpcode(Depth);
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opc));
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendDUPInstruction(unsigned Depth) {
  unsigned Opc = EVM::getDUPOpcode(Depth);
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opc));
  StackHeight += 1;
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendPOPInstruction() {
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::POP));
  assert(StackHeight > 0);
  StackHeight -= 1;
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendConstant(const APInt &Val) {
  unsigned Opc = EVM::getPUSHOpcode(Val);
  MachineInstrBuilder Builder =
      BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(Opc));
  if (Opc != EVM::PUSH0) {
    LLVMContext &Ctx = MF->getFunction().getContext();
    Builder.addCImm(ConstantInt::get(Ctx, Val));
  }
  StackHeight += 1;
  CurMIIt = Builder;
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendConstant(uint64_t Val) {
  appendConstant(APInt(256, Val));
}

void EVMAssembly::appendLabel() {
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::JUMPDEST));
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendLabelReference(MCSymbol *Label) {
  // Create push of the return address.
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::PUSH8_LABEL))
                .addSym(Label);
  StackHeight += 1;
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

MCSymbol *EVMAssembly::createFuncRetSymbol() {
  return MF->getContext().createTempSymbol("FUNC_RET", true);
}

void EVMAssembly::appendFuncCall(const MachineInstr *MI,
                                 const llvm::GlobalValue *Func, int stackAdj,
                                 MCSymbol *RetSym) {
  // Push the function label
  assert(CurMBB == MI->getParent());
  // Create jump to the callee. Note we don't add the 'target' operand to JUMP.
  CurMIIt =
      BuildMI(*CurMBB, CurMIIt, MI->getDebugLoc(), TII->get(EVM::PUSH8_LABEL))
          .addGlobalAddress(Func);
  // PUSH8_LABEL technically increases the stack height on 1, but we don't
  // increase it explicitly here, as the label will be consumed by the following
  // JUMP.
  AssemblyInstrs.insert(&*CurMIIt);
  StackHeight += stackAdj;
  dumpInst(&*CurMIIt);

  CurMIIt = std::next(CurMIIt);
  CurMIIt = BuildMI(*CurMBB, CurMIIt, MI->getDebugLoc(), TII->get(EVM::JUMP));
  if (RetSym)
    CurMIIt->setPostInstrSymbol(*MF, RetSym);
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::appendJump(int stackAdj) {
  CurMIIt = BuildMI(*CurMBB, CurMIIt, DebugLoc(), TII->get(EVM::JUMP));
  StackHeight += stackAdj;
  AssemblyInstrs.insert(&*CurMIIt);
  dumpInst(&*CurMIIt);
  CurMIIt = std::next(CurMIIt);
}

void EVMAssembly::removeUnusedInstrs() {
  SmallVector<MachineInstr *, 128> ToRemove;
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (!AssemblyInstrs.count(&MI) && MI.getOpcode() != EVM::JUMP &&
          MI.getOpcode() != EVM::JUMPI)
        ToRemove.emplace_back(&MI);
    }
  }

  for (MachineInstr *MI : ToRemove)
    MI->eraseFromParent();
}
