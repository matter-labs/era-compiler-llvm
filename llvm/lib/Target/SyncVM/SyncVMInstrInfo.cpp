//===-- SyncVMInstrInfo.cpp - SyncVM Instruction Information --------------===//
//
// This file contains the SyncVM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "SyncVMInstrInfo.h"

#include <deque>
#include <optional>

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

namespace llvm {
namespace SyncVM {

ArgumentType argumentType(ArgumentKind Kind, unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  // TODO: Mappings for Select.
  // Select is not a part of a mapping, so have to handle it manually.
  const DenseSet<unsigned> In0R = {SyncVM::SELrrr, SyncVM::SELrir,
                                   SyncVM::SELrcr, SyncVM::SELrsr,
                                   SyncVM::FATPTR_SELrrr};
  const DenseSet<unsigned> In0I = {SyncVM::SELirr, SyncVM::SELiir,
                                   SyncVM::SELicr, SyncVM::SELisr};
  const DenseSet<unsigned> In0C = {SyncVM::SELcrr, SyncVM::SELcir,
                                   SyncVM::SELccr, SyncVM::SELcsr};
  const DenseSet<unsigned> In0S = {SyncVM::SELsrr, SyncVM::SELsir,
                                   SyncVM::SELscr, SyncVM::SELssr};
  const DenseSet<unsigned> In1R = {SyncVM::SELrrr, SyncVM::SELirr,
                                   SyncVM::SELcrr, SyncVM::SELsrr,
                                   SyncVM::FATPTR_SELrrr};
  const DenseSet<unsigned> In1I = {SyncVM::SELrir, SyncVM::SELiir,
                                   SyncVM::SELcir, SyncVM::SELsir};
  const DenseSet<unsigned> In1C = {SyncVM::SELrcr, SyncVM::SELicr,
                                   SyncVM::SELccr, SyncVM::SELscr};
  const DenseSet<unsigned> In1S = {SyncVM::SELrsr, SyncVM::SELisr,
                                   SyncVM::SELcsr, SyncVM::SELssr};
  if (Kind == ArgumentKind::Out1) {
    // TODO: Support stack output for Select.
    return ArgumentType::Register;
  } else if (Kind == ArgumentKind::In1) {
    if (In1R.count(Opcode))
      return ArgumentType::Register;
    if (In1I.count(Opcode))
      return ArgumentType::Immediate;
    if (In1C.count(Opcode))
      return ArgumentType::Code;
    if (In1S.count(Opcode))
      return ArgumentType::Stack;
    return ArgumentType::Register;
  } else if (Kind == ArgumentKind::Out0) {
    if (hasSROutAddressingMode(Opcode))
      return ArgumentType::Stack;
    return ArgumentType::Register;
  }
  assert(Kind == ArgumentKind::In0);
  if (In0R.count(Opcode))
    return ArgumentType::Register;
  if (In0I.count(Opcode))
    return ArgumentType::Immediate;
  if (In0C.count(Opcode))
    return ArgumentType::Code;
  if (In0S.count(Opcode))
    return ArgumentType::Stack;
  if (hasRRInAddressingMode(Opcode))
    return ArgumentType::Register;
  if (hasIRInAddressingMode(Opcode))
    return ArgumentType::Immediate;
  if (hasCRInAddressingMode(Opcode))
    return ArgumentType::Code;
  return ArgumentType::Stack;
}

MachineInstr::mop_iterator in0Iterator(MachineInstr &MI) {
  return MI.operands_begin() + MI.getNumExplicitDefs();
}

MachineInstr::mop_iterator in1Iterator(MachineInstr &MI) {
  return in0Iterator(MI) + argumentSize(ArgumentKind::In0, MI);
}

MachineInstr::mop_iterator out0Iterator(MachineInstr &MI) {
  auto Begin = MI.operands_begin();
  if (hasRROutAddressingMode(MI) || isSelect(MI))
    return Begin;
  return in1Iterator(MI) + argumentSize(ArgumentKind::In1, MI);
}

MachineInstr::mop_iterator out1Iterator(MachineInstr &MI) {
  return MI.operands_begin() + MI.getNumExplicitDefs() - 1;
}

int getWithRRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_0); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_0); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_0); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_0);
}

int getWithIRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_1); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_1); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_1); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_1);
}

int getWithCRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_2); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_2); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_2); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_2);
}

int getWithSRInAddrMode(uint16_t Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  if (int Result = mapRRInputTo(Opcode, OperandAM_3); Result != -1)
    return Result;
  if (int Result = mapIRInputTo(Opcode, OperandAM_3); Result != -1)
    return Result;
  if (int Result = mapCRInputTo(Opcode, OperandAM_3); Result != -1)
    return Result;
  return mapSRInputTo(Opcode, OperandAM_3);
}

