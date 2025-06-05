//===----- EVMConstantUnfolding.cpp - Constant unfolding -------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implement large constants unfolding.
//
// Large constants are broken into sequences of operations
// to reduce bytecode size. For example,
//
//   PUSH32 0xFFFFFFFF00000000000000000000000000000000000000000000000000000000
//   (33 bytes)
//
// can be replaced with
//
//   PUSH4  0xFFFFFFFF
//   PUSH1  0xE0
//   SHL
//   (8 bytes)
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
#include <functional>

using namespace llvm;

#define DEBUG_TYPE "evm-constant-unfolding"

namespace {
class EVMConstantUnfolding final : public MachineFunctionPass {
public:
  static char ID;

  EVMConstantUnfolding() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM constant unfolding"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

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

  void buildImm(MachineInstr &MI, const APInt &Val);

  bool tryUnfoldConstant(MachineInstr &MI);
};
} // end anonymous namespace

char EVMConstantUnfolding::ID = 0;

INITIALIZE_PASS(EVMConstantUnfolding, DEBUG_TYPE, "Constant unfolding", false,
                false)

FunctionPass *llvm::createEVMConstantUnfolding() {
  return new EVMConstantUnfolding();
}

void EVMConstantUnfolding::buildImm(MachineInstr &MI, const APInt &Val) {
  unsigned PushOpc = EVM::getPUSHOpcode(Val);
  auto NewMI = BuildMI(*MI.getParent(), MI, MI.getDebugLoc(),
                       TII->get(EVM::getStackOpcode(PushOpc)));
  if (PushOpc != EVM::PUSH0)
    NewMI.addCImm(ConstantInt::get(MF->getFunction().getContext(), Val));
}

