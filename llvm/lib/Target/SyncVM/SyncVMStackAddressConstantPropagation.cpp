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
  
  // match an instruction pattern. If it matches, return the register that
  // are being scaled
  Register matchScalingBy32(MachineInstr &MI) const {
    if (MI.getOpcode() != SyncVM::MULirrr_s)
      return {};
    auto In0Const = SyncVM::in0Iterator(MI);
    if (getImmOrCImm(*In0Const) != 32)
      return {};
    auto In1Reg = SyncVM::in1Iterator(MI)->getReg();
    if (!In1Reg.isVirtual())
      return {};
    // do not check because it can be reused
    //if (!RegInfo->hasOneNonDBGUse(In1Reg))
    //  return {};
    return In1Reg;
  }
  
  Register isScaledBy32(Register Reg) const {
    if (!Reg.isVirtual()) return false;
    if (!RegInfo->hasOneDef(Reg)) return false;
    MachineInstr* DefMI = RegInfo->getVRegDef(Reg);
    if (matchScalingBy32(*DefMI))
      return SyncVM::in1Iterator(*DefMI)->getReg();
    else
      return {};
  }
  
  // TODO: refactor
  Register matchDescalingBy32(MachineInstr &MI) const {
    if (MI.getOpcode() != SyncVM::DIVxrrr_s)
      return {};
    auto In0Const = SyncVM::in0Iterator(MI);
    if (getImmOrCImm(*In0Const) != 32)
      return {};
    auto In1Reg = SyncVM::in1Iterator(MI)->getReg();
    if (!In1Reg.isVirtual())
      return {};
    if (!RegInfo->hasOneNonDBGUse(In1Reg))
      return {};
    return In1Reg;
  }

  // return the output result of ADDframe
  Register matchADDframe(MachineInstr &MI) const {
    if (MI.getOpcode() != SyncVM::ADDframe)
      return {};
    return MI.getOperand(0).getReg();
  }

  std::pair<Register, unsigned>
  matchAddWithMultipleOf32(MachineInstr &MI) const {
    if (MI.getOpcode() != SyncVM::ADDirr_s)
      return {{}, 0};
    auto In0Const = SyncVM::in0Iterator(MI);
    
    unsigned ConstAddValue = getImmOrCImm(*In0Const);
    if (ConstAddValue % 32 != 0)
      return {{}, 0};
    auto In1Reg = SyncVM::in1Iterator(MI)->getReg();
    if (!In1Reg.isVirtual())
      return {{}, 0};
    if (!RegInfo->hasOneNonDBGUse(In1Reg))
      return {{}, 0};
    return {In1Reg, ConstAddValue / 32};
  }

  MachineInstr* getDefOfRegister(Register Reg) const {
    if (!Reg.isVirtual() || !RegInfo->hasOneDef(Reg))
      return nullptr;
    return RegInfo->getVRegDef(Reg);
  }

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
  
  auto foldMulDiv = [&] (MachineInstr* MI) {
    Register DivReg = matchDescalingBy32(*MI);
    if (!DivReg) return false;
    Register ScaledAndDescaledReg = SyncVM::out0Iterator(*MI)->getReg();
    assert(ScaledAndDescaledReg);

    if (!RegInfo->hasOneNonDBGUse(DivReg))
      return false;
    
    MachineInstr* DivDefMI = RegInfo->getVRegDef(DivReg);
    Register FrameReg = matchADDframe(*DivDefMI);
    if (!FrameReg) return false;

    Register AddFrameReturnReg = DivDefMI->getOperand(0).getReg();
    RegInfo->replaceRegWith(ScaledAndDescaledReg, AddFrameReturnReg);

    MI->eraseFromParent();
    DivDefMI->setDesc(TII->get(SyncVM::ADDframeNoScaling));
    return true;
  };
  
  for (auto &BB : MF)
    for (auto II = BB.begin(); II != BB.end();) {
      MachineInstr &MI = *II;
      ++II;
      if (foldMulDiv(&MI)) {
        Changed = true;
      }
    }

  auto getStackAccess = [&](MachineInstr &MI) {
    if (SyncVM::hasSRInAddressingMode(MI)) {
      auto In0Reg = SyncVM::in0Iterator(MI) + 1;
      auto In0Const = SyncVM::in0Iterator(MI) + 2;
      if (In0Reg->isReg())
        return std::make_pair(In0Reg, In0Const);
    }
    // TODO: fix it
    if (MI.getOpcode() == SyncVM::ADDirs_s) {
      auto In0Reg = SyncVM::out0Iterator(MI) + 1;
      auto In0Const = SyncVM::out0Iterator(MI) + 2;
      if (In0Reg->isReg())
        return std::make_pair(In0Reg, In0Const);
    }
    return std::make_pair(MI.operands_end(), MI.operands_end());
  };

  for (auto &BB : MF)
    for (auto &MI : BB) {
      auto [In0Reg, In0Const] = getStackAccess(MI);
      if (In0Reg == MI.operands_end())
        continue;

      Register Base = In0Reg->getReg();
      MachineInstr *DivMI = RegInfo->getVRegDef(Base);
      
      // here we try to determine if it is the following pattern:
      // 
      //  mul     32, r2, r6, r0
      //  add     32, r6, r6
      //  DIV     32, r6, r2, r0
      //  STACKOP stack[r2], r1, r1
      Register DivReg = matchDescalingBy32(*DivMI);
      if (!DivReg) continue;
      MachineInstr* AddMI = getDefOfRegister(DivReg);
      if (!AddMI) continue;

      auto [ScaledBaseReg, ScalingFactor] = matchAddWithMultipleOf32(*AddMI);
      if (ScaledBaseReg) {
        Register UnscaledReg = isScaledBy32(ScaledBaseReg);
        MachineInstr* MulMI = RegInfo->getVRegDef(ScaledBaseReg);
        if (!UnscaledReg) continue;

        // now we can safely replace the register with the unscaled one, and add offset
        int64_t Displacement = getImmOrCImm(*In0Const);
        In0Const->ChangeToImmediate(Displacement + ScalingFactor, false); 
        In0Reg->ChangeToRegister(UnscaledReg, false);

        DivMI->eraseFromParent();
        AddMI->eraseFromParent();
        MulMI->eraseFromParent();

        Changed = true;
      } else if (Register UnscaledReg = matchScalingBy32(*AddMI)) {
        assert(false);
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
