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
#include "EVMInstrInfo.h"
#include "EVMMachineFunctionInfo.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <functional>

using namespace llvm;

#define DEBUG_TYPE "evm-constant-unfolding"

STATISTIC(NumOfUnfoldings, "Number of unfolded constants");

// The initial values of the following options were determined experimentally
// to allow constant unfolding in non-OptForSize mode without noticeably
// impacting performance.
static cl::opt<unsigned>
    SizeGainThreshold("evm-const-unfolding-size-gain-threshold", cl::Hidden,
                      cl::init(3),
                      cl::desc("Minimum original-to-unfolded size ratio"
                               "required for constant unfolding when"
                               "optimizing for speed"));

static cl::opt<unsigned>
    InstrNumLimitUnfolInto("evm-const-unfolding-inst-num-limit", cl::Hidden,
                           cl::init(3),
                           cl::desc("Maximum number of instructions an original"
                                    "instruction can be unfolded into"));

namespace {
class EVMConstantUnfolding final : public MachineFunctionPass {
public:
  static char ID;

  EVMConstantUnfolding() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EVM constant unfolding"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  // A transformation function type
  using TransformFunction = std::function<bool(void)>;
  // A transformation cost function type that returns a tuple
  // <size, instr_count>, where size represents the total size of the
  // instruction sequence after constant unfolding, and instr_count is the
  // number of instructions in that sequence.
  using CostFunction = std::function<std::pair<unsigned, unsigned>(void)>;
  using Transformation = std::pair<CostFunction, TransformFunction>;

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
  bool OptForSize =
      MF->getFunction().hasFnAttribute(Attribute::OptimizeForSize);

  // Define the potential unfolding scenarios.

  // 1. A generic transformation that represents an immediate value as::
  //
  //    ((0 - AbsVal) << shift_l) >> shift_r
  //
  // For example,
  //
  //   0x00000000FFFFFFFFFFFFFF000000000000000000000000000000000000000000
  //
  // is represented as:
  //
  //   ((0 - 1) << 200) >> 32
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
  Transformation NegateAndLeftRightShift = {
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
      }};

  // 2. A transformation that represents an immediate value in the
  //    form of a right-shifted mask as:
  //
  //   ((0 - 1) >> shift_r
  //
  // For example,
  //
  //   0x0000000000000000000000000000000000000000000000000000FFFFFFFFFFFF
  //
  // is represented as:
  //
  //   ((0 - 1) >> 192
  //
  // Cost:
  //   PUSH1 1          // 2
  //   PUSH0            // 1
  //   SUB              // 1
  //   PUSH1 shift      // 2
  //   SHR              // 1
  //                    ---------
  //                    7
  //
  Transformation MinusOneAndRightShift = {
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
      }};

  // 3. A transformation that expresses an immediate value as a smaller
  //    bit-width value shifted to the left:
  //
  //   Val << shift
  //
  // For example,
  //
  //    0xFFFFFF0000000000000000000000000000000000000000000000000000000000
  //
  // is represented as:
  //
  //   0xFFFFFF << 192
  //
  // Cost:
  //   PUSH  Val        // 1 + ValLen
  //   PUSH1 shift      // 2
  //   SHL              // 1
  //                    ---------
  //                    4 + ValLen
  //
  Transformation SmallValueAndLeftShift = {
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
      }};

  // 4. No transformation, leave immediate as is.
  //
  // Cost:
  //   PUSH Imm         // 1 + ImmLen
  //                    ---------
  //                    1 + ImmLen
  //
  Transformation NoTranformation = {
      [Imm]() {
        unsigned ImmLenBytes = alignTo(Imm.getActiveBits(), 8) / 8;
        return std::pair<unsigned, unsigned>(1 + ImmLenBytes, 1);
      },
      []() { return false; }};

  // Group the transformations into an array and select the one with the
  // lowest cost.
  SmallVector<Transformation> Transformations = {
      NegateAndLeftRightShift, MinusOneAndRightShift, SmallValueAndLeftShift,
      NoTranformation};

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
    // by a factor of at least the 'SizeGainThreshold' and keeps the instruction
    // count to 'InstrNumLimitUnfolInto' or fewer.
    if (OrigSize / Cost.first >= SizeGainThreshold &&
        Cost.second <= InstrNumLimitUnfolInto) {
      Changed = OptTransform->second();
      LLVM_DEBUG({
        dbgs() << "Applying optimal transform : [size: " << Cost.first
               << ", OpNum: " << Cost.second << "]\n";
      });
    }
  }

  if (Changed) {
    MI.eraseFromParent();
    ++NumOfUnfoldings;
  }

  return Changed;
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
      // Skip PUSH0, since there's nothing to optimize in its case.
      if (!EVMInstrInfo::isPush(&MI) || MI.getOpcode() == EVM::PUSH0)
        continue;

      LLVM_DEBUG({ dbgs() << "Checking for unfolding: " << MI; });

      Changed |= tryUnfoldConstant(MI);
    }
  }
  return Changed;
}
