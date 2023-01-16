//===-- SyncVMCombineSpills.cpp - Replace bytes addresses with cells ones
//--===//
//
/// \file
/// This pass tries to reduce the virtual register pressure before RegAlloc.
/// Specifically, we allocate stack slots for vregs that can be moved to stack,
/// this requires that the def MachineInstr can be converted to stack target
/// addressing mode, and all its use MachineInstr can be converted to stack
/// source addressing mode.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMInstrInfo.h"
#include "SyncVMSubtarget.h"

#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/InitializePasses.h"

#include "llvm/ADT/Statistic.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-combine-spills"
#define SYNCVM_COMBINE_SPILLS_NAME "SyncVM combine spills"

STATISTIC(ConvertedRegs, "Number of virtual registers converted");

static cl::opt<bool>
    DoNotConvertWhenStackEmpty("syncvm-no-combine-spills-when-frame-empty",
                               cl::init(true), cl::Hidden);

namespace {

class SyncVMCombineSpills : public MachineFunctionPass {
public:
  static char ID;
  SyncVMCombineSpills() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override { return SYNCVM_COMBINE_SPILLS_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  const SyncVMInstrInfo *TII;

  bool convertVReg(Register reg, MachineFunction &MF);
  static bool functionHasCall(MachineFunction &MF);

  bool isCandidateRegister(Register reg) const;

  MachineRegisterInfo *MRI;
  std::vector<Register>
  getSortedCandidateRegistersByLiveIntervalRange(MachineFunction &MF);

 LiveIntervals *LIS;
};

char SyncVMCombineSpills::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(SyncVMCombineSpills, DEBUG_TYPE,
                      SYNCVM_COMBINE_SPILLS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(SyncVMCombineSpills, DEBUG_TYPE, SYNCVM_COMBINE_SPILLS_NAME,
                    false, false)

bool SyncVMCombineSpills::isCandidateRegister(Register Reg) const {
  if (!Reg.isVirtual() || !LIS->hasInterval(Reg)) {
    LLVM_DEBUG(dbgs() << " . Reg is not virtual or has no interval: " << Reg
                      << ". Skipped.\n");
    return false;
  }

  const LiveInterval &CLI = LIS->getInterval(Reg);

  LLVM_DEBUG(dbgs() << " . Investigating LI for: "; CLI.print(dbgs());
             dbgs() << "\n";);

  auto CanTransformDefInstrToStackMode = [&](Register reg) {
    MachineInstr *DefMI = MRI->getUniqueVRegDef(reg);
    if (!DefMI) {
      LLVM_DEBUG(dbgs() << " . Cannot find DefMI of reg: " << reg
                        << ". Skipping\n");
      return false;
    }
    auto new_opcode = llvm::SyncVM::getStackSettingOpcode(DefMI->getOpcode());
    if (new_opcode == -1) {
      LLVM_DEBUG(
          dbgs() << " . Cannot transform def instruction to stack mode: ";
          DefMI->dump(););
      return false;
    }
    // if the defining operand is dead, skip it
    if (!DefMI) {
      LLVM_DEBUG(dbgs() << " . Skip: No defining instruction.");
      return false;
    }
    if (DefMI->getOperand(0).isDead()) {
      LLVM_DEBUG(dbgs() << " . Skip: Def MI is dead: "; DefMI->dump(););
      return false;
    }
    return true;
  };

  bool canTransform = CanTransformDefInstrToStackMode(Reg);
  if (!canTransform) {
    LLVM_DEBUG(dbgs() << "\n");
  }
  return canTransform;
}

std::vector<Register>
SyncVMCombineSpills::getSortedCandidateRegistersByLiveIntervalRange(
    MachineFunction &MF) {
  MRI = &MF.getRegInfo();
  assert(MRI);
  assert(MRI->isSSA() && "This pass requires MachineFunction to be SSA");
  assert(MRI->tracksLiveness() && "This pass requires MachineFunction to track "
                                  "liveness");

  std::vector<Register> sortedCandidateRegisters;

  // filter out possible live intervals and sort them by size (our heuristic
  // function)
  for (unsigned I = 0, E = MRI->getNumVirtRegs(); I != E; ++I) {
    auto Reg = Register::index2VirtReg(I);
    if (isCandidateRegister(Reg)) {
      assert(LIS->hasInterval(Reg));
      sortedCandidateRegisters.push_back(Reg);
    }
  }

  auto compareLI = [&](Register L, Register R) {
    assert(L);
    assert(R);
    auto &LLI = LIS->getInterval(L);
    auto &RLI = LIS->getInterval(R);
    return LLI.getSize() > RLI.getSize();
  };

  std::sort(sortedCandidateRegisters.begin(), sortedCandidateRegisters.end(),
            compareLI);

  LLVM_DEBUG(dbgs() << "Live intervals sorted:\n";
             for (auto Reg
                  : sortedCandidateRegisters) {
               auto &LI = LIS->getInterval(Reg);
               LI.print(dbgs());
               dbgs() << ", Size: " << LI.getSize() << "\n";
             });
  return sortedCandidateRegisters;
}

bool SyncVMCombineSpills::convertVReg(Register reg, MachineFunction &MF) {
  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  auto AllUsesCanBeConvertedToStackOperandMode = [&](Register reg) {
    for (const MachineInstr &UseMI : MRI->use_nodbg_instructions(reg)) {
      if (!TII->hasRROperandAddressingMode(UseMI)) {
        LLVM_DEBUG(
            dbgs() << "  Use MI is not Reg-Reg operand addressing mode: ";
            UseMI.dump(););
        return false;
      }
      int srOpcode =
          llvm::SyncVM::getSROperandAddressingModeOpcode(UseMI.getOpcode());
      if (srOpcode == -1) {
        LLVM_DEBUG(
            dbgs() << "  Use MI cannot be converted to Stack-Reg operand "
                      "addressing mode: ";
            UseMI.dump(););
        return false;
      }

      bool isFirstOperand =
          SyncVMInstrInfo::useRegIsFirstSourceOperand(UseMI, reg);

      if (!UseMI.isCommutable()) {
        if (!isFirstOperand) {
          // try to see if we can get a reversed operand opcode
          srOpcode = llvm::SyncVM::getReversedOperandOpcode(srOpcode);
          if (srOpcode == -1) {
            LLVM_DEBUG(dbgs()
                           << "  Use reg is 2nd operand of non-commutative MI, "
                              "and cannot find reversed operand opcode: ";
                       UseMI.dump(););
            return false;
          }
        }
      }
    }
    return true;
  };

  auto addStackOperand = [&](auto &NewMI, int new_fi) {
    NewMI.addFrameIndex(new_fi).addImm(32).addImm(0);
  };

  if (!AllUsesCanBeConvertedToStackOperandMode(reg)) {
    LLVM_DEBUG(dbgs() << "Cannot transform def instruction to stack mode\n");
    return false;
  }

  // now we can safely transform the program:

  // 0. allocate spilled stack slot. We could probably reuse stack slots
  // but that would not have performance benefits
  int new_fi = MF.getFrameInfo().CreateSpillStackObject(32, Align(1));

  // 1. convert def instruction to store to stack:
  MachineInstr *DefMI = MRI->getUniqueVRegDef(reg);
  if (!DefMI) {
    LLVM_DEBUG(dbgs() << "VReg has un-unique definition.\n");
    return false;
  }
  LLVM_DEBUG(dbgs() << "Converting def to stack target addressing mode: ";
             DefMI->dump());
  auto new_opcode = llvm::SyncVM::getStackSettingOpcode(DefMI->getOpcode());
  if (new_opcode == -1) {
    LLVM_DEBUG(
        dbgs()
        << "Def instruction does not have a stack mode opcode counterpart.\n");
    return false;
  }
  auto NewDefMI = BuildMI(*DefMI->getParent(), DefMI, DefMI->getDebugLoc(),
                          TII->get(new_opcode));
  // adds operands
  for (unsigned int i = 1; i < DefMI->getNumOperands() - 1; i++) {
    NewDefMI.add(DefMI->getOperand(i));
  }
  // adds stack slot operand
  addStackOperand(NewDefMI, new_fi);
  NewDefMI.addImm(0);
  LLVM_DEBUG(dbgs() << "To: "; NewDefMI->dump());

  std::vector<MachineInstr *> ToBeRemoved;
  ToBeRemoved.push_back(DefMI);

  // 2. convert all use instructions to load from stack
  for (MachineInstr &UseMI : MRI->use_instructions(reg)) {
    LLVM_DEBUG(dbgs() << "Converting use: "; UseMI.dump(););

    bool IsFirstOperand =
        SyncVMInstrInfo::useRegIsFirstSourceOperand(UseMI, reg);

    auto new_use_opcode =
        llvm::SyncVM::getSROperandAddressingModeOpcode(UseMI.getOpcode());
    if (new_use_opcode == -1) {
      LLVM_DEBUG(dbgs() << "Cannot transform use instruction to stack mode\n");
      llvm_unreachable("Unexpected error happened during conversion:"
                       " cannot find new use opcode");
    }

    // have to reverse operands if use is 2nd operand of non-commutative
    // instruction
    if (!UseMI.isCommutable()) {
      if (!IsFirstOperand) {
        int sr_opcode = llvm::SyncVM::getReversedOperandOpcode(new_use_opcode);
        if (sr_opcode == -1) {
          LLVM_DEBUG(dbgs()
                     << "Cannot get reversed operand opcode of use MI\n");
          llvm_unreachable("Unexpected error happened during conversion:"
                           "cannot find reverse operand opcode");
        }
        new_use_opcode = sr_opcode;
      }
    }

    auto NewUseMI = BuildMI(*UseMI.getParent(), &UseMI, UseMI.getDebugLoc(),
                            TII->get(new_use_opcode));

    // find the index of use:
    auto index = UseMI.findRegisterUseOperandIdx(reg);
    assert(index != -1);

    auto copyOverDestOperands = [](auto &SrcMI, auto &DestMI) {
      for (unsigned i = 0; i < SrcMI.getNumExplicitDefs(); ++i) {
        DestMI.add(SrcMI.getOperand(i));
      }
    };

    // first, copy over destination operands
    copyOverDestOperands(UseMI, NewUseMI);

    // then, stuff in stack operand
    addStackOperand(NewUseMI, new_fi);

    // add the other operands.
    if (IsFirstOperand) {
      // if it is first operand, then add the second operand
      for (unsigned i = index + 1; i < UseMI.getNumOperands(); ++i) {
        NewUseMI.add(UseMI.getOperand(i));
      }
    } else {
      NewUseMI.add(UseMI.getOperand(index - 1));
      for (unsigned i = index + 1; i < UseMI.getNumOperands(); ++i) {
        NewUseMI.add(UseMI.getOperand(i));
      }
    }

    LLVM_DEBUG(dbgs() << "  To new use: "; NewUseMI->dump());
    ToBeRemoved.push_back(&UseMI);
  }

  for (auto *MI : ToBeRemoved) {
    MI->eraseFromParent();
  }

  return true;
}

bool SyncVMCombineSpills::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM combine spills and uses "
                       "******************** Function: "
                    << MF.getName() << '\n');

  LIS = &getAnalysis<LiveIntervals>();

  if (DoNotConvertWhenStackEmpty) {
    if (MF.getFrameInfo().getStackSize() == 0 &&
        !SyncVMCombineSpills::functionHasCall(MF)) {
      LLVM_DEBUG(dbgs() << "skipping function analysis: function does not meet "
                           "the conversion criteria.\n");
      return false;
    }
  }

  bool Changed = false;

  std::vector<Register> sortedCandidateRegisters =
      getSortedCandidateRegistersByLiveIntervalRange(MF);

  for (auto reg : sortedCandidateRegisters) {
    bool converted = convertVReg(reg, MF);
    if (converted) {
      ++ConvertedRegs;
    }
    Changed |= converted;
  }

  return Changed;
}

bool SyncVMCombineSpills::functionHasCall(MachineFunction &MF) {
  for (MachineBasicBlock &MBB : MF) {
    auto It = llvm::find_if(MBB, [](MachineInstr &MI) { return MI.isCall(); });
    if (It != MBB.end()) {
      return true;
    }
  }
  return false;
}

/// createSyncVMCombineSpillsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMCombineSpillsPass() {
  return new SyncVMCombineSpills();
}
