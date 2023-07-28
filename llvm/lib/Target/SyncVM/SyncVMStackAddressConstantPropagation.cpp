//===------------ SyncVMStackAddressConstantPropagation.cpp ---------------===//
//
/// \file
/// This file contains a pass that attempts to extract contant part of a stack
/// address from the register, replacing (op reg) where reg = reg1 + C with
/// (op reg1 + C), thus utilizing reg + imm addressing mode.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include <optional>

#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "SyncVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-stack-address-constant-propagation"
#define SYNCVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME                         \
  "SyncVM stack address constant propagation"

STATISTIC(NumInstructionsErased, "Number of instructions erased");

namespace {

struct PropagationResult {
  Register Base;
  int64_t Displacement;
};
using MopIter = MachineInstr::mop_iterator;

class SyncVMStackAddressConstantPropagation : public MachineFunctionPass {
public:
  static char ID;
  SyncVMStackAddressConstantPropagation() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &Fn) override;

  StringRef getPassName() const override {
    return SYNCVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME;
  }

private:
  // If base for a stack address is in form x1 + x2 + ... + xn, propagate and
  // fold all constants that are in expression.
  // TODO: When FE start to produce LLVM arrays, it make sense to support
  // propagation through mul.
  std::optional<PropagationResult> tryPropagateConstant(MachineInstr &MI);

  // match an instruction pattern:
  // %dst, $r0 = MULirrr_s 32, %src
  // If it matches, return the register that are being scaled (%src)
  Register matchScalingBy32(MachineInstr &MI) const;

  // If an instruction takes a stack operand (either in or out), return the
  // iterators for that stack access (base and displacement iterators)
  std::optional<
      std::pair<MachineInstr::mop_iterator, MachineInstr::mop_iterator>>
  getStackAccess(MachineInstr &MI) const;

  // fold ADDframe with DIV into ADDframeNoScaling, which does not do cells to
  // bytes conversion
  bool foldAddFrame(MachineInstr* MI) const;


  // match pattern: %dst, $r0 = MULirrr_s 32, %src, where %dst is the input reg
  // if it matches, return the register that are being scaled (%src)
  Register isScaledBy32(Register Reg) const;
  
  // match an instruction pattern:
  // %dst, $r0 = DIVxrrr_s 32, %src 
  // If it matches, return the register that are being scaled (%src)
  Register matchDescalingBy32(MachineInstr &MI) const;

  // match and return the output result of ADDframe
  Register matchADDframe(MachineInstr &MI) const;

  // if match a = %reg + 32*b, return the %reg and the constant b
  std::optional<std::pair<Register, unsigned>>
  matchAddWithMultipleOf32(MachineInstr &MI) const;

  MachineInstr* getDefOfRegister(Register Reg) const;

  const SyncVMInstrInfo *TII;
  MachineRegisterInfo *RegInfo;
};

char SyncVMStackAddressConstantPropagation::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMStackAddressConstantPropagation, DEBUG_TYPE,
                SYNCVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME, false, false)

// match an instruction pattern:
// %dst, $r0 = MULirrr_s 32, %src
// If it matches, return the register that are being scaled (%src)
Register SyncVMStackAddressConstantPropagation::matchScalingBy32(
    MachineInstr &MI) const {
  if (MI.getOpcode() != SyncVM::MULirrr_s)
    return {};
  auto In0Const = SyncVM::in0Iterator(MI);
  auto In1Reg = SyncVM::in1Iterator(MI)->getReg();

  if (getImmOrCImm(*In0Const) != 32 ||
      SyncVM::out1Iterator(MI)->getReg() != SyncVM::R0 || !In1Reg.isVirtual())
    return {};
  return In1Reg;
}

Register
SyncVMStackAddressConstantPropagation::isScaledBy32(Register Reg) const {
  if (!Reg.isVirtual() || !RegInfo->hasOneDef(Reg))
    return false;
  MachineInstr *DefMI = RegInfo->getVRegDef(Reg);
  if (matchScalingBy32(*DefMI))
    return SyncVM::in1Iterator(*DefMI)->getReg();
  else
    return {};
}

Register SyncVMStackAddressConstantPropagation::matchDescalingBy32(
    MachineInstr &MI) const {
  if (MI.getOpcode() != SyncVM::DIVxrrr_s ||
      getImmOrCImm(*SyncVM::in0Iterator(MI)) != 32)
    return {};
  auto In1Reg = SyncVM::in1Iterator(MI)->getReg();
  if (!In1Reg.isVirtual() || SyncVM::out1Iterator(MI)->getReg() != SyncVM::R0 ||
      !RegInfo->hasOneNonDBGUse(In1Reg))
    return {};
  return In1Reg;
}

