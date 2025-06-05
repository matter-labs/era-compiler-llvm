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
#include "llvm/CodeGen/MachineLoopInfo.h"
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
static cl::opt<float>
    SizeGainThreshold("evm-const-unfolding-size-gain-threshold", cl::Hidden,
                      cl::init(2.5),
                      cl::desc("Minimum original-to-unfolded size ratio"
                               "required for constant unfolding when"
                               "optimizing for speed"));

static cl::opt<unsigned>
    InstrNumLimitUnfolInto("evm-const-unfolding-inst-num-limit", cl::Hidden,
                           cl::init(3),
                           cl::desc("Maximum number of instructions an original"
                                    "instruction can be unfolded into"));

static cl::opt<unsigned> GasWeight("evm-const-unfolding-gas-weight", cl::Hidden,
                                   cl::init(2),
                                   cl::desc("Gas weight used as a factor in the"
                                            "transformation cost function"));

static cl::opt<unsigned>
    LoopDepthLimit("evm-const-loop-depth-limit", cl::Hidden, cl::init(2),
                   cl::desc("The maximum loop depth at which constant"
                            "unfolding is still considered beneficial"));

namespace {
class EVMConstantUnfolding final : public MachineFunctionPass {
public:
  static char ID;

  EVMConstantUnfolding() : MachineFunctionPass(ID) {}

private:
  // Estimates the execution cost of EVM-style stack operations.
  // Tracks instruction count, gas cost, and unfolded bytecode size.
  // It abstracts gas accounting for pushes and simple arithmetic/logical
  // operations.
  class StackCostModel {
  public:
    StackCostModel() = default;

    void push(const APInt &Val) {
      Gas += (Val.isZero() ? EVMCOST::PUSH0 : EVMCOST::PUSH);
      ByteSize += getPushSize(Val);
      ++InstrCount;
    }

    void shift() { accountInstr(EVMCOST::SHIFT); }

    void add() { accountInstr(EVMCOST::ADD); }

    void sub() { accountInstr(EVMCOST::SUB); }

    void bit_not() { accountInstr(EVMCOST::NOT); }

    unsigned getInstrCount() const { return InstrCount; }
    unsigned getByteSize() const { return ByteSize; }
    unsigned getGas() const { return Gas; }

    // Get the size of the PUSH instruction required for
    // the immediate value.
    static unsigned getPushSize(const APInt &Val) {
      return 1 + (alignTo(Val.getActiveBits(), 8) / 8);
    }

  private:
    void accountInstr(unsigned GasCost) {
      ++InstrCount;
      ++ByteSize;
      Gas += GasCost;
    }

    unsigned InstrCount = 0;
    unsigned ByteSize = 0;
    unsigned Gas = 0;
  };

  // Builds and applies a sequence of machine instructions required to
  // unfold a constant. Instruction generation is deferred using lambdas,
  // allowing the TransformationBuilder object to be reused for repeated
  // constants. As new instructions are added, the StackCostModel is used
  // to track the accumulated cost.
  class TransformationBuilder {
  public:
    TransformationBuilder(LLVMContext &Context, const TargetInstrInfo *TII)
        : Context(Context), TII(TII) {}

    void addSub() {
      CostModel.sub();
      BuildItems.push_back(
          [this](MachineInstr &MI) { insertInstr(MI, EVM::SUB_S); });
    }

    void addShl() {
      CostModel.shift();
      BuildItems.push_back(
          [this](MachineInstr &MI) { insertInstr(MI, EVM::SHL_S); });
    }

    void addShr() {
      CostModel.shift();
      BuildItems.push_back(
          [this](MachineInstr &MI) { insertInstr(MI, EVM::SHR_S); });
    }

    void addNot() {
      CostModel.bit_not();
      BuildItems.push_back(
          [this](MachineInstr &MI) { insertInstr(MI, EVM::NOT_S); });
    }

    void addImm(const APInt &Val) {
      CostModel.push(Val);
      BuildItems.push_back(
          [this, Val = Val](MachineInstr &MI) { buildImm(MI, Val); });
    }

    // Applies queued build instruction steps to replace a given instruction.
    void apply(MachineInstr &MI) const {
      for (const auto &func : BuildItems)
        func(MI);
    }

