//===-- EraVMExpandSelect.cpp - Expand select instructions ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands SEL pseudo instructions into target
// instructions.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ReachingDefAnalysis.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-expand-select"
#define ERAVM_EXPAND_SELECT_NAME "EraVM expand select pseudo instructions"

namespace {

/// Lower SEL instruction family to uncoditional + conditional move.
/// Select x, y, cc -> add x, r0 + add.cc y, r0.
class EraVMExpandSelect : public MachineFunctionPass {
public:
  static char ID;
  EraVMExpandSelect() : MachineFunctionPass(ID) {
    initializeEraVMExpandSelectPass(*PassRegistry::getPassRegistry());
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return ERAVM_EXPAND_SELECT_NAME; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<ReachingDefAnalysis>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  const EraVMInstrInfo *TII{};
  ReachingDefAnalysis *RDA{};
  std::vector<MachineInstr *> findSelectAdd(MachineFunction &MF);
};

char EraVMExpandSelect::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(EraVMExpandSelect, DEBUG_TYPE, ERAVM_EXPAND_SELECT_NAME, false,
                false)
INITIALIZE_PASS_DEPENDENCY(ReachingDefAnalysis)
INITIALIZE_PASS_END(EraVMExpandSelect, DEBUG_TYPE, ERAVM_EXPAND_SELECT_NAME, false,
                false)

/// For given \p Select and argument \p Kind return corresponding mov opcode
/// for conditional or unconditional mov.
static unsigned movOpcode(EraVM::ArgumentKind Kind, unsigned Select) {
  switch (EraVM::argumentType(Kind, Select)) {
  case EraVM::ArgumentType::Register:
    if (Select == EraVM::FATPTR_SELrrr)
      return EraVM::PTR_ADDrrr_s;
    return EraVM::ADDrrr_s;
  case EraVM::ArgumentType::Immediate:
    return EraVM::ADDirr_s;
  case EraVM::ArgumentType::Code:
    return EraVM::ADDcrr_s;
  case EraVM::ArgumentType::Stack:
    return EraVM::ADDsrr_s;
  default:
    break;
  }
  llvm_unreachable("Unexpected argument type");
}

std::vector<MachineInstr *> EraVMExpandSelect::findSelectAdd(MachineFunction &MF) {
  std::vector<MachineInstr *> Result;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == EraVM::SELiir && EraVM::in0Iterator(MI)->isCImm() && EraVM::in0Iterator(MI)->getCImm()->isOne()
          && EraVM::in1Iterator(MI)->isCImm() && EraVM::in1Iterator(MI)->getCImm()->isZero()) {
        const Register Def = MI.getOperand(0).getReg();
        SmallPtrSet<MachineInstr *, 4> Uses;
        RDA->getGlobalUses(&MI, Def, Uses);
        if (Uses.size() == 1 && std::next(MI.getIterator()) != MI.getParent()->end() && &*std::next(MI.getIterator()) == *Uses.begin() &&
            TII->isAdd(**Uses.begin()) && EraVM::hasRRInAddressingMode(**Uses.begin())
            &&  EraVM::in0Iterator(**Uses.begin())->getReg() !=  EraVM::in1Iterator(**Uses.begin())->getReg()
            && getImmOrCImm(*EraVM::ccIterator(**Uses.begin())) == EraVMCC::COND_NONE)

          Result.push_back(&MI);
      }
    }
  return Result;
}