// if match a = %reg + 32*b, return the %reg and the constant b
std::optional<std::pair<Register, unsigned>>
SyncVMStackAddressConstantPropagation::matchAddWithMultipleOf32(
    MachineInstr &MI) const {
  if (MI.getOpcode() != SyncVM::ADDirr_s)
    return std::nullopt;
  auto In0Const = SyncVM::in0Iterator(MI);

  unsigned ConstAddValue = getImmOrCImm(*In0Const);
  if (ConstAddValue % 32 != 0)
    return std::nullopt;
  auto In1Reg = SyncVM::in1Iterator(MI)->getReg();
  if (!In1Reg.isVirtual())
    return std::nullopt;
  if (!RegInfo->hasOneNonDBGUse(In1Reg))
    return std::nullopt;
  return std::make_pair(In1Reg, ConstAddValue / 32);
}

MachineInstr *
SyncVMStackAddressConstantPropagation::getDefOfRegister(Register Reg) const {
  if (!Reg.isVirtual() || !RegInfo->hasOneDef(Reg))
    return nullptr;
  return RegInfo->getVRegDef(Reg);
}

Register
SyncVMStackAddressConstantPropagation::matchADDframe(MachineInstr &MI) const {
  if (MI.getOpcode() != SyncVM::ADDframe)
    return {};
  return SyncVM::out0Iterator(MI)->getReg();
}

std::optional<PropagationResult>
SyncVMStackAddressConstantPropagation::tryPropagateConstant(MachineInstr &MI) {
  if (!TII->isAdd(MI) || !TII->isSilent(MI) || MI.mayStore() || MI.mayLoad())
    return {};

  // If the result of the operation is used more than once, don't extract a
  // constant from it.
  if (!RegInfo->hasOneNonDBGUse(SyncVM::out0Iterator(MI)->getReg()))
    return {};

  auto In0 = SyncVM::in0Iterator(MI);
  auto In1 = SyncVM::in1Iterator(MI);
  Register In1Reg = In1->getReg();
  MachineInstr &In1Def = *RegInfo->getVRegDef(In1Reg);
  auto In1Res = tryPropagateConstant(In1Def);

  // If Base = VR1 + VR2, try to propagate constant from VR1 and VR2
  // recursively.
  if (SyncVM::hasRRInAddressingMode(MI)) {
    Register In0Reg = In0->getReg();
    MachineInstr &LHS = *RegInfo->getVRegDef(In0Reg);
    auto LHSRes = tryPropagateConstant(LHS);
    if (!LHSRes && In1Res)
      return {};
    In0Reg = LHSRes ? LHSRes->Base : In0Reg;
    In1Reg = In1Res ? In1Res->Base : In1Reg;
    int64_t In0Const = LHSRes ? LHSRes->Displacement : 0;
    int64_t In1Const = In1Res ? In1Res->Displacement : 0;
    Register NewVR = RegInfo->createVirtualRegister(&SyncVM::GR256RegClass);
    MachineInstr *NewMI = BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
                                  TII->get(SyncVM::ADDrrr_s))
                              .addDef(NewVR)
                              .addReg(In0Reg)
                              .addReg(In1Reg)
                              .addImm(SyncVMCC::COND_NONE)
                              .getInstr();
    LLVM_DEBUG(dbgs() << "Replace " << MI << "\n  with " << NewMI);
    ++NumInstructionsErased;
    MI.eraseFromParent();
    return PropagationResult{NewVR, In0Const + In1Const};
  }
  // If Base = VR + Disp, try to propagate const from VR and use Disp as Imm in
  // Reg + Imm addressing mode.
  assert(SyncVM::hasIRInAddressingMode(MI));
  In1Reg = In1Res ? In1Res->Base : In1Reg;
  unsigned Displacement =
      getImmOrCImm(*In0) + (In1Res ? In1Res->Displacement : 0);
  LLVM_DEBUG(dbgs() << "Erase " << MI);
  ++NumInstructionsErased;
  MI.eraseFromParent();
  return PropagationResult{In1Reg, Displacement};
}

std::optional<std::pair<MachineInstr::mop_iterator, MachineInstr::mop_iterator>>
SyncVMStackAddressConstantPropagation::getStackAccess(MachineInstr &MI) const {
  // check if the stack access is in input operands
  if (SyncVM::hasSRInAddressingMode(MI)) {
    auto In0Reg = SyncVM::in0Iterator(MI) + 1;
    auto In0Const = SyncVM::in0Iterator(MI) + 2;
    if (In0Reg->isReg())
      return std::make_pair(In0Reg, In0Const);
  }

  // check if the stack access is in output operands
  if (SyncVM::hasSROutAddressingMode(MI)) {
    auto In0Reg = SyncVM::out0Iterator(MI) + 1;
    auto In0Const = SyncVM::out0Iterator(MI) + 2;
    if (In0Reg->isReg())
      return std::make_pair(In0Reg, In0Const);
  }
  return {};
}

