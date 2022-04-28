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

class SyncVMExpandSelect : public MachineFunctionPass {
public:
  static char ID;
  SyncVMExpandSelect() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_EXPAND_SELECT_NAME; }

private:
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMExpandSelect::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMExpandSelect, DEBUG_TYPE, SYNCVM_EXPAND_SELECT_NAME,
                false, false)

bool SyncVMExpandSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');
  // pseudo opcode -> mov opcode, cmov opcode, #args src0, #args src1, #args dst
  DenseMap<unsigned, std::vector<unsigned>> Pseudos{
      {SyncVM::SELrrr, {SyncVM::ADDrrr_s, SyncVM::ADDrrr_s, 1, 1, 0}},
      {SyncVM::SELirr, {SyncVM::ADDirr_s, SyncVM::ADDrrr_s, 1, 1, 0}},
      {SyncVM::SELcrr, {SyncVM::ADDcrr_s, SyncVM::ADDrrr_s, 2, 1, 0}},
      {SyncVM::SELsrr, {SyncVM::ADDsrr_s, SyncVM::ADDrrr_s, 3, 1, 0}},
      {SyncVM::SELrir, {SyncVM::ADDrrr_s, SyncVM::ADDirr_s, 1, 1, 0}},
      {SyncVM::SELiir, {SyncVM::ADDirr_s, SyncVM::ADDirr_s, 1, 1, 0}},
      {SyncVM::SELcir, {SyncVM::ADDcrr_s, SyncVM::ADDirr_s, 2, 1, 0}},
      {SyncVM::SELsir, {SyncVM::ADDsrr_s, SyncVM::ADDirr_s, 3, 1, 0}},
      {SyncVM::SELrcr, {SyncVM::ADDrrr_s, SyncVM::ADDcrr_s, 1, 2, 0}},
      {SyncVM::SELicr, {SyncVM::ADDirr_s, SyncVM::ADDcrr_s, 1, 2, 0}},
      {SyncVM::SELccr, {SyncVM::ADDcrr_s, SyncVM::ADDcrr_s, 2, 2, 0}},
      {SyncVM::SELscr, {SyncVM::ADDsrr_s, SyncVM::ADDcrr_s, 3, 2, 0}},
      {SyncVM::SELrsr, {SyncVM::ADDrrr_s, SyncVM::ADDsrr_s, 1, 3, 0}},
      {SyncVM::SELisr, {SyncVM::ADDirr_s, SyncVM::ADDsrr_s, 1, 3, 0}},
      {SyncVM::SELcsr, {SyncVM::ADDcrr_s, SyncVM::ADDsrr_s, 2, 3, 0}},
      {SyncVM::SELssr, {SyncVM::ADDsrr_s, SyncVM::ADDsrr_s, 3, 3, 0}},

      /*
      {SyncVM::SELrrs, {SyncVM::ADDrrs_s, SyncVM::ADDrrs_s, 1, 1, 3}},
      {SyncVM::SELirs, {SyncVM::ADDirs_s, SyncVM::ADDrrs_s, 1, 1, 3}},
      {SyncVM::SELcrs, {SyncVM::ADDcrs_s, SyncVM::ADDrrs_s, 2, 1, 3}},
      {SyncVM::SELsrs, {SyncVM::ADDsrs_s, SyncVM::ADDrrs_s, 3, 1, 3}},
      {SyncVM::SELris, {SyncVM::ADDrrs_s, SyncVM::ADDirs_s, 1, 1, 3}},
      {SyncVM::SELiis, {SyncVM::ADDirs_s, SyncVM::ADDirs_s, 1, 1, 3}},
      {SyncVM::SELcis, {SyncVM::ADDcrs_s, SyncVM::ADDirs_s, 2, 1, 3}},
      {SyncVM::SELsis, {SyncVM::ADDsrs_s, SyncVM::ADDirs_s, 3, 1, 3}},
      {SyncVM::SELrcs, {SyncVM::ADDrrs_s, SyncVM::ADDcrs_s, 1, 2, 3}},
      {SyncVM::SELics, {SyncVM::ADDirs_s, SyncVM::ADDcrs_s, 1, 2, 3}},
      {SyncVM::SELccs, {SyncVM::ADDcrs_s, SyncVM::ADDcrs_s, 2, 2, 3}},
      {SyncVM::SELscs, {SyncVM::ADDsrs_s, SyncVM::ADDcrs_s, 3, 2, 3}},
      {SyncVM::SELrss, {SyncVM::ADDrrs_s, SyncVM::ADDsrs_s, 1, 3, 3}},
      {SyncVM::SELiss, {SyncVM::ADDirs_s, SyncVM::ADDsrs_s, 1, 3, 3}},
      {SyncVM::SELcss, {SyncVM::ADDcrs_s, SyncVM::ADDsrs_s, 2, 3, 3}},
      {SyncVM::SELsss, {SyncVM::ADDsrs_s, SyncVM::ADDsrs_s, 3, 3, 3}},
      */
  };

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
  };

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!MI.isPseudo())
        continue;

      if (Pseudos.count(MI.getOpcode())) {
        // Expand SELxxx pseudo into mov + cmov
        unsigned Opc = MI.getOpcode();
        DebugLoc DL = MI.getDebugLoc();

        assert(Pseudos.count(Opc) && "Unexpected instr type to insert");
        unsigned NumDefs = MI.getNumDefs();
        unsigned Src0ArgPos = NumDefs;
        unsigned Src1ArgPos = NumDefs + Pseudos[Opc][2];
        unsigned CCPos = Src1ArgPos + Pseudos[Opc][3];
        unsigned DstArgPos = CCPos + 1;
        unsigned EndPos = DstArgPos + Pseudos[Opc][4];

        bool ShouldInverse =
            Inverse.count(Opc) != 0u &&
            MI.getOperand(0).getReg() == MI.getOperand(Src0ArgPos).getReg();

        auto buildMOV = [&](unsigned OpNo, unsigned CC) {
          unsigned ArgPos = (OpNo == 0) ? Src0ArgPos : Src1ArgPos;
          unsigned NextPos = (OpNo == 0) ? Src1ArgPos : CCPos;
          bool IsReg = (NextPos - ArgPos == 1) && MI.getOperand(ArgPos).isReg();
          unsigned MovOpc = Pseudos[Opc][OpNo];
          // Avoid unconditional mov rN, rN
          if (CC == SyncVMCC::COND_NONE && NumDefs && IsReg &&
              MI.getOperand(ArgPos).getReg() == MI.getOperand(0).getReg())
            return;
          auto Mov = [&]() {
            if (NumDefs)
              return BuildMI(MBB, &MI, DL, TII->get(MovOpc),
                             MI.getOperand(0).getReg());
            return BuildMI(MBB, &MI, DL, TII->get(MovOpc));
          }();
          for (unsigned i = ArgPos; i < NextPos; ++i)
            Mov.add(MI.getOperand(i));
          Mov.addReg(SyncVM::R0);
          for (unsigned i = DstArgPos; i < EndPos; ++i) {
            Mov.add(MI.getOperand(i));
          }
          Mov.addImm(CC);
          return;
        };

        if (ShouldInverse) {
          buildMOV(0, SyncVMCC::COND_NONE);
          buildMOV(1, InverseCond[getImmOrCImm(MI.getOperand(CCPos))]);
        } else {
          buildMOV(1, SyncVMCC::COND_NONE);
          buildMOV(0, getImmOrCImm(MI.getOperand(CCPos)));
        }

        PseudoInst.push_back(&MI);
      }
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
