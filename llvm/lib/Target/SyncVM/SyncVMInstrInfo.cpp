//===-- SyncVMInstrInfo.cpp - SyncVM Instruction Information --------------===//
//
// This file contains the SyncVM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "SyncVMInstrInfo.h"

#include <deque>

#include "SyncVM.h"
#include "SyncVMMachineFunctionInfo.h"
#include "SyncVMTargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#define GET_INSTRMAP_INFO
#include "SyncVMGenInstrInfo.inc"

// Pin the vtable to this file.
void SyncVMInstrInfo::anchor() {}

SyncVMInstrInfo::SyncVMInstrInfo()
    : SyncVMGenInstrInfo(SyncVM::ADJCALLSTACKDOWN, SyncVM::ADJCALLSTACKUP),
      RI() {}

unsigned SyncVMInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");

  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;

  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!I->isBranch())
      break;
    // Remove the branch.
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }

  return Count;
}

bool SyncVMInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid Xbranch condition!");

  SyncVMCC::CondCodes CC = static_cast<SyncVMCC::CondCodes>(
      Cond[0].isImm() ? Cond[0].getImm() : Cond[0].getCImm()->getZExtValue());

  switch (CC) {
  default:
    llvm_unreachable("Invalid branch condition!");
  case SyncVMCC::COND_E:
    CC = SyncVMCC::COND_NE;
    break;
  case SyncVMCC::COND_NE:
    CC = SyncVMCC::COND_E;
    break;
  case SyncVMCC::COND_LT:
    CC = SyncVMCC::COND_GE;
    break;
  case SyncVMCC::COND_GE:
    CC = SyncVMCC::COND_LT;
    break;
  case SyncVMCC::COND_LE:
    CC = SyncVMCC::COND_GT;
    break;
  case SyncVMCC::COND_GT:
    CC = SyncVMCC::COND_LE;
    break;
  }

  Cond.pop_back();
  Cond.push_back(MachineOperand::CreateImm(CC));
  return false;
}

bool SyncVMInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  // Start from the bottom of the block and work up, examining the
  // terminator instructions.
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;

    // Working from the bottom, when we see a non-terminator
    // instruction, we're done.
    if (!isUnpredicatedTerminator(*I))
      break;

    // A terminator that isn't a branch can't easily be handled
    // by this analysis.
    if (!I->isBranch())
      return true;

    if (I->getOpcode() == SyncVM::J) {
      // Handle unconditional branches.
      auto jumpTarget = I->getOperand(0).getMBB();
      if (!AllowModify) {
        TBB = jumpTarget;
        continue;
      }

      // If the block has any instructions after a JMP, delete them.
      MBB.erase(std::next(I), MBB.end());

      Cond.clear();
      FBB = nullptr;

      // Delete the JMP if it's equivalent to a fall-through.
      if (MBB.isLayoutSuccessor(jumpTarget)) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }

      // TBB is used to indicate the unconditional destination.
      TBB = jumpTarget;
      continue;
    }

    // Handle unconditional branches.
    if (getImmOrCImm(I->getOperand(1)) == 0) {
      // TBB is used to indicate the unconditional destination.
      TBB = I->getOperand(0).getMBB();

      if (AllowModify) {
        // If the block has any instructions after a JMP, delete them.
        while (std::next(I) != MBB.end())
          std::next(I)->eraseFromParent();
        Cond.clear();
        FBB = nullptr;

        // Delete the JMP if it's equivalent to a fall-through.
        if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
          TBB = nullptr;
          I->eraseFromParent();
          I = MBB.end();
          continue;
        }
      }

      continue;
    }

    SyncVMCC::CondCodes BranchCode =
        static_cast<SyncVMCC::CondCodes>(getImmOrCImm(I->getOperand(1)));
    if (BranchCode == SyncVMCC::COND_INVALID)
      return true; // Can't handle weird stuff.

    // Working from the bottom, handle the first conditional branch.
    if (Cond.empty()) {
      FBB = TBB;
      TBB = I->getOperand(0).getMBB();
      LLVMContext &C = MBB.getParent()->getFunction().getContext();
      auto CImmCC =
          ConstantInt::get(IntegerType::get(C, 256), BranchCode, false);
      Cond.push_back(MachineOperand::CreateCImm(CImmCC));
      continue;
    }

    // Handle subsequent conditional branches. Only handle the case where all
    // conditional branches branch to the same destination.
    assert(Cond.size() == 1);
    assert(TBB);

    // Only handle the case where all conditional branches branch to
    // the same destination.
    if (TBB != I->getOperand(0).getMBB())
      return true;

    SyncVMCC::CondCodes OldBranchCode =
        (SyncVMCC::CondCodes)Cond[0].getCImm()->getZExtValue();
    // If the conditions are the same, we can leave them alone.
    if (OldBranchCode == BranchCode)
      continue;

    return true;
  }

  return false;
}