    const StackCostModel &getCost() const { return CostModel; }

  private:
    using BuildFunction = std::function<void(MachineInstr &)>;

    LLVMContext &Context;
    const TargetInstrInfo *TII;

    StackCostModel CostModel;
    SmallVector<BuildFunction, 16> BuildItems;

    void insertInstr(MachineInstr &MI, unsigned Opcode) {
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(Opcode));
    }

    void buildImm(MachineInstr &MI, const APInt &Val) {
      unsigned PushOpc = EVM::getPUSHOpcode(Val);
      auto NewMI = BuildMI(*MI.getParent(), MI, MI.getDebugLoc(),
                           TII->get(EVM::getStackOpcode(PushOpc)));
      if (PushOpc != EVM::PUSH0)
        NewMI.addCImm(ConstantInt::get(Context, Val));
    }
  };

  MachineFunction *MF = nullptr;
  const TargetInstrInfo *TII = nullptr;
  DenseMap<APInt, std::unique_ptr<TransformationBuilder>> TransformationCache;

  StringRef getPassName() const override { return "EVM constant unfolding"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  const TransformationBuilder *findOptimalTransfomtation(const APInt &Imm);

  bool tryUnfoldConstant(MachineInstr &MI);
};
} // end anonymous namespace

char EVMConstantUnfolding::ID = 0;