bool EraVMExpandSelect::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND SELECT INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');
  bool Changed = false;

  DenseMap<unsigned, unsigned> Inverse{
      {EraVM::SELrrr, EraVM::SELrrr},
      {EraVM::SELrir, EraVM::SELirr},
      {EraVM::SELrcr, EraVM::SELcrr},
      {EraVM::SELrsr, EraVM::SELsrr},
  };

  DenseMap<unsigned, unsigned> InverseCond{
      {EraVMCC::COND_E, EraVMCC::COND_NE},
      {EraVMCC::COND_NE, EraVMCC::COND_E},
      {EraVMCC::COND_LT, EraVMCC::COND_GE},
      {EraVMCC::COND_LE, EraVMCC::COND_GT},
      {EraVMCC::COND_GT, EraVMCC::COND_LE},
      {EraVMCC::COND_GE, EraVMCC::COND_LT},
      // COND_OF is an alias for COND_LT
      {EraVMCC::COND_OF, EraVMCC::COND_GE},
  };

  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");
  RDA = &getAnalysis<ReachingDefAnalysis>();
  std::vector<MachineInstr*> ToCombine = findSelectAdd(MF);
  std::vector<MachineInstr *> PseudoInst;
  for (MachineInstr *Select : ToCombine) {
    MachineInstr *Add = &*std::next(Select->getIterator());
    Register Def = Select->getOperand(0).getReg();
    DebugLoc DL = Select->getDebugLoc();
    MachineBasicBlock &MBB = *Select->getParent();
    PseudoInst.push_back(Select);
    PseudoInst.push_back(Add);
    auto NewInst = EraVM::hasRROutAddressingMode(*Add) ?
      BuildMI(MBB, Select, DL, TII->get(EraVM::getWithIRInAddrMode(Add->getOpcode())), EraVM::out0Iterator(*Add)->getReg()) 
      : BuildMI(MBB, Select, DL, TII->get(EraVM::getWithIRInAddrMode(Add->getOpcode())));
     NewInst.addCImm(EraVM::in0Iterator(*Select)->getCImm())
      .addReg((EraVM::in0Iterator(*Add)->getReg()) == Def ? EraVM::in1Iterator(*Add)->getReg() : EraVM::in0Iterator(*Add)->getReg());
     if (!EraVM::hasRROutAddressingMode(*Add))
       EraVM::copyOperands(NewInst, EraVM::out0Range(*Add));
     NewInst.add(*EraVM::ccIterator(*Select));
  }

  Changed = !PseudoInst.empty();
  for (auto *I : PseudoInst)
    I->eraseFromParent();
  PseudoInst.clear();

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (!EraVM::isSelect(MI))
        continue;

      unsigned Opc = MI.getOpcode();
      DebugLoc DL = MI.getDebugLoc();
      auto *In0 = EraVM::in0Iterator(MI);
      auto In0Range = EraVM::in0Range(MI);
      auto In1Range = EraVM::in1Range(MI);
      auto *Out = EraVM::out0Iterator(MI);
      auto CCVal = getImmOrCImm(*EraVM::ccIterator(MI));


      // For rN = cc ? rN : y it's profitable to reverse (rN = reverse_cc ? y :
      // rN) It allows to lower select to a single instruction rN =
      // add.reverse_cc y, r0.
      bool ShouldInverse =
          Inverse.count(Opc) != 0U && Out->getReg() == In0->getReg();

      auto buildMOV = [&](EraVM::ArgumentKind OpNo, unsigned CC) {
        auto OperandRange =
            (OpNo == EraVM::ArgumentKind::In0) ? In0Range : In1Range;
        auto *OperandIt = OperandRange.begin();
        bool IsRegister =
            argumentType(OpNo, MI) == EraVM::ArgumentType::Register;
        unsigned MovOpc = movOpcode(OpNo, Opc);
        // Avoid unconditional mov rN, rN
        if (CC == EraVMCC::COND_NONE && IsRegister &&
            OperandIt->getReg() == Out->getReg())
          return;
        auto Mov = BuildMI(MBB, &MI, DL, TII->get(MovOpc), Out->getReg());
        EraVM::copyOperands(Mov, OperandRange);
        Mov.addReg(EraVM::R0);
        Mov.addImm(CC);
        if (CC != EraVMCC::COND_NONE)
          Mov.addReg(EraVM::Flags, RegState::Implicit);
        return;
      };

      if (ShouldInverse) {
        buildMOV(EraVM::ArgumentKind::In0, EraVMCC::COND_NONE);
        buildMOV(EraVM::ArgumentKind::In1, InverseCond[CCVal]);
      } else {
        buildMOV(EraVM::ArgumentKind::In1, EraVMCC::COND_NONE);
        buildMOV(EraVM::ArgumentKind::In0, CCVal);
      }

      PseudoInst.push_back(&MI);
    }

  for (auto *I : PseudoInst)
    I->eraseFromParent();

  LLVM_DEBUG(
      dbgs() << "*******************************************************\n");
  return Changed || !PseudoInst.empty();
}

/// createEraVMExpandPseudoPass - returns an instance of the pseudo instruction
/// expansion pass.
FunctionPass *llvm::createEraVMExpandSelectPass() {
  return new EraVMExpandSelect();
}