unsigned SyncVMInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.size() == 0) &&
         "SyncVM branch conditions have one component!");
  assert(!BytesAdded && "code size not handled");

  if (Cond.empty()) {
    // Unconditional branch?
    assert(!FBB && "Unconditional branch with multiple successors!");
    BuildMI(&MBB, DL, get(SyncVM::J)).addMBB(TBB);
    return 1;
  }
  // Conditional branch.
  unsigned Count = 0;
  auto cond_code = getImmOrCImm(Cond[0]);
  BuildMI(&MBB, DL, get(SyncVM::JC)).addMBB(TBB).addImm(cond_code);
  ++Count;

  if (FBB) {
    // Two-way Conditional branch. Insert the second branch.
    BuildMI(&MBB, DL, get(SyncVM::J)).addMBB(FBB);
    ++Count;
  }
  return Count;
}

void SyncVMInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI,
                                          Register SrcReg, bool isKill,
                                          int FrameIdx,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &SyncVM::GR256RegClass) {
    BuildMI(MBB, MI, DL, get(SyncVM::ADDrrs_s))
        .addReg(SrcReg, getKillRegState(isKill))
        .addReg(SyncVM::R0)
        .addFrameIndex(FrameIdx)
        .addImm(32)
        .addImm(0)
        .addImm(0)
        .addMemOperand(MMO);
  } else if (RC == &SyncVM::GRPTRRegClass) {
    BuildMI(MBB, MI, DL, get(SyncVM::PTR_ADDrrs_s))
        .addReg(SrcReg, getKillRegState(isKill))
        .addReg(SyncVM::R0)
        .addFrameIndex(FrameIdx)
        .addImm(32)
        .addImm(0)
        .addImm(0)
        .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

void SyncVMInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIdx, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIdx),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIdx),
      MFI.getObjectAlign(FrameIdx));

  if (RC == &SyncVM::GR256RegClass) {
    BuildMI(MBB, MI, DL, get(SyncVM::ADDsrr_s))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIdx)
        .addImm(32)
        .addImm(0)
        .addReg(SyncVM::R0)
        .addImm(0)
        .addMemOperand(MMO);
  } else if (RC == &SyncVM::GRPTRRegClass) {
    BuildMI(MBB, MI, DL, get(SyncVM::PTR_ADDsrr_s))
        .addReg(DestReg, getDefRegState(true))
        .addFrameIndex(FrameIdx)
        .addImm(32)
        .addImm(0)
        .addReg(SyncVM::R0)
        .addImm(0)
        .addMemOperand(MMO);
  } else {
    llvm_unreachable("Cannot store this register to stack slot!");
  }
}

void SyncVMInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const DebugLoc &DL, MCRegister DestReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  unsigned opcode = I->getFlag(MachineInstr::MIFlag::IsFatPtr)
                        ? SyncVM::PTR_ADDrrr_s
                        : SyncVM::ADDrrr_s;

  BuildMI(MBB, I, DL, get(opcode), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addReg(SyncVM::R0)
      .addImm(0);
}

/// GetInstSize - Return the number of bytes of code the specified
/// instruction may be.  This returns the maximum number of bytes.
///
unsigned SyncVMInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  const MCInstrDesc &Desc = MI.getDesc();
  if (Desc.getOpcode() == TargetOpcode::DBG_VALUE)
    return 0;
  return Desc.getSize();
}

bool SyncVMInstrInfo::isFarCall(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case SyncVM::FAR_CALL:
  case SyncVM::STATIC_CALL:
  case SyncVM::DELEGATE_CALL:
  case SyncVM::MIMIC_CALL:
    return true;
  }
  return false;
}

#define INSTR_TESTER(NAME, OPCODE)                                             \
  bool SyncVMInstrInfo::is##NAME(const MachineInstr &MI) const {               \
    StringRef Mnemonic = getName(MI.getOpcode());                              \
    return Mnemonic.startswith(#OPCODE);                                       \
  }

