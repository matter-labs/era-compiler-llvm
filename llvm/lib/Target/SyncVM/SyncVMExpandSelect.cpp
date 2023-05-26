//===-- SyncVMExpandSelect.cpp - Expand select pseudo instruction ---------===//
//
/// \file
/// This file contains a pass that expands SEL pseudo instructions into target
/// instructions.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-expand-select"
#define SYNCVM_EXPAND_SELECT_NAME "SyncVM expand select pseudo instructions"

namespace {

/// Lower SEL instruction family to uncoditional + conditional move.
/// Select x, y, cc -> add x, r0 + add.cc y, r0.
class SyncVMExpandSelect : public MachineFunctionPass {
public:
  static char ID;
  SyncVMExpandSelect() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &Fn) override;
  StringRef getPassName() const override { return SYNCVM_EXPAND_SELECT_NAME; }

private:
  const SyncVMInstrInfo *TII;
};

char SyncVMExpandSelect::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMExpandSelect, DEBUG_TYPE, SYNCVM_EXPAND_SELECT_NAME,
                false, false)

/// For given \p Select and argument \p Kind return corresponding mov opcode
/// for conditional or unconditional mov.
static unsigned movOpcode(SyncVM::ArgumentKind Kind, unsigned Select) {
  switch (SyncVM::argumentType(Kind, Select)) {
  case SyncVM::ArgumentType::Register:
    if (Select == SyncVM::FATPTR_SELrrr)
      return SyncVM::PTR_ADDrrr_s;
    return SyncVM::ADDrrr_s;
  case SyncVM::ArgumentType::Immediate:
    return SyncVM::ADDirr_s;
  case SyncVM::ArgumentType::Code:
    return SyncVM::ADDcrr_s;
  case SyncVM::ArgumentType::Stack:
    return SyncVM::ADDsrr_s;
  default:
    break;
  }
  llvm_unreachable("Unexpected argument type");
}

bool SyncVMExpandSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  DenseMap<unsigned, unsigned> Inverse{
      {SyncVM::SELrrr, SyncVM::SELrrr},
      {SyncVM::SELrir, SyncVM::SELirr},
      {SyncVM::SELrcr, SyncVM::SELcrr},
      {SyncVM::SELrsr, SyncVM::SELsrr},
  };

  DenseMap<unsigned, unsigned> InverseCond{
      {SyncVMCC::COND_E, SyncVMCC::COND_NE},
      {SyncVMCC::COND_NE, SyncVMCC::COND_E},
      {SyncVMCC::COND_LT, SyncVMCC::COND_GE},
      {SyncVMCC::COND_LE, SyncVMCC::COND_GT},
      {SyncVMCC::COND_GT, SyncVMCC::COND_LE},
      {SyncVMCC::COND_GE, SyncVMCC::COND_LT},
      // COND_OF is an alias for COND_LT
      {SyncVMCC::COND_OF, SyncVMCC::COND_GE},
  };

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!SyncVM::isSelect(MI))
        continue;

      unsigned Opc = MI.getOpcode();
      DebugLoc DL = MI.getDebugLoc();
      auto In0 = SyncVM::in0Iterator(MI);
      auto In0Range = SyncVM::in0Range(MI);
      auto In1Range = SyncVM::in1Range(MI);
      auto Out = SyncVM::out0Iterator(MI);
      auto CCVal = getImmOrCImm(*SyncVM::ccIterator(MI));

      // For rN = cc ? rN : y it's profitable to reverse (rN = reverse_cc ? y :
      // rN) It allows to lower select to a single instruction rN =
      // add.reverse_cc y, r0.
      bool ShouldInverse =
          Inverse.count(Opc) != 0u && Out->getReg() == In0->getReg();

      auto buildMOV = [&](SyncVM::ArgumentKind OpNo, unsigned CC) {
        auto OperandRange =
            (OpNo == SyncVM::ArgumentKind::In0) ? In0Range : In1Range;
        auto OperandIt = OperandRange.begin();
        bool IsRegister =
            argumentType(OpNo, MI) == SyncVM::ArgumentType::Register;
        unsigned MovOpc = movOpcode(OpNo, Opc);
        // Avoid unconditional mov rN, rN
        if (CC == SyncVMCC::COND_NONE && IsRegister &&
            OperandIt->getReg() == Out->getReg())
          return;
        auto Mov = BuildMI(MBB, &MI, DL, TII->get(MovOpc), Out->getReg());
        SyncVM::copyOperands(Mov, OperandRange);
        Mov.addReg(SyncVM::R0);
        Mov.addImm(CC);
        if (CC != SyncVMCC::COND_NONE)
          Mov.addReg(SyncVM::Flags, RegState::Implicit);
        return;
      };

      if (ShouldInverse) {
        buildMOV(SyncVM::ArgumentKind::In0, SyncVMCC::COND_NONE);
        buildMOV(SyncVM::ArgumentKind::In1, InverseCond[CCVal]);
      } else {
        buildMOV(SyncVM::ArgumentKind::In1, SyncVMCC::COND_NONE);
        buildMOV(SyncVM::ArgumentKind::In0, CCVal);
      }

      PseudoInst.push_back(&MI);
    }

  for (auto *I : PseudoInst)
    I->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return !PseudoInst.empty();
}

/// createSyncVMExpandPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createSyncVMExpandSelectPass() {
  return new SyncVMExpandSelect();
}