bool EVMConstantUnfolding::tryUnfoldConstant(MachineInstr &MI) {
  const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
  // It’s not practical to check small constants, since the default
  // instructions are cheapest in those cases.
  if (Imm.getActiveBits() <= 5 * 8)
    return false;

  // Decompose a given immediate operand of the form:
  //
  //   0x0000001...100000000
  //
  // into:
  //    - trailing and leading zero bits
  //    - 'Val' part, that starts and ends with '1'
  //    - abs('Val')
  //
  // The following transformations are considered, depending on their
  // cost-effectiveness:
  //   - ((0 - AbsVal) << shift_l) >> shift_r
  //   - Val << shift
  //   - Default, i.e., leave Imm as is
  //
  unsigned Ones = Imm.popcount();
  unsigned TrailZ = Imm.countTrailingZeros();
  unsigned LeadZ = Imm.countLeadingZeros();
  APInt Val = Imm.extractBits(Imm.getBitWidth() - TrailZ - LeadZ, TrailZ);
  unsigned ValLen = Val.getActiveBits();
  APInt AbsVal = Val.abs();
  unsigned AbsValLen = AbsVal.getActiveBits();
  bool IsMask = ((Ones + LeadZ + TrailZ) == Imm.getBitWidth());
  assert(ValLen == (Imm.getBitWidth() - TrailZ - LeadZ));
  assert(Val.isNegative());

  // The upper limit for both size and instruction count.
  constexpr unsigned MaxCost = /* PUSH32 size*/ 33 * 2;
  SmallVector<std::pair<std::function<std::pair<unsigned, unsigned>(void)>,
                        std::function<bool(void)>>>
      Transformations = {
          {// Transformation 1
           //
           // 0x0000000000fffffffffffe000000000000
           //
           // Cost:
           //   PUSH AbsVal     // 1 + AbsValLen
           //   PUSH0           // 1
           //   SUB             // 1
           //   -------------------- // Optional
           //   PUSH1 shiftl    // 2
           //   SHL             // 1
           //   -------------------- // Optional
           //   PUSH1 shiftr    // 2
           //   SHR             // 1
           //                   ---------
           //                   [4 ... 9] + AbsValLen
           //
           [ValLen, AbsValLen, LeadZ]() {
             unsigned AbsValLenBytes = alignTo(AbsValLen, 8) / 8;
             unsigned Size = 3 + AbsValLenBytes;
             unsigned OpNum = 3;
             if (ValLen != 256) {
               Size += 3; // PUSH + SHL
               OpNum += 2;
             }
             if (LeadZ) {
               Size += 3; // PUSH + SHR
               OpNum += 2;
             }
             return std::pair<unsigned, unsigned>(Size, OpNum);
           },
           [&MI, &AbsVal, ValLen, LeadZ, this]() {
             buildImm(MI, AbsVal);
             buildImm(MI, APInt::getZero(256));
             buildSub(MI);
             if (ValLen != 256) {
               buildImm(MI, APInt(256, 256 - ValLen));
               buildShl(MI);
             }

             if (LeadZ) {
               buildImm(MI, APInt(256, LeadZ));
               buildShr(MI);
             }
             return true;
           }},
          {// Transformation 2
           //
           // Mask shifted to the right.
           //
           // 0x0000000000ffffffffffff
           //
           // Cost:
           //   PUSH1 1          // 2
           //   PUSH0            // 1
           //   SUB              // 1
           //   PUSH1 shift      // 2
           //   SHR              // 1
           //                    ---------
           //                    7
           [IsMask, TrailZ]() {
             unsigned Size = (IsMask && !TrailZ) ? 7 : MaxCost;
             return std::pair<unsigned, unsigned>(Size, 5);
           },
           [&MI, ValLen, this]() {
             buildImm(MI, APInt(256, 1));
             buildImm(MI, APInt::getZero(256));
             buildSub(MI);
             buildImm(MI, APInt(256, 256 - ValLen));
             buildShr(MI);
             assert(ValLen != 256);
             return true;
           }},
          {// Transformation 3
           //
           // 0x0000000000abcde000000000000
           //
           // Cost:
           //   PUSH  Val        // 1 + ValLen
           //   PUSH1 shift      // 2
           //   SHL              // 1
           //                    ---------
           //                    4 + ValLen
           //
           [ValLen, TrailZ]() {
             unsigned ValLenBytes = alignTo(ValLen, 8) / 8;
             unsigned Size = 1 + ValLenBytes;
             unsigned OpNum = 1;
             if (TrailZ) {
               Size += 3; // PUSH + SHL
               OpNum += 2;
             }
             return std::pair<unsigned, unsigned>(Size, OpNum);
           },
           [&MI, Val, TrailZ, this]() {
             buildImm(MI, Val);
             if (TrailZ) {
               buildImm(MI, APInt(256, TrailZ));
               buildShl(MI);
             }
             return true;
           }},
          {// Default case
           //
           // Cost:
           //   PUSH  Imm        // 1 + Imm
           //                    ---------
           //                    1 + Imm
           //
           [Imm]() {
             unsigned ImmLenBytes = alignTo(Imm.getActiveBits(), 8) / 8;
             return std::pair<unsigned, unsigned>(1 + ImmLenBytes, 1);
           },
           []() { return false; }}};

  bool OptForSize =
      MF->getFunction().hasFnAttribute(Attribute::OptimizeForSize);

  auto *OptTransform = std::min_element(
      Transformations.begin(), Transformations.end(),
      [OptForSize](const auto &TrA, const auto &TrB) {
        std::pair<unsigned, unsigned> CostA = TrA.first();
        std::pair<unsigned, unsigned> CostB = TrB.first();
        // When optimizing for size, our primary goal is
        // to minimize the total size of the instructions.
        if (OptForSize) {
          // First check, if sizes differ.
          if (CostA.first != CostB.first)
            return CostA.first < CostB.first;

          // Then the number of operations.
          return CostA.second < CostB.second;
        }
        // Optimizing for speed.
        // Expressing total cost in terms of both size and number of operations
        // enables more performance-friendly transformation choices. For
        // example, consider the following two options:
        //
        //   [size: 7, OpNum: 5]
        // vs
        //   [size: 8, OpNum: 3]
        //
        // The second option is likely preferable, as the operation count is
        // notably lower while the size remains nearly the same.
        return (CostA.first + CostA.second) < (CostB.first + CostB.second);
      });

  std::pair<unsigned, unsigned> Cost = OptTransform->first();

#ifndef NDEBUG
  for (const auto &Tr : Transformations) {
    std::pair<unsigned, unsigned> Cost = Tr.first();
    LLVM_DEBUG({
      dbgs() << "Transform cost: [size: " << Cost.first
             << ", OpNum: " << Cost.second << "]\n";
    });
  }
#endif // NDEBUG

  bool Changed = false;
  if (OptForSize) {
    Changed = OptTransform->second();
    LLVM_DEBUG({
      dbgs() << "Applying optimal transform : [size: " << Cost.first
             << ", OpNum: " << Cost.second << "]\n";
    });
  } else {
    unsigned OrigSize = (alignTo(Imm.getActiveBits(), 8) / 8) + 1;
    // When optimizing for speed, only unfold constants if it reduces the size
    // by a factor of at least 3 and keeps the instruction count to 3 or fewer.
    // This seems to be an effective heuristic that avoids significantly
    // impacting performance.
    if (OrigSize / Cost.first >= 3 && Cost.second <= 3) {
      Changed = OptTransform->second();
      LLVM_DEBUG({
        dbgs() << "Applying optimal transform : [size: " << Cost.first
               << ", OpNum: " << Cost.second << "]\n";
      });
    }
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
    dbgs() << "\n********** Constant unfolding **********\n"
           << "********** Function: " << Mf.getName() << '\n';
  });

  TII = MF->getSubtarget().getInstrInfo();
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      if (!isPUSH(MI))
        continue;

      LLVM_DEBUG({ dbgs() << "Checking for unfolding: " << MI; });

      Changed |= tryUnfoldConstant(MI);
    }
  }
  return Changed;
}