bool SyncVMStackAddressConstantPropagation::foldAddFrame(
    MachineInstr *MI) const {
  Register DivReg = matchDescalingBy32(*MI);
  if (!DivReg)
    return false;
  Register ScaledAndDescaledReg = SyncVM::out0Iterator(*MI)->getReg();
  assert(ScaledAndDescaledReg);

  if (!RegInfo->hasOneNonDBGUse(DivReg))
    return false;

  MachineInstr *DivDefMI = RegInfo->getVRegDef(DivReg);
  assert(DivDefMI);
  Register FrameReg = matchADDframe(*DivDefMI);
  if (!FrameReg)
    return false;

  Register AddFrameReturnReg = SyncVM::out0Iterator(*DivDefMI)->getReg();
  RegInfo->replaceRegWith(ScaledAndDescaledReg, AddFrameReturnReg);

  MI->eraseFromParent();
  DivDefMI->setDesc(TII->get(SyncVM::ADDframeNoScaling));
  // actually, we remove two instructions because ADDframeNoScaling is 1
  // instruction shorter than ADDframe
  ++NumInstructionsErased;
  return true;
}

bool SyncVMStackAddressConstantPropagation::runOnMachineFunction(
    MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** SyncVM convert bytes to cells **********\n"
                    << "********** Function: " << MF.getName() << '\n');
  RegInfo = &MF.getRegInfo();
  assert(RegInfo->isSSA() && "The pass is supposed to be run on SSA form MIR");

  bool Changed = false;
  TII =
      cast<SyncVMInstrInfo>(MF.getSubtarget<SyncVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  auto startPropagateConstant = [&](MachineInstr &MI) {
    auto StackIt = getStackAccess(MI);
    if (!StackIt)
      return false;
    auto [In0Reg, In0Const] = *StackIt;
    Register Base = In0Reg->getReg();
    if (!RegInfo->hasOneNonDBGUse(Base))
      return false;
    MachineInstr *DefMI = RegInfo->getVRegDef(Base);
    auto ConstPropagationResult = tryPropagateConstant(*DefMI);
    if (!ConstPropagationResult)
      return false;
    int64_t Displacement = getImmOrCImm(*In0Const);
    Displacement += ConstPropagationResult->Displacement;
    LLVM_DEBUG(dbgs() << "Replace " << MI);
    In0Reg->ChangeToRegister(ConstPropagationResult->Base, 0);
    In0Const->ChangeToImmediate(Displacement, 0);
    LLVM_DEBUG(dbgs() << "  with " << MI);
    return true;
  };
  

  auto foldStackArithmetic = [&](MachineInstr &MI) {
    auto StackIt = getStackAccess(MI);
    if (!StackIt)
      return false;
    auto [In0Reg, In0Const] = *StackIt;

    Register Base = In0Reg->getReg();
    MachineInstr *DivMI = RegInfo->getVRegDef(Base);
    // here we try to determine if it is the following pattern:
    // %r1, $r0 = MULirrr_s 32, %r0
    // %2 = ADDirr_s 32*Offset, %r1
    // %r3, $r0 = DIVxrrr_s 32, %r2
    // STACKOP(stack[%r3], ...)
    Register DivReg = matchDescalingBy32(*DivMI);
    if (!DivReg)
      return false;
    MachineInstr *AddMI = getDefOfRegister(DivReg);
    if (!AddMI)
      return false;

    auto MatchedResult = matchAddWithMultipleOf32(*AddMI);
    if (!MatchedResult)
      return false;
    auto [ScaledBaseReg, ScalingFactor] = *MatchedResult;
    Register UnscaledReg = isScaledBy32(ScaledBaseReg);
    if (!UnscaledReg)
      return false;
    MachineInstr *MulMI = RegInfo->getVRegDef(ScaledBaseReg);

    // now we can safely replace the register with the unscaled one, and add
    // offset
    int64_t Displacement = getImmOrCImm(*In0Const);
    In0Const->ChangeToImmediate(Displacement + ScalingFactor, false);
    In0Reg->ChangeToRegister(UnscaledReg, false);

    DivMI->eraseFromParent();
    AddMI->eraseFromParent();
    MulMI->eraseFromParent();
    NumInstructionsErased += 3;
    return true;
  };

  for (auto &BB : MF)
    for (auto II = BB.begin(); II != BB.end();) {
      MachineInstr &MI = *II;
      ++II;
      if (foldAddFrame(&MI)) {
        Changed = true;
        continue;
      }
      Changed |= startPropagateConstant(MI);
      Changed |= foldStackArithmetic(MI);
    }

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed;
}

/// createSyncVMBytesToCellsPass - returns an instance of bytes to cells
/// conversion pass.
FunctionPass *llvm::createSyncVMStackAddressConstantPropagationPass() {
  return new SyncVMStackAddressConstantPropagation();
}