INITIALIZE_PASS_BEGIN(EVMConstantUnfolding, DEBUG_TYPE, "Constant unfolding",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(EVMConstantUnfolding, DEBUG_TYPE, "Constant unfolding",
                    false, false)

FunctionPass *llvm::createEVMConstantUnfolding() {
  return new EVMConstantUnfolding();
}

const EVMConstantUnfolding::TransformationBuilder *
EVMConstantUnfolding::findOptimalTransfomtation(const APInt &Imm) {
  if (auto It = TransformationCache.find(Imm);
      It != TransformationCache.end()) {
    LLVM_DEBUG({ dbgs() << " Retrieving transformation from the cache\n"; });
    return It->second.get();
  }

  // Decompose a given immediate operand of the form:
  //
  //   0x0000001...100000000
  //
  // into:
  //    - trailing and leading zero bits
  //    - 'Val' part, that starts and ends with '1'
  //    - abs('Val')
  //
  unsigned Ones = Imm.popcount();
  unsigned TrailZ = Imm.countTrailingZeros();
  unsigned LeadZ = Imm.countLeadingZeros();
  APInt Val = Imm.extractBits(Imm.getBitWidth() - TrailZ - LeadZ, TrailZ);
  unsigned ValLen = Val.getActiveBits();
  bool IsMask = ((Ones + LeadZ + TrailZ) == Imm.getBitWidth());
  assert(ValLen == (Imm.getBitWidth() - TrailZ - LeadZ));
  assert(Val.isNegative());

  bool OptForSize = MF->getFunction().hasOptSize();

  SmallVector<std::unique_ptr<TransformationBuilder>, 8> Transformations;

  // 1. A transformation that represents an immediate value as:
  //
  //    (~(AbsVal - 1) << shift_l) >> shift_r
  //
  // For example,
  //
  //   0x00000000FFFFFFFFFFFFFF000000000000000000000000000000000000000000
  //
  // is represented as:
  //
  //   (~0 << 200) >> 32
  //
  // Cost:
  //   PUSH AbsVal     // 1 + AbsValLen
  //   NOT             // 1
  //   PUSH1 shiftl    // 2
  //   SHR             // 1
  //   -------------------- // Optional
  //   PUSH1 shiftr    // 2
  //   SHL             // 1
  //                   ---------
  //                   [2 ... 8] + AbsValLen
  //
  {
    // Not and left/right shift
    auto Tr = std::make_unique<TransformationBuilder>(
        MF->getFunction().getContext(), TII);
    assert(!Val.abs().isZero());
    Tr->addImm(Val.abs() - 1);
    Tr->addNot();
    Tr->addImm(APInt(256, 256 - ValLen));
    Tr->addShl();
    if (LeadZ) {
      Tr->addImm(APInt(256, LeadZ));
      Tr->addShr();
    }
    Transformations.emplace_back(std::move(Tr));
  }

  // 2. A transformation that represents an immediate value in the
  //    form of a right-shifted mask.
  //
  // For example,
  //
  //   0x0000000000000000000000000000000000000000000000000000FFFFFFFFFFFF
  //
  // is represented as:
  //
  //   (~0) >> 192
  //
  // Cost:
  //   PUSH0            // 1
  //   NOT              // 1
  //   PUSH1 shift      // 2
  //   SHR              // 1
  //                    ---------
  //                    5
  //
  if (IsMask && !TrailZ) {
    assert(ValLen != 256);
    auto Tr = std::make_unique<TransformationBuilder>(
        MF->getFunction().getContext(), TII);
    Tr->addImm(APInt::getZero(256));
    Tr->addNot();
    Tr->addImm(APInt(256, 256 - ValLen));
    Tr->addShr();
    Transformations.emplace_back(std::move(Tr));
  }

  // 3. A transformation that expresses an immediate value as a smaller
  //    bit-width value shifted to the left
  //
  // For example,
  //
  //   0xFFFFFF0000000000000000000000000000000000000000000000000000000000
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
  if (TrailZ) {
    auto Tr = std::make_unique<TransformationBuilder>(
        MF->getFunction().getContext(), TII);
    Tr->addImm(Val);
    Tr->addImm(APInt(256, TrailZ));
    Tr->addShl();
    Transformations.emplace_back(std::move(Tr));
  }

  if (!LeadZ) {
    // 4. A transformation that represents an immediate value in the
    //    form of bit-reversing.
    //
    // For example,
    //
    //   0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE
    //
    // is represented as:
    //
    //   ~0x01
    //
    // Cost:
    //   PUSH             // 1 InvImm
    //   NOT              // 1
    //                    ---------
    //                    2 + InvImm
    //
    {
      auto Tr = std::make_unique<TransformationBuilder>(
          MF->getFunction().getContext(), TII);
      Tr->addImm(~Imm);
      Tr->addNot();
      Transformations.emplace_back(std::move(Tr));
    }

    // 5. A transformation that represents an immediate value by reversing its
    //    bits and representing the reversed portion using a left shift
    //    operation.
    //
    // For example,
    //
    //   0xFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    //
    // is represented as:
    //
    //   ~(0x01 << 155)
    //
    // Cost:
    //   PUSH             // 1 InvImmVal
    //   PUSH1 shift      // 2
    //   SHL              // 1
    //   NOT              // 1
    //                    ---------
    //                    5 + InvImmVal
    //
    {
      auto Tr = std::make_unique<TransformationBuilder>(
          MF->getFunction().getContext(), TII);
      APInt ImmNot = ~Imm;
      unsigned Tz = ImmNot.countTrailingZeros();
      unsigned Lz = ImmNot.countLeadingZeros();
      APInt ImmNotVal = ImmNot.extractBits(ImmNot.getBitWidth() - Tz - Lz, Tz);
      Tr->addImm(ImmNotVal);
      Tr->addImm(APInt(256, Tz));
      Tr->addShl();
      Tr->addNot();
      Transformations.emplace_back(std::move(Tr));
    }
  }

  // 6. No transformation, leave immediate as is.
  {
    auto Tr = std::make_unique<TransformationBuilder>(
        MF->getFunction().getContext(), TII);
    Tr->addImm(Imm);
    Transformations.emplace_back(std::move(Tr));
  }

  auto *OptIt = std::min_element(
      Transformations.begin(), Transformations.end(),
      [OptForSize](const auto &TrA, const auto &TrB) {
        const StackCostModel &CostA = TrA->getCost();
        const StackCostModel &CostB = TrB->getCost();
        // When optimizing for size, our primary goal is
        // to minimize the total size of the instructions.
        if (OptForSize) {
          // First check, if sizes differ.
          if (CostA.getByteSize() != CostB.getByteSize())
            return CostA.getByteSize() < CostB.getByteSize();

          // Then the number of operations.
          return CostA.getInstrCount() < CostB.getInstrCount();
        }
        // Optimizing for speed.
        // Expressing total cost in terms of both size and gas
        // enables more performance-friendly transformation choices.
        unsigned SizeGasA = CostA.getByteSize() + (GasWeight * CostA.getGas());
        unsigned SizeGasB = CostB.getByteSize() + (GasWeight * CostB.getGas());
        if (SizeGasA != SizeGasB)
          return SizeGasA < SizeGasB;

        return CostA.getInstrCount() < CostB.getInstrCount();
      });

#ifndef NDEBUG
  LLVM_DEBUG({ dbgs() << " Candidate transformations:\n"; });
  for (const auto &Tr : Transformations) {
    const StackCostModel &Cost = Tr->getCost();
    LLVM_DEBUG({
      dbgs() << "  [size: " << Cost.getByteSize()
             << ", OpNum: " << Cost.getInstrCount()
             << ", Gas: " << Cost.getGas() << "]\n";
    });
  }
#endif // NDEBUG

  const TransformationBuilder *Tr = OptIt->get();
  [[maybe_unused]] auto Res =
      TransformationCache.try_emplace(Imm, std::move(*OptIt));
  assert(Res.second);

  return Tr;
}

bool EVMConstantUnfolding::tryUnfoldConstant(MachineInstr &MI) {
  const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
  assert(Imm.getActiveBits() > 4 * 8);

  // Check for the -1 value early
  if (Imm.isAllOnes()) {
    auto Tr = std::make_unique<TransformationBuilder>(
        MF->getFunction().getContext(), TII);
    Tr->addImm(APInt::getZero(256));
    Tr->addNot();
    Tr->apply(MI);
    MI.eraseFromParent();
    ++NumOfUnfoldings;

    LLVM_DEBUG({ dbgs() << " Transforming -1 to ~0\n"; });
    return true;
  }

  bool OptForSize = MF->getFunction().hasOptSize();
  const TransformationBuilder *OptTransformation =
      findOptimalTransfomtation(Imm);

  const StackCostModel &OptCost = OptTransformation->getCost();
  LLVM_DEBUG({
    dbgs() << " Transformation candidate:\n  [size: " << OptCost.getByteSize()
           << ", OpNum: " << OptCost.getInstrCount()
           << ", Gas: " << OptCost.getGas() << "]\n";
  });

  if (OptCost.getInstrCount() == 1) {
    LLVM_DEBUG({ dbgs() << " Skipping identity transformation\n"; });
    return false;
  }

  bool Changed = false;
  if (OptForSize) {
    OptTransformation->apply(MI);
    Changed = true;
  } else {
    unsigned OrigSize = (alignTo(Imm.getActiveBits(), 8) / 8) + 1;
    // When optimizing for speed, only unfold constants if it reduces the size
    // by a factor of at least the 'SizeGainThreshold' and keeps the instruction
    // count to 'InstrNumLimitUnfolInto' or fewer.
    if ((static_cast<float>(OrigSize) /
             static_cast<float>(OptCost.getByteSize()) >
         SizeGainThreshold) &&
        OptCost.getInstrCount() <= InstrNumLimitUnfolInto) {
      OptTransformation->apply(MI);
      Changed = true;
    } else {
      LLVM_DEBUG({
        dbgs() << " Unfolding is omitted as its effect does not meet the"
               << "required gain threshold\n";
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
  MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();
  for (MachineBasicBlock &MBB : *MF) {
    unsigned LoopDepth = MLI->getLoopDepth(&MBB);
    if (LoopDepth > LoopDepthLimit) {
      LLVM_DEBUG({
        dbgs() << "Skipping block " << MBB.getName()
               << " due to exceeding the loop depth limit: " << LoopDepthLimit
               << '\n';
      });
      continue;
    }

    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      if (!EVMInstrInfo::isPush(&MI))
        continue;

      LLVM_DEBUG({ dbgs() << "Checking " << MI; });

      // It’s not practical to check small constants, since the default
      // instructions are cheapest in those cases. The limit is based on the
      // fact that the most compact transformation for representing a
      // constant is SmallValueAndLeftShift which requires at least 5 bytes.
      switch (MI.getOpcode()) {
      case EVM::PUSH0_S:
      case EVM::PUSH1_S:
      case EVM::PUSH2_S:
      case EVM::PUSH3_S:
      case EVM::PUSH4_S: {
        LLVM_DEBUG({ dbgs() << " Skipped due to bit-wise small constant\n"; });
        continue;
      }
      default:
        break;
      }

      Changed |= tryUnfoldConstant(MI);
    }
  }
  return Changed;
}
