//===-- SyncVMExpandPseudoInsts.cpp - Expand pseudo instructions ----------===//
//
/// \file
/// This file contains a pass that expands pseudo instructions into target
/// instructions. This pass should be run after register allocation but before
/// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-pseudo"
#define SYNCVM_EXPAND_PSEUDO_NAME "SyncVM expand pseudo instructions"

namespace {

class SyncVMExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  SyncVMExpandPseudo() : MachineFunctionPass(ID) {}

  const TargetRegisterInfo *TRI;

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_EXPAND_PSEUDO_NAME; }

private:
  void expandConst(MachineInstr &MI) const;
  void expandLoadConst(MachineInstr &MI) const;
  const TargetInstrInfo *TII;
  LLVMContext *Context;
};

char SyncVMExpandPseudo::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMExpandPseudo, DEBUG_TYPE, SYNCVM_EXPAND_PSEUDO_NAME,
                false, false)

void SyncVMExpandPseudo::expandConst(MachineInstr &MI) const {
  MachineOperand Constant = MI.getOperand(1);
  MachineOperand Reg = MI.getOperand(0);
  assert((Constant.isImm() || Constant.isCImm()) && "Unexpected operand type");
  const APInt &Val = Constant.isCImm() ? Constant.getCImm()->getValue()
                                       : APInt(256, Constant.getImm(), true);
  // big immediate or negative values are loaded from constant pool
  assert(Val.isIntN(16) && !Val.isNegative());
  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(SyncVM::ADDirr))
      .add(Reg)
      .addReg(SyncVM::R0)
      .addCImm(ConstantInt::get(*Context, Val));
}

void SyncVMExpandPseudo::expandLoadConst(MachineInstr &MI) const {
  MachineOperand ConstantPool = MI.getOperand(1);
  MachineOperand Reg = MI.getOperand(0);

  auto can_combine = [] (MachineInstr &cur, MachineInstr &next) {
    auto opcode = next.getOpcode(); 
    switch (opcode) {
      default:{
        break;
      }
      // this handles commutative cases
      case SyncVM::ADDrrr:
      case SyncVM::ANDrrr:
      case SyncVM::XORrrr:
      case SyncVM::ORrrr: {
        auto outReg = cur.getOperand(0).getReg();
        if (next.getOperand(1).getReg() == outReg ||
            next.getOperand(2).getReg() == outReg) {
          return true;
        }
        break;
      }
    }
    return false;
  };

  auto can_non_commute_combine = [](MachineInstr &cur, MachineInstr &next) {
    auto opcode = next.getOpcode(); 
    switch (opcode) {
      default:{
        break;
      }
      // this handles commutative cases
      case SyncVM::SUBrrr:
      case SyncVM::SHLrrr:
      case SyncVM::SHRrrr:
      case SyncVM::ROLrrr:
      case SyncVM::RORrrr: {
        auto outReg = cur.getOperand(0).getReg();
        if (next.getOperand(1).getReg() == outReg ||
            next.getOperand(2).getReg() == outReg) {
          return true;
        }
        break;
      }
    }
    return false;
  };

  auto get_crr_op = [] (auto opcode, bool reverse = false) {
    switch (opcode) {
      default: {
        llvm_unreachable("wrong opcode");
        break;
      }
      case SyncVM::ADDrrr: {
        return SyncVM::ADDcrr;
      }
      case SyncVM::ANDrrr: {
        return SyncVM::ANDcrr;
      }
      case SyncVM::XORrrr: {
        return SyncVM::XORcrr;
      }
      case SyncVM::ORrrr: {
        return SyncVM::ORcrr;
      }
      case SyncVM::SUBrrr: {
        return reverse ? SyncVM::SUByrr : SyncVM::SUBcrr;
      }
      case SyncVM::SHLrrr: {
        return reverse ? SyncVM::SHLyrr : SyncVM::SHLcrr;
      }
      case SyncVM::SHRrrr: {
        return reverse ? SyncVM::SHRyrr : SyncVM::SHRcrr;
      }
      case SyncVM::ROLrrr: {
        return reverse ? SyncVM::ROLyrr : SyncVM::ROLcrr;
      }
      case SyncVM::RORrrr: {
        return reverse ? SyncVM::RORyrr : SyncVM::RORrrr;
      }
    }
  };

  // it is possible that we can merge two instructions, as long as we do not
  // call a scheduler the materialization of a const will be followed by its
  // use.
  auto MBBI = std::next(MachineBasicBlock::iterator(MI));
  auto outReg = MI.getOperand(0).getReg();
  if (can_combine(MI, *MBBI)) {
    auto opcode = MBBI->getOpcode();
    auto other_op = get_crr_op(opcode);

    auto outReg = MI.getOperand(0).getReg();
    auto otherReg = MBBI->getOperand(1).getReg() == outReg
                        ? MBBI->getOperand(2)
                        : MBBI->getOperand(1);

    BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(other_op))
        .add(MBBI->getOperand(0))
        .addImm(0)
        .add(ConstantPool)
        .add(otherReg);
    MBBI->eraseFromParent();
    return;
  }

  if (can_non_commute_combine(MI, *MBBI)) {
    auto opcode = MBBI->getOpcode();

    bool reverse;
    MachineOperand * otherOpnd;
    if (MBBI->getOperand(1).getReg() == outReg) {
      reverse = false;
      otherOpnd = &MBBI->getOperand(2);
    } else {
      assert(MBBI->getOperand(2).getReg() == outReg);
      reverse = true;
      otherOpnd = &MBBI->getOperand(1);
    }
    auto other_op = get_crr_op(opcode, reverse);

    BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(other_op))
        .add(MBBI->getOperand(0))
        .addImm(0)
        .add(ConstantPool)
        .add(*otherOpnd);
    MBBI->eraseFromParent();
    return;
  }

  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(SyncVM::ADDcrr))
      .add(Reg)
      .addImm(0)
      .add(ConstantPool)
      .addReg(SyncVM::R0)
      .getInstr();
}


