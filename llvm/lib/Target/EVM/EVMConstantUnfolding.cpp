//===----- EVMConstantUnfolding.cpp - Split Critical Edges -----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implement large constants unfolding. A large constant is replaced
// with a sequence of operations to reduce code size.
//
// For example:
//
//   0xFFFFFFFF00000000000000000000000000000000000000000000000000000000
//
// can be replaced with
//
//   0xFFFFFFFF << 224
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "evm-constant-unfolding"

namespace {
class EVMConstantUnfolding final : public MachineFunctionPass {
public:
  static char ID;

  EVMConstantUnfolding() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM constant unfolding"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  //  void getAnalysisUsage(AnalysisUsage &AU) const override {
  //    AU.setPreservesAll();
  //  }

private:
  MachineFunction *MF = nullptr;
  const TargetInstrInfo *TII = nullptr;

  void buildSub(MachineInstr &MI) {
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::SUB_S));
  }

  void buildShl(MachineInstr &MI) {
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::SHL_S));
  }

  void buildShr(MachineInstr &MI) {
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(EVM::SHR_S));
  }

  void buildImmValue(MachineInstr &MI, const APInt &Val);
  bool tryUnfoldConstant(MachineInstr &MI);
};
} // end anonymous namespace

char EVMConstantUnfolding::ID = 0;

INITIALIZE_PASS(EVMConstantUnfolding, DEBUG_TYPE, "Constant unfolding", false,
                false)

FunctionPass *llvm::createEVMConstantUnfolding() {
  return new EVMConstantUnfolding();
}

void EVMConstantUnfolding::buildImmValue(MachineInstr &MI, const APInt &Val) {
  unsigned PushOpc = EVM::getPUSHOpcode(Val);
  auto NewMI = BuildMI(*MI.getParent(), MI, MI.getDebugLoc(),
                       TII->get(EVM::getStackOpcode(PushOpc)));
  if (PushOpc != EVM::PUSH0)
    NewMI.addCImm(ConstantInt::get(MF->getFunction().getContext(), Val));
}

bool EVMConstantUnfolding::tryUnfoldConstant(MachineInstr &MI) {
  bool Changed = false;
  APInt Imm = MI.getOperand(0).getCImm()->getValue();
  if (Imm.isZero())
    return false;

  // Decompose an arbitrary immediate:
  //
  //   0x0000001...100000000
  //
  // into:
  //    - traling and leading zero bits
  //    - 'value' part, that starts and and ends with '1'
  //    - abs('value')
  //
  // The following transformations are considered, depending on their
  // cost-effectiveness:
  //   - ((0 - AbsVal) << shift_l) >> shift_r
  //   - Val << shift
  //
  unsigned Ones = Imm.popcount();
  unsigned TrailZ = Imm.countTrailingZeros();
  unsigned LeadZ = Imm.countLeadingZeros();
  APInt Val = Imm.extractBits(256 - TrailZ - LeadZ, TrailZ);
  unsigned ValLen = Val.getActiveBits();
  bool IsMask = ((Ones + LeadZ + TrailZ) == Imm.getBitWidth());
  APInt AbsVal = Val.abs();
  assert(ValLen == (256 - TrailZ - LeadZ));
  assert(Val.isNegative());

  if (!LeadZ && Ones > 4 * 8) {
    // 0xfffffe000000000
    // Costs:
    //   PUSH  AbsVal     // 1 + sizeof(AbsVal)
    //   PUSH0            // 1
    //   SUB              // 1
    //   PUSH1 shift      // 2
    //   SHL              // 1
    //                    ---------
    //                    6 + sizeof(AbsVal)
    //
    buildImmValue(MI, AbsVal.zext(256));
    buildImmValue(MI, APInt::getZero(256));
    buildSub(MI);
    buildImmValue(MI, APInt(256, 256 - ValLen));
    buildShl(MI);
    Changed = true;
  } else if (IsMask && !TrailZ && Ones > 6 * 8) {
    assert(AbsVal.isOne());
    // 0x0000000000ffffffffffff
    // Costs:
    //   PUSH  1          // 2
    //   PUSH0            // 1
    //   SUB              // 1
    //   PUSH1 shift      // 2
    //   SHR              // 1
    //                    ---------
    //                    7
    //
    buildImmValue(MI, APInt(256, 1));
    buildImmValue(MI, APInt::getZero(256));
    buildSub(MI);
    buildImmValue(MI, APInt(256, 256 - ValLen));
    buildShr(MI);
    Changed = true;
  } else if (ValLen > (AbsVal.getActiveBits() + 8 * 8)) {
    // 0x0000000000fffffffffffe000000000000
    // Costs:
    //   PUSH AbsVal      // 1 + sizeof(AbsVal)
    //   PUSH0            // 1
    //   SUB              // 1
    //   PUSH1 shiftl     // 2
    //   SHL              // 1
    //   PUSH1 shiftr     // 2
    //   SHR              // 1
    //                    ---------
    //                    9 + size(AbsVal)
    //
    buildImmValue(MI, AbsVal);
    buildImmValue(MI, APInt::getZero(256));
    buildSub(MI);
    buildImmValue(MI, APInt(256, 256 - ValLen));
    buildShl(MI);
    buildImmValue(MI, APInt(256, LeadZ));
    buildShr(MI);
    Changed = true;
  } else if (TrailZ > 3 * 8) {
    // Costs:
    //   PUSH  ShiftedVal  // 1 + sizeof(ShiftedVal)
    //   PUSH1 Shift       // 2
    //   SHL               // 1
    //                     ---------
    //                     4 + sizeof(ShiftedVal)
    //
    buildImmValue(MI, Imm.lshr(TrailZ));
    buildImmValue(MI, APInt(256, TrailZ));
    buildShl(MI);
    Changed = true;
  }
  if (Changed)
    MI.eraseFromParent();

  return Changed;
}

bool static isPUSH(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case EVM::PUSH1_S:
  case EVM::PUSH2_S:
  case EVM::PUSH3_S:
  case EVM::PUSH4_S:
  case EVM::PUSH5_S:
  case EVM::PUSH6_S:
  case EVM::PUSH7_S:
  case EVM::PUSH8_S:
  case EVM::PUSH9_S:
  case EVM::PUSH10_S:
  case EVM::PUSH11_S:
  case EVM::PUSH12_S:
  case EVM::PUSH13_S:
  case EVM::PUSH14_S:
  case EVM::PUSH15_S:
  case EVM::PUSH16_S:
  case EVM::PUSH17_S:
  case EVM::PUSH18_S:
  case EVM::PUSH19_S:
  case EVM::PUSH20_S:
  case EVM::PUSH21_S:
  case EVM::PUSH22_S:
  case EVM::PUSH23_S:
  case EVM::PUSH24_S:
  case EVM::PUSH25_S:
  case EVM::PUSH26_S:
  case EVM::PUSH27_S:
  case EVM::PUSH28_S:
  case EVM::PUSH29_S:
  case EVM::PUSH30_S:
  case EVM::PUSH31_S:
  case EVM::PUSH32_S:
    return true;
  default:
    return false;
  }
}

bool EVMConstantUnfolding::runOnMachineFunction(MachineFunction &Mf) {
  MF = &Mf;
  bool Changed = false;
  LLVM_DEBUG({
    dbgs() << "********** Constant unfolding **********\n"
           << "********** Function: " << Mf.getName() << '\n';
  });

  TII = MF->getSubtarget().getInstrInfo();
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      if (!isPUSH(MI))
        continue;

      Changed |= tryUnfoldConstant(MI);
    }
  }
  return Changed;
}