int getWithRROutAddrMode(uint16_t Opcode) {
  if (int Result = withRegisterResult(Opcode); Result != -1)
    return Result;
  return Opcode;
}

int getWithSROutAddrMode(uint16_t Opcode) {
  if (int Result = withStackResult(Opcode); Result != -1)
    return Result;
  return Opcode;
}

int getWithInsNotSwapped(uint16_t Opcode) {
  if (int Result = withInsNotSwapped(Opcode); Result != -1)
    return Result;
  return Opcode;
}

int getWithInsSwapped(uint16_t Opcode) {
  if (int Result = withInsSwapped(Opcode); Result != -1)
    return Result;
  return Opcode;
}

bool hasRRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapRRInputTo(Opcode, OperandAM_0) == Opcode;
}

bool hasIRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapIRInputTo(Opcode, OperandAM_1) == Opcode;
}

bool hasCRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapCRInputTo(Opcode, OperandAM_2) == Opcode;
}

bool hasSRInAddressingMode(unsigned Opcode) {
  Opcode = getWithInsNotSwapped(Opcode);
  return (unsigned)mapSRInputTo(Opcode, OperandAM_3) == Opcode;
}

bool hasRROutAddressingMode(unsigned Opcode) {
  return withStackResult(Opcode) != -1;
}

bool hasSROutAddressingMode(unsigned Opcode) {
  return withRegisterResult(Opcode) != -1;
}

// TODO: Implement in via td.
bool isSelect(unsigned Opcode) {
  DenseSet<unsigned> Members = {
      SyncVM::SELrrr,       SyncVM::SELrir, SyncVM::SELrcr, SyncVM::SELrsr,
      SyncVM::SELirr,       SyncVM::SELiir, SyncVM::SELicr, SyncVM::SELisr,
      SyncVM::SELcrr,       SyncVM::SELcir, SyncVM::SELccr, SyncVM::SELcsr,
      SyncVM::SELsrr,       SyncVM::SELsir, SyncVM::SELscr, SyncVM::SELssr,
      SyncVM::FATPTR_SELrrr};
  return Members.count(Opcode);
}

bool hasInvalidRelativeStackAccess(MachineInstr::const_mop_iterator Op) {
  auto SA = SyncVM::classifyStackAccess(Op);
  if (SA == SyncVM::StackAccess::Invalid)
    return true;

  if (SA == SyncVM::StackAccess::Relative && !(Op + 2)->isImm())
    return true;
  return false;
}

} // namespace SyncVM
} // namespace llvm

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
  case SyncVMCC::COND_OF:
    // we cannot reverse overflow checking condition
    return true;
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

    // We can't analyze if target address is on stack.
    if (I->getOpcode() == SyncVM::J_s)
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
  return isAdd(MI) && SyncVM::hasRRInAddressingMode(MI) &&
         MI.getNumDefs() == 1u && MI.getOperand(1).getReg() == SyncVM::R0 &&
         MI.getOperand(2).getReg() == SyncVM::R0;
}