bool SyncVMExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');
  // pseudo opcode -> mov opcode, cmov opcode, #args src0, #args src1, #args dst
  DenseMap<unsigned, std::vector<unsigned>> Pseudos{
      {SyncVM::SELrrr, {SyncVM::CMOVrrr, SyncVM::CMOVrrr, 1, 1, 0}},
      {SyncVM::SELirr, {SyncVM::CMOVirr, SyncVM::CMOVrrr, 1, 1, 0}},
      {SyncVM::SELcrr, {SyncVM::CMOVcrr, SyncVM::CMOVrrr, 2, 1, 0}},
      {SyncVM::SELsrr, {SyncVM::CMOVsrr, SyncVM::CMOVrrr, 3, 1, 0}},
      {SyncVM::SELrir, {SyncVM::CMOVrrr, SyncVM::CMOVirr, 1, 1, 0}},
      {SyncVM::SELiir, {SyncVM::CMOVirr, SyncVM::CMOVirr, 1, 1, 0}},
      {SyncVM::SELcir, {SyncVM::CMOVcrr, SyncVM::CMOVirr, 2, 1, 0}},
      {SyncVM::SELsir, {SyncVM::CMOVsrr, SyncVM::CMOVirr, 3, 1, 0}},
      {SyncVM::SELrcr, {SyncVM::CMOVrrr, SyncVM::CMOVcrr, 1, 2, 0}},
      {SyncVM::SELicr, {SyncVM::CMOVirr, SyncVM::CMOVcrr, 1, 2, 0}},
      {SyncVM::SELccr, {SyncVM::CMOVcrr, SyncVM::CMOVcrr, 2, 2, 0}},
      {SyncVM::SELscr, {SyncVM::CMOVsrr, SyncVM::CMOVcrr, 3, 2, 0}},
      {SyncVM::SELrsr, {SyncVM::CMOVrrr, SyncVM::CMOVsrr, 1, 3, 0}},
      {SyncVM::SELisr, {SyncVM::CMOVirr, SyncVM::CMOVsrr, 1, 3, 0}},
      {SyncVM::SELcsr, {SyncVM::CMOVcrr, SyncVM::CMOVsrr, 2, 3, 0}},
      {SyncVM::SELssr, {SyncVM::CMOVsrr, SyncVM::CMOVsrr, 3, 3, 0}},

      {SyncVM::SELrrs, {SyncVM::CMOVrrs, SyncVM::CMOVrrs, 1, 1, 3}},
      {SyncVM::SELirs, {SyncVM::CMOVirs, SyncVM::CMOVrrs, 1, 1, 3}},
      {SyncVM::SELcrs, {SyncVM::CMOVcrs, SyncVM::CMOVrrs, 2, 1, 3}},
      {SyncVM::SELsrs, {SyncVM::CMOVsrs, SyncVM::CMOVrrs, 3, 1, 3}},
      {SyncVM::SELris, {SyncVM::CMOVrrs, SyncVM::CMOVirs, 1, 1, 3}},
      {SyncVM::SELiis, {SyncVM::CMOVirs, SyncVM::CMOVirs, 1, 1, 3}},
      {SyncVM::SELcis, {SyncVM::CMOVcrs, SyncVM::CMOVirs, 2, 1, 3}},
      {SyncVM::SELsis, {SyncVM::CMOVsrs, SyncVM::CMOVirs, 3, 1, 3}},
      {SyncVM::SELrcs, {SyncVM::CMOVrrs, SyncVM::CMOVcrs, 1, 2, 3}},
      {SyncVM::SELics, {SyncVM::CMOVirs, SyncVM::CMOVcrs, 1, 2, 3}},
      {SyncVM::SELccs, {SyncVM::CMOVcrs, SyncVM::CMOVcrs, 2, 2, 3}},
      {SyncVM::SELscs, {SyncVM::CMOVsrs, SyncVM::CMOVcrs, 3, 2, 3}},
      {SyncVM::SELrss, {SyncVM::CMOVrrs, SyncVM::CMOVsrs, 1, 3, 3}},
      {SyncVM::SELiss, {SyncVM::CMOVirs, SyncVM::CMOVsrs, 1, 3, 3}},
      {SyncVM::SELcss, {SyncVM::CMOVcrs, SyncVM::CMOVsrs, 2, 3, 3}},
      {SyncVM::SELsss, {SyncVM::CMOVsrs, SyncVM::CMOVsrs, 3, 3, 3}},
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
        unsigned NumDefs    = MI.getNumDefs();
        unsigned MovOpc     = Pseudos[Opc][1];
        unsigned CMovOpc    = Pseudos[Opc][0];
        unsigned Src0ArgPos = NumDefs;
        unsigned Src1ArgPos = NumDefs + Pseudos[Opc][2];
        unsigned CCPos      = Src1ArgPos + Pseudos[Opc][3];
        unsigned DstArgPos  = CCPos + 1;
        unsigned EndPos     = DstArgPos + Pseudos[Opc][4];

        // Avoid mov rN, rN
        if (NumDefs != 1 || CCPos - Src1ArgPos != 1
            || !MI.getOperand(Src1ArgPos).isReg()
            || MI.getOperand(0).getReg() != MI.getOperand(Src1ArgPos).getReg()) {
          // unconditional mov
          auto Mov = [&]() {
            if (NumDefs)
              return BuildMI(MBB, &MI, DL, TII->get(MovOpc),
                             MI.getOperand(0).getReg());
            return BuildMI(MBB, &MI, DL, TII->get(MovOpc));
          }();
          // SEL src1 -> MOV src0
          for (unsigned i = Src1ArgPos; i < CCPos; ++i)
            Mov.add(MI.getOperand(i));
          // r0 -> MOV src1
          Mov.addReg(SyncVM::R0);
          // COND_NONE -> MOV cc
          Mov.addImm(SyncVMCC::COND_NONE);
          // SEL dst -> MOV dst
          for (unsigned i = DstArgPos; i < EndPos; ++i)
            Mov.add(MI.getOperand(i));
        }

        // condtional mov
        auto CMov = [&]() {
          if (NumDefs)
            return BuildMI(MBB, &MI, DL, TII->get(CMovOpc),
                           MI.getOperand(0).getReg());
          return BuildMI(MBB, &MI, DL, TII->get(CMovOpc));
        }();
        // SEL src0 -> CMOV src0
        for (unsigned i = Src0ArgPos; i < Src1ArgPos; ++i)
          CMov.add(MI.getOperand(i));
        // r0 -> CMOV src1
        CMov.addReg(SyncVM::R0);
        // SEL cc -> CMOV cc
        CMov.add(MI.getOperand(CCPos));
        // SEL dst -> CMOV dst
        for (unsigned i = DstArgPos; i < EndPos; ++i)
          CMov.add(MI.getOperand(i));

        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == SyncVM::CONST) {
        expandConst(MI);
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == SyncVM::LOADCONST) {
        expandLoadConst(MI);
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
FunctionPass *llvm::createSyncVMExpandPseudoPass() {
  return new SyncVMExpandPseudo();
}
