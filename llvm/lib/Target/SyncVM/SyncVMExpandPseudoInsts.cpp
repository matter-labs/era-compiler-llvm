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
  void expandThrow(MachineInstr& MI) const;
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
  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(SyncVM::ADDirr_s))
      .add(Reg)
      .addReg(SyncVM::R0)
      .addCImm(ConstantInt::get(*Context, Val))
      .addImm(0);
}

void SyncVMExpandPseudo::expandLoadConst(MachineInstr &MI) const {
  MachineOperand ConstantPool = MI.getOperand(1);
  MachineOperand Reg = MI.getOperand(0);

  auto can_combine = [](MachineInstr &cur, MachineInstr &next) {
    auto opcode = next.getOpcode();
    switch (opcode) {
    default: {
      break;
    }
    // this handles commutative cases
    case SyncVM::ADDrrr_s:
    case SyncVM::ANDrrr_s:
    case SyncVM::XORrrr_s:
    case SyncVM::ORrrr_s: {
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
    default: {
      break;
    }
    // this handles commutative cases
    case SyncVM::SUBrrr_s:
    case SyncVM::SHLrrr_s:
    case SyncVM::SHRrrr_s:
    case SyncVM::ROLrrr_s:
    case SyncVM::RORrrr_s: {
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

  auto get_crr_op = [](auto opcode, bool reverse = false) {
    switch (opcode) {
    default: {
      llvm_unreachable("wrong opcode");
      break;
    }
    case SyncVM::ADDrrr_s: {
      return SyncVM::ADDcrr_s;
    }
    case SyncVM::ANDrrr_s: {
      return SyncVM::ANDcrr_s;
    }
    case SyncVM::XORrrr_s: {
      return SyncVM::XORcrr_s;
    }
    case SyncVM::ORrrr_s: {
      return SyncVM::ORcrr_s;
    }
    case SyncVM::SUBrrr_s: {
      return reverse ? SyncVM::SUByrr_s : SyncVM::SUBcrr_s;
    }
    case SyncVM::SHLrrr_s: {
      return reverse ? SyncVM::SHLyrr_s : SyncVM::SHLcrr_s;
    }
    case SyncVM::SHRrrr_s: {
      return reverse ? SyncVM::SHRyrr_s : SyncVM::SHRcrr_s;
    }
    case SyncVM::ROLrrr_s: {
      return reverse ? SyncVM::ROLyrr_s : SyncVM::ROLcrr_s;
    }
    case SyncVM::RORrrr_s: {
      return reverse ? SyncVM::RORyrr_s : SyncVM::RORrrr_s;
    }
    }
  };

  // it is possible that we can merge two instructions, as long as we do not
  // call a scheduler the materialization of a const will be followed by its
  // use.
  auto MBBI = std::next(MachineBasicBlock::iterator(MI));
  auto outReg = MI.getOperand(0).getReg();
  
  // We temporarily disabled combining because it will introduce
  // a bug: when combining, we lose the MI's def, and when there are 
  // other uses of the def, we result wrong program.
  if (false && can_combine(MI, *MBBI)) {
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
        .add(otherReg)
        .addImm(0);
    MBBI->eraseFromParent();
    return;
  }

  if (false && can_non_commute_combine(MI, *MBBI)) {
    auto opcode = MBBI->getOpcode();

    bool reverse;
    MachineOperand *otherOpnd;
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
        .add(*otherOpnd)
        .addImm(0);
    MBBI->eraseFromParent();
    return;
  }

  BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII->get(SyncVM::ADDcrr_s))
      .add(Reg)
      .addImm(0)
      .add(ConstantPool)
      .addReg(SyncVM::R0)
      .addImm(0)
      .getInstr();
}

bool SyncVMExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** SyncVM EXPAND PSEUDO INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  TII = MF.getSubtarget<SyncVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  Context = &MF.getFunction().getContext();

  std::vector<MachineInstr *> PseudoInst;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!MI.isPseudo())
        continue;

      if (MI.getOpcode() == SyncVM::INVOKE) {
        // convert INVOKE to an actual call
        Register ABIReg = MI.getOperand(0).getReg();
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(SyncVM::NEAR_CALL))
            .addReg(ABIReg)
            .add(MI.getOperand(1))
            .add(MI.getOperand(2));
        PseudoInst.push_back(&MI);
        continue;
      }

      if (MI.getOpcode() == SyncVM::CONST) {
        expandConst(MI);
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == SyncVM::LOADCONST) {
        expandLoadConst(MI);
        PseudoInst.push_back(&MI);
      }
    }

  // Handle calls
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == SyncVM::INVOKE) {
        // convert INVOKE to an actual near_call
        Register ABIReg = MI.getOperand(0).getReg();
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(SyncVM::NEAR_CALL))
            .addReg(ABIReg)
            .add(MI.getOperand(1))
            .add(MI.getOperand(2));
        PseudoInst.push_back(&MI);
      } else if (MI.getOpcode() == SyncVM::CALL) {
        // Special handling of calling to __cxa_throw.
        // If we are calling into the throw wrapper function, we jump into a 
        // local frame with unwind path of `DEFAULT_UNWIND`, which will turn
        // our prepared THROW into a PANIC. This will cause values in registers
        // not propagated back to upper level, causing lost of returndata 
        auto *func_opnd = MI.getOperand(1).getGlobal();
        auto func_name = func_opnd->getName();
        if (func_name == "__cxa_throw") {
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                TII->get(SyncVM::THROW));
        } else {
          // One of the problem: the backend cannot restrict frontend to not emit
          // calls (Should we reinforce it?) so this route is needed.
          // If a call is generated, it is incomplete as it misses EH label info,
          // pad 0 instead.
          Register ABIReg = MI.getOperand(0).getReg();
          BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                  TII->get(SyncVM::NEAR_CALL))
              .addReg(ABIReg)
              .add(MI.getOperand(1))
              .addExternalSymbol(
                  "DEFAULT_UNWIND"); // Linker inserts a basic block
                                     // which bubbles up the exception.
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
FunctionPass *llvm::createSyncVMExpandPseudoPass() {
  return new SyncVMExpandPseudo();
}