bool SyncVMInstrInfo::isSilent(const MachineInstr &MI) const {
  StringRef Mnemonic = getName(MI.getOpcode());
  return Mnemonic.endswith("s");
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

bool SyncVMInstrInfo::isPredicatedInstr(const MachineInstr &MI) const {
  return MI.isBranch() || isArithmetic(MI) || isBitwise(MI) || isShift(MI) ||
         isRotate(MI) || isLoad(MI) || isFatLoad(MI) || isStore(MI) ||
         isNOP(MI) || isSel(MI) || isPtr(MI);
}

/// Returns the predicate operand
SyncVMCC::CondCodes SyncVMInstrInfo::getCCCode(const MachineInstr &MI) const {
  if (!isPredicatedInstr(MI))
    return SyncVMCC::COND_INVALID;

  auto CC = MI.getOperand(MI.getNumExplicitOperands() - 1);

  if (CC.isImm()) {
    return SyncVMCC::CondCodes(CC.getImm());
  } else if (CC.isCImm()) {
    return SyncVMCC::CondCodes(CC.getCImm()->getZExtValue());
  }
  return SyncVMCC::COND_INVALID;
}

/// Return true if the first instruction in a function is stack advance.
static bool hasStackAdvanceInstruction(MachineFunction &MF) {
  auto EntryBB = MF.begin();
  auto FirstMI = EntryBB->getFirstNonDebugInstr();
  return FirstMI != EntryBB->end() && FirstMI->getOpcode() == SyncVM::NOPSP;
}

/// Sets the offsets on instructions in MBB which use SP, so that they will be
/// valid post-outlining.
static void fixupStackOffsetPostOutline(MachineBasicBlock &MBB,
                                        int64_t FixupOffset) {
  auto AdjustDisp = [FixupOffset](MachineInstr::mop_iterator It) {
    if (SyncVM::classifyStackAccess(It) != SyncVM::StackAccess::Relative)
      return;

    auto DispIt = It + 2;
    assert(DispIt->isImm() && "Displacement is not immediate.");
    DispIt->setImm(DispIt->getImm() + FixupOffset);
  };

  for (MachineInstr &MI : MBB) {
    if (SyncVM::hasSRInAddressingMode(MI))
      AdjustDisp(SyncVM::in0Iterator(MI));
    if (SyncVM::hasSROutAddressingMode(MI))
      AdjustDisp(SyncVM::out0Iterator(MI));

    // Adjust sub instruction that was generated from ADDframe during
    // elimination of frame indices.
    if (MI.getOpcode() == SyncVM::SUBxrr_s &&
        MI.getOperand(1).getTargetFlags() == SyncVMII::MO_STACK_SLOT_IDX) {
      auto &StackSlotOP = MI.getOperand(1);
      StackSlotOP.setImm(StackSlotOP.getImm() - FixupOffset);
    }
  }
}

/// Since we are placing return address from the outlined function onto the top
/// of the stack, we need to reserve stack slot for it and to adjust
/// instructions which use SP in this function.
void SyncVMInstrInfo::fixupPostOutline(MachineFunction &MF) const {
  auto *SFI = MF.getInfo<SyncVMMachineFunctionInfo>();
  if (SFI->isStackAdjustedPostOutline())
    return;

  // Reserve stack slot for return address from outlined function.
  if (hasStackAdvanceInstruction(MF)) {
    auto NopIt = MF.begin()->getFirstNonDebugInstr();
    auto &StackAdvanceOp = NopIt->getOperand(0);
    StackAdvanceOp.setImm(StackAdvanceOp.getImm() + 1 /* StackSlotSize */);
  } else {
    auto EntryBB = MF.begin();
    BuildMI(*EntryBB, EntryBB->begin(), DebugLoc(), get(SyncVM::NOPSP))
        .addImm(1 /* StackSlotSize */)
        .addImm(SyncVMCC::COND_NONE);
  }

  // Adjust instructions which use SP in this function.
  for (MachineBasicBlock &MBB : MF)
    fixupStackOffsetPostOutline(MBB, -1 /* FixupOffset */);

  SFI->setStackAdjustedPostOutline();
}

/// Enum values indicating how an outlined call should be constructed.
enum MachineOutlinerConstructionID { MachineOutlinerDefault };

bool SyncVMInstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  return true;
}

bool SyncVMInstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();

  // Can F be deduplicated by the linker? If it can, don't outline from it.
  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;

  // Don't outline from functions with section markings; the program could
  // expect that all the code is in the named section.
  if (F.hasSection())
    return false;

  // Don't outline from functions if there is non-adjustable stack relative
  // addressing instruction.
  for (MachineBasicBlock &MBB : MF) {
    if (any_of(instructionsWithoutDebug(MBB.begin(), MBB.end()),
               [](MachineInstr &MI) {
                 if (SyncVM::hasSRInAddressingMode(MI) &&
                     SyncVM::hasInvalidRelativeStackAccess(
                         SyncVM::in0Iterator(MI)))
                   return true;
                 if (SyncVM::hasSROutAddressingMode(MI) &&
                     SyncVM::hasInvalidRelativeStackAccess(
                         SyncVM::out0Iterator(MI)))
                   return true;
                 return false;
               }))
      return false;
  }

  // It's safe to outline from MF.
  return true;
}

bool SyncVMInstrInfo::isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                                             unsigned &Flags) const {
  return TargetInstrInfo::isMBBSafeToOutlineFrom(MBB, Flags);
}