INSTR_TESTER(Add, ADD)
INSTR_TESTER(Sub, SUB)
INSTR_TESTER(Mul, MUL)
INSTR_TESTER(Div, DIV)
INSTR_TESTER(And, AND)
INSTR_TESTER(Or, OR)
INSTR_TESTER(Xor, XOR)
INSTR_TESTER(Shl, SHL)
INSTR_TESTER(Shr, SHR)
INSTR_TESTER(Rol, ROL)
INSTR_TESTER(Ror, ROR)
INSTR_TESTER(Sel, SEL)
INSTR_TESTER(Load, LD)
INSTR_TESTER(Store, ST)
INSTR_TESTER(FatLoad, FATPTR_LD)
INSTR_TESTER(NOP, NOP)

bool SyncVMInstrInfo::isPtr(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.startswith("PTR");
}

bool SyncVMInstrInfo::isNull(const MachineInstr &MI) const {
  return isAdd(MI) && hasRROperandAddressingMode(MI) && MI.getNumDefs() == 1u &&
         MI.getOperand(1).getReg() == SyncVM::R0 &&
         MI.getOperand(2).getReg() == SyncVM::R0;
}

bool SyncVMInstrInfo::isSilent(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.endswith("s");
}

SyncVMInstrInfo::GenericInstruction
SyncVMInstrInfo::genericInstructionFor(const MachineInstr &MI) const {
  if (isAdd(MI))
    return ADD;
  if (isSub(MI))
    return SUB;
  if (isMul(MI))
    return MUL;
  if (isDiv(MI))
    return DIV;
  return Unsupported;
}

bool SyncVMInstrInfo::hasRIOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return find(Mnemonic, 'i') != Mnemonic.end();
}

bool SyncVMInstrInfo::hasRXOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return find(Mnemonic, 'x') != Mnemonic.end();
}

bool SyncVMInstrInfo::hasRSOperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  auto LCIt = find_if<StringRef, int(int)>(std::move(Mnemonic), std::islower);

  return LCIt != Mnemonic.end() && (*LCIt == 's' || *LCIt == 'y');
}

bool SyncVMInstrInfo::hasRROperandAddressingMode(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  auto LCIt = find_if<StringRef, int(int)>(std::move(Mnemonic), std::islower);

  return LCIt != Mnemonic.end() && *LCIt == 'r' && *std::next(LCIt) == 'r';
}

/// Mark a copy to a fat pointer register or from a fat pointer register with
/// IsFatPtr MI flag. Then when COPY is lowered to a real instruction the flag
/// is used to choose between add and ptr.add.
void SyncVMInstrInfo::tagFatPointerCopy(MachineInstr &MI) const {
  if (!MI.isCopy())
    return;

  const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
  Register DstReg = MI.getOperand(0).getReg();
  // If COPY doesn't have arguments use destination reg class twice in the
  // check.
  Register SrcReg = (MI.getNumOperands() > 1 && MI.getOperand(1).isReg())
                        ? MI.getOperand(1).getReg()
                        : DstReg;

  auto isFPReg = [&MRI](Register R) {
    return R.isVirtual() &&
           MRI.getRegClass(R)->getID() == SyncVM::GRPTRRegClassID;
  };

  if (isFPReg(SrcReg) || isFPReg(DstReg))
    MI.setFlag(MachineInstr::MIFlag::IsFatPtr);
}

bool SyncVMInstrInfo::isPredicatedInstr(MachineInstr &MI) const {
  return isArithmetic(MI) || isBitwise(MI) || isShift(MI) || isRotate(MI) ||
         isLoad(MI) || isFatLoad(MI) || isStore(MI) || isNOP(MI) || isSel(MI);
}

/// Returns the predicate operand
unsigned SyncVMInstrInfo::getCCCode(MachineInstr &MI) const {
  assert(isPredicatedInstr(MI) && "MI is not predicated");
  auto CC = MI.getOperand(MI.getNumExplicitOperands() - 1);

  if (CC.isImm()) {
    return CC.getImm();
  } else if (CC.isCImm()) {
    return CC.getCImm()->getZExtValue();
  }
  llvm_unreachable("CC operand is not immediate");
}

bool SyncVMInstrInfo::isUnconditionalNonTerminator(MachineInstr &MI) const {
  if (MI.isTerminator())
    return false;
  if (!isPredicatedInstr(MI))
    return true;

  // check if implicitly uses flags
  for (auto &opnd : MI.implicit_operands()) {
    if (opnd.isReg() && opnd.getReg() == SyncVM::Flags)
      return false;
  }

  return getCCCode(MI) == SyncVMCC::COND_NONE;
}
