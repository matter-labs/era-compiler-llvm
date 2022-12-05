//===------ SyncVMCombineInstrs.cpp - Combine instructions Pre-RA ---------===//
//
/// \file
/// This file contains a pass that combines instructions.
/// It relies on def-use chains and runs on pre-RA phase.
///
/// Similar to ARM architecture, arithmetic instructions in SyncVM has the
/// capability to set flags while doing computations. This creates an
/// opportunity to avoid explicit comparison with zero.
///
/// For example, the  following two instructions (not necessarily adjacent) that
/// produces `def` and `result` inside a basic block:
///
/// '''
/// def = op x, y
/// ; irrelevant instructions that do not touch flags register in the middle
/// ; ... ...
/// result = sub.s! 0, def
/// '''
///
/// can be folded into:
/// '''
/// def = op! x, y
/// '''
///
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-combine-instrs"
#define SYNCVM_COMBINE_INSTRUCTIONS_NAME "SyncVM combine instructions"

static cl::opt<bool> EnableSyncVMCombine("enable-syncvm-combine-instrs",
                                         cl::init(true), cl::Hidden);

namespace {

class SyncVMCombineInstrs : public MachineFunctionPass {
public:
  static char ID;
  SyncVMCombineInstrs() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_COMBINE_INSTRUCTIONS_NAME;
  }

private:
  const SyncVMInstrInfo *TII;
};

char SyncVMCombineInstrs::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMCombineInstrs, DEBUG_TYPE,
                SYNCVM_COMBINE_INSTRUCTIONS_NAME, false, false)

bool SyncVMCombineInstrs::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM COMBINE INSTRUCTIONS **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  if (!EnableSyncVMCombine) {
    LLVM_DEBUG(dbgs() << "manually disabled optimization\n");
    return false;
  }

  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  std::vector<MachineInstr *> ToRemove;

  for (MachineBasicBlock &MBB : MF)
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      // Iterate over a BB, and look for a comparison with 0 instruction
      if (MI->getOpcode() == SyncVM::SUBxrr_v) {
        // must be comparing with constant zero
        if (!MI->getOperand(1).getCImm()->isZero()) {
          continue;
        }

        // look for the register (that are compared with zero) definition
        Register OpReg = MI->getOperand(2).getReg();

        // Skip the combination if reg has multiple definitions
        if (!RegInfo.hasOneDef(OpReg)) {
          LLVM_DEBUG(
              dbgs() << "  ** Skipping combining because reg has not one def:";
              MI->dump());
          continue;
        }

        MachineInstr *DefMI = &*RegInfo.def_instructions(OpReg).begin();

        // Since we are moving flag setting to a previous instruction,
        // we are literally re-scheduling locally the comparison instruction
        // to an early position. The state of the flags must not change.
        auto hasNoFlagsDefOrUseBetween = [&](auto Start, auto End) {
          // Def and use must be within the same basic block
          if (Start->getParent() != End->getParent()) {
            LLVM_DEBUG(dbgs() << "  ** Skipping combining because instructions "
                                 "are not in the same BB.\n");
            return false;
          }
          // Check for def or use of flags between the two instructions
          for (auto I = std::next(Start); I != End; ++I) {
            LLVM_DEBUG(dbgs() << "      ** Checking instruction:"; I->dump());
            if (TII->isFlagSettingInstruction(I->getOpcode()) ||
                I->getOpcode() == SyncVM::ADDframe || I->isCall()) {
              LLVM_DEBUG(dbgs() << "  ** Instruction might set flags:";
                         I->dump());
              return false;
            }
            if (!TII->isUnconditionalNonTerminator(*I)) {
              LLVM_DEBUG(dbgs() << "  ** Instruction reads flags:"; I->dump());
              return false;
            }
          }
          return true;
        };

        if (!hasNoFlagsDefOrUseBetween(DefMI->getIterator(), MI)) {
          LLVM_DEBUG(dbgs() << " .   Def MI: "; DefMI->dump());
          LLVM_DEBUG(dbgs() << " .   Use MI: "; MI->dump());
          continue;
        }

        // DefMI's result cannot be used more than once
        // TODO: CPR-894 investigate why this constraint is necessary --
        // removing it will result in failures.
        Register DefResultReg = DefMI->getOperand(0).getReg();
        if (!RegInfo.hasOneUse(DefResultReg)) {
          LLVM_DEBUG(dbgs() << "  ** Skipping combining because DefMI's result "
                               "has multiple uses:";
                     DefMI->dump());
          continue;
        }

        // check if DefMI can have a flag setting opcode
        if (llvm::SyncVM::getFlagSettingOpcode(DefMI->getOpcode()) == -1) {
          LLVM_DEBUG(dbgs() << "  ** Skipping because cannot find flag setting "
                               "opcode for defining instruction:";
                     DefMI->dump());
          continue;
        }

        // mul and div may have different flag setting scheme so skip them as
        // well
        // TODO: CPR-886 enable combination of mul/div if possible.
        if ((TII->isMul(*DefMI) || TII->isDiv(*DefMI))) {
          LLVM_DEBUG(dbgs()
                         << "  ** Skipping because of Mul and Div instruction:";
                     DefMI->dump());
          continue;
        }

        LLVM_DEBUG(dbgs() << "== Combined instruction:"; DefMI->dump();
                   dbgs() << "        And instruction:"; MI->dump(););

        // redirect output registers of the compare as it is the same with DefMI
        Register ResultReg = MI->getOperand(0).getReg();
        RegInfo.replaceRegWith(ResultReg, DefResultReg);

        // change DefMI's opcode to the flag setting opcode
        DefMI->setDesc(
            TII->get(SyncVM::getFlagSettingOpcode(DefMI->getOpcode())));

        // and remove the compare
        ToRemove.push_back(&*MI);

        LLVM_DEBUG(dbgs() << "== Into:"; DefMI->dump(););
      }
    }

  for (auto MI : ToRemove) {
    MI->eraseFromParent();
  }

  return !ToRemove.empty();
}

/// createSyncVMCombineInstrsPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createSyncVMCombineInstrsPass() {
  return new SyncVMCombineInstrs();
}