outliner::InstrType
SyncVMInstrInfo::getOutliningType(MachineBasicBlock::iterator &MBBI,
                                  unsigned Flags) const {
  MachineInstr &MI = *MBBI;

  // Don't allow debug values to impact outlining type.
  if (MI.isDebugInstr() || MI.isIndirectDebugValue())
    return outliner::InstrType::Invisible;

  // Don't allow instructions which won't be materialized to impact outlining
  // analysis.
  if (MI.isMetaInstruction())
    return outliner::InstrType::Invisible;

  // Don't outline context.gas_left instruction.
  if (MI.getOpcode() == SyncVM::CTXr_se &&
      getImmOrCImm(MI.getOperand(1)) == SyncVMCTX::GAS_LEFT)
    return outliner::InstrType::Illegal;

  // Positions generally can't safely be outlined.
  if (MI.isPosition())
    return outliner::InstrType::Illegal;

  // Don't trust the user to write safe inline assembly.
  if (MI.isInlineAsm())
    return outliner::InstrType::Illegal;

  // We can't outline branches and return statements.
  if (MI.isTerminator() || MI.isReturn())
    return outliner::InstrType::Illegal;

  // Don't outline instructions that set or modify SP.
  if (MI.getDesc().hasImplicitDefOfPhysReg(SyncVM::SP))
    return outliner::InstrType::Illegal;

  // Make sure the operands don't reference something unsafe.
  for (const auto &MO : MI.operands())
    if (MO.isMBB() || MO.isBlockAddress() || MO.isCPI() || MO.isJTI())
      return outliner::InstrType::Illegal;

  return outliner::InstrType::Legal;
}

outliner::OutlinedFunction SyncVMInstrInfo::getOutliningCandidateInfo(
    std::vector<outliner::Candidate> &RepeatedSequenceLocs) const {
  unsigned SequenceSize =
      std::accumulate(RepeatedSequenceLocs[0].front(),
                      std::next(RepeatedSequenceLocs[0].back()), 0,
                      [this](unsigned Sum, const MachineInstr &MI) {
                        return Sum + getInstSizeInBytes(MI);
                      });

  SmallPtrSet<MachineFunction *, 4> NoStackAdvance;
  unsigned SaveRetSize = get(SyncVM::ADDcrs_s).getSize();
  unsigned JumpSize = get(SyncVM::JCALL).getSize();
  unsigned CallOverhead = SaveRetSize + JumpSize;
  for (auto &C : RepeatedSequenceLocs) {
    unsigned Overhead = CallOverhead;
    MachineFunction *MF = C.getMF();

    // If function doesn't have stack advance instruction, add overhead only
    // once for each function.
    if (!hasStackAdvanceInstruction(*MF) && NoStackAdvance.insert(MF).second)
      Overhead += get(SyncVM::NOPSP).getSize();

    C.setCallInfo(MachineOutlinerDefault, Overhead);
  }

  unsigned JumpSPSize = get(SyncVM::J_s).getSize();
  unsigned FrameOverhead = JumpSPSize;
  return outliner::OutlinedFunction(RepeatedSequenceLocs, SequenceSize,
                                    FrameOverhead, MachineOutlinerDefault);
}

void SyncVMInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  // Perform stack fixup only if we didn't do it in the original function from
  // which we are outlining. Machine outliner algorithm is outlining from
  // the first candidate, so take original function from it.
  MachineFunction *OriginalMF = OF.Candidates.front().getMF();
  auto *SFI = OriginalMF->getInfo<SyncVMMachineFunctionInfo>();
  if (!SFI->isStackAdjustedPostOutline())
    fixupStackOffsetPostOutline(MBB, -1 /* FixupOffset */);

  DebugLoc DL;
  if (!MBB.empty())
    DL = MBB.back().getDebugLoc();

  // Add jump instruction to the end of the outlined frame.
  MBB.insert(MBB.end(), BuildMI(MF, DL, get(SyncVM::J_s))
                            .addReg(SyncVM::SP)
                            .addImm(0 /* AMBase2 */)
                            .addImm(-1 /* StackOffset */));
}

MachineBasicBlock::iterator SyncVMInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &OutlinedMF, outliner::Candidate &C) const {
  MachineFunction &MF = *C.getMF();
  fixupPostOutline(MF);

  DebugLoc DL;
  if (It != MBB.end())
    DL = It->getDebugLoc();

  // Add jump instruction to the outlined function at the given location.
  It = MBB.insert(It,
                  BuildMI(MF, DL, get(SyncVM::JCALL))
                      .addGlobalAddress(M.getNamedValue(OutlinedMF.getName())));

  // Add symbol just after the jump that will be used as a return address
  // from the outlined function.
  MCSymbol *RetSym =
      MF.getContext().createTempSymbol("OUTLINED_FUNCTION_RET", true);
  It->setPostInstrSymbol(MF, RetSym);

  // Add instruction to store return address onto the top of the stack.
  BuildMI(MBB, It, DL, get(SyncVM::ADDcrs_s))
      .addSym(RetSym)
      .addImm(0 /* RetSymOffset */)
      .addReg(SyncVM::R0)
      .addReg(SyncVM::SP)
      .addImm(0 /* AMBase2 */)
      .addImm(-1 /* StackOffset */)
      .addImm(SyncVMCC::COND_NONE);

  return It;
}
