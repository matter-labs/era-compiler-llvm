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
  const SyncVMInstrInfo *TII;
  MachineRegisterInfo *RegInfo;
};

char SyncVMStackAddressConstantPropagation::ID = 0;

} // namespace

INITIALIZE_PASS(SyncVMStackAddressConstantPropagation, DEBUG_TYPE,
                SYNCVM_STACK_ADDRESS_CONSTANT_PROPAGATION_NAME, false, false)

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

  for (auto &BB : MF) {
    for (auto &MI : BB) {
      if (!SyncVM::hasSRInAddressingMode(MI))
        continue;
      auto In0Reg = SyncVM::in0Iterator(MI) + 1;
      auto In0Const = SyncVM::in0Iterator(MI) + 2;
      if (!In0Reg->isReg())
        continue;
      Register Base = In0Reg->getReg();
      if (!RegInfo->hasOneNonDBGUse(Base))
        continue;
      MachineInstr *DefMI = RegInfo->getVRegDef(Base);
      auto ConstPropagationResult = tryPropagateConstant(*DefMI);
      if (!ConstPropagationResult)
        continue;
      int64_t Displacement = getImmOrCImm(*In0Const);
      Displacement += ConstPropagationResult->Displacement;
      LLVM_DEBUG(dbgs() << "Replace " << MI);
      In0Reg->ChangeToRegister(ConstPropagationResult->Base, 0);
      In0Const->ChangeToImmediate(Displacement, 0);
      LLVM_DEBUG(dbgs() << "  with " << MI);
      Changed = true;
    }
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
