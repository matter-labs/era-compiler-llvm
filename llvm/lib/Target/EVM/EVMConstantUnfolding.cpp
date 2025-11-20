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
#include "EVMCalculateModuleSize.h"
#include "EVMConstantSpiller.h"
#include "EVMInstrInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include <functional>

using namespace llvm;

#define DEBUG_TYPE "evm-constant-unfolding"
#define PASS_NAME "EVM constant unfolding"

STATISTIC(StatNumOfUnfoldings, "Number of unfolded constants");
STATISTIC(StatCodeSizeReduction, "Total size reduction across all functions");

// The initial values of the following options were determined experimentally
// to allow constant unfolding in non-OptForSize mode without noticeably
// impacting performance.
static cl::opt<float>
    SizeReductionThreshold("evm-const-unfolding-size-reduction-threshold",
                           cl::Hidden, cl::init(2.5),
                           cl::desc("Minimum original-to-unfolded size ratio"
                                    "required for constant unfolding when"
                                    "optimizing for speed"));

static cl::opt<unsigned> InstrNumLimitUnfoldInto(
    "evm-const-unfolding-inst-num-limit", cl::Hidden, cl::init(4),
    cl::desc("Maximum number of instructions an original"
             "instruction can be unfolded into"));

static cl::opt<unsigned>
    LoopDepthLimit("evm-const-loop-depth-limit", cl::Hidden, cl::init(2),
                   cl::desc("The maximum loop depth at which constant"
                            "unfolding is still considered beneficial"));

static cl::opt<unsigned>
    CodeSizeLimit("evm-bytecode-sizelimit", cl::Hidden, cl::init(24576),
                  cl::desc("EVM contract bytecode size limit"));

static cl::opt<unsigned> MetadataSize("evm-metadata-size", cl::Hidden,
                                      cl::init(0),
                                      cl::desc("EVM metadata size"));

static cl::opt<unsigned> ConstantReloadThreshold(
    "evm-constant-reload-threshold", cl::Hidden, cl::init(30),
    cl::desc("Minimum number of uses of a constant across the module required "
             "before reloading it from memory is considered profitable"));

static cl::opt<bool> EnableConstSpillWithoutUnsafeAsm(
    "enable-constant-spilling-without-unsafe-asm",
    cl::desc("Also enable constant spilling if there is no unsafe asm in the "
             "module"),
    cl::init(false),
    cl::Hidden // optional: hide from general users if needed
);

namespace {
using InstrsPerLoopDepthTy = SmallVector<SmallVector<MachineInstr *>>;

// This helper calculates the frequency of constants across all machine
// functions in the module and determines whether a given constant is
// eligible for spilling.
class SpillHelper {
public:
  explicit SpillHelper(SmallVector<MachineFunction *> &MFs, bool HasUnsafeAsm) {
    // Perform constant spilling only if the module already requires
    // spilling after stackification. This helps reduce compilation time,
    // since otherwise we would need to compile roughly twice as many
    // contracts.
    if (none_of(MFs,
                [](const MachineFunction *MF) {
                  return MF->getFrameInfo().hasStackObjects();
                }) &&
        (!EnableConstSpillWithoutUnsafeAsm || HasUnsafeAsm))
      return;

    for (MachineFunction *MF : MFs) {
      for (MachineBasicBlock &MBB : *MF) {
        for (MachineInstr &MI : MBB) {
          if (!EVMInstrInfo::isPush(&MI) || (MI.getOpcode() == EVM::PUSH0_S))
            continue;

          const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
          // Perform spilling only for large constants where it can have
          // a noticeable impact.
          if (Imm.getActiveBits() > 16 * 8)
            ImmToUseCount[Imm]++;
        }
      }
    }
  }

  bool isSpillEligible(const APInt &Imm) const {
    auto It = ImmToUseCount.find(Imm);
    if (It == ImmToUseCount.end())
      return false;

    return It->second >= ConstantReloadThreshold;
  }

private:
  SmallDenseMap<APInt, unsigned> ImmToUseCount;
};

// Estimates the execution cost of EVM-style stack operations.
// Tracks instruction count, gas cost, and unfolded bytecode size.
// It abstracts gas accounting for pushes and simple arithmetic/logical
// operations.
class StackCostModel {
public:
  StackCostModel() = default;

  void accountInstr(unsigned Opc, const TargetInstrInfo *TII) {
    ++InstrCount;
    const MCInstrDesc &Desc = TII->get(Opc);
    ByteSize += Desc.getSize();
    Gas += EVMInstrInfo::getGasCost(Desc);
  }

  void accountSpill(const TargetInstrInfo *TII) {
    // In most cases, offsets within the spill area can be represented
    // using a single byte, but let's pessimistically assume they require
    // 2 bytes.
    //
    //   PUSH2 offset
    //   MLOAD

    InstrCount = 2;
    const MCInstrDesc &PushDesc = TII->get(EVM::PUSH2_S);
    const MCInstrDesc &LoadDesc = TII->get(EVM::MLOAD_S);
    ByteSize = PushDesc.getSize() + LoadDesc.getSize();
    Gas =
        EVMInstrInfo::getGasCost(PushDesc) + EVMInstrInfo::getGasCost(LoadDesc);
  }

  unsigned getInstrCount() const { return InstrCount; }
  unsigned getByteSize() const { return ByteSize; }
  unsigned getGas() const { return Gas; }

private:
  unsigned InstrCount{0};
  unsigned ByteSize{0};
  unsigned Gas{0};
};

// Builds and applies a sequence of machine instructions required to
// unfold a constant. Instruction generation is deferred using lambdas,
// allowing the TransformationCandidate object to be reused for repeated
// constants. As new instructions are added, the StackCostModel is used
// to track the accumulated cost.
class TransformationCandidate {
public:
  TransformationCandidate(LLVMContext &Context, const EVMInstrInfo *TII)
      : Context(Context), TII(TII) {}

  void addShl() {
    constexpr unsigned Opc = EVM::SHL_S;
    CostModel.accountInstr(Opc, TII);
    BuildItems.push_back([this](MachineInstr &MI) { insertInstr(MI, Opc); });
  }

  void addShr() {
    constexpr unsigned Opc = EVM::SHR_S;
    CostModel.accountInstr(Opc, TII);
    BuildItems.push_back([this](MachineInstr &MI) { insertInstr(MI, Opc); });
  }

  void addNot() {
    constexpr unsigned Opc = EVM::NOT_S;
    CostModel.accountInstr(Opc, TII);
    BuildItems.push_back([this](MachineInstr &MI) { insertInstr(MI, Opc); });
  }

  void addReload() {
    CostModel.accountSpill(TII);
    IsReload = true;
  }

  void addImm(const APInt &Val) {
    unsigned Opc = EVM::getStackOpcode(EVM::getPUSHOpcode(Val));
    CostModel.accountInstr(Opc, TII);
    BuildItems.push_back([this, Val = Val](MachineInstr &MI) {
      TII->insertPush(Val, *MI.getParent(), MI, MI.getDebugLoc());
    });
  }

  // Applies queued build instruction steps to replace a given instruction.
  void apply(MachineInstr &MI) const {
    if (IsReload) {
      auto NewMI = BuildMI(*MI.getParent(), MI, MI.getDebugLoc(),
                           TII->get(EVM::CONSTANT_RELOAD));

      const APInt Imm = MI.getOperand(0).getCImm()->getValue();
      NewMI.addCImm(ConstantInt::get(Context, Imm));
      return;
    }

    for (const auto &func : BuildItems)
      func(MI);
  }

  const StackCostModel &getCost() const { return CostModel; }

  bool isReload() const { return IsReload; }

private:
  using BuildFunction = std::function<void(MachineInstr &)>;

  LLVMContext &Context;
  const EVMInstrInfo *TII{};

  StackCostModel CostModel;
  SmallVector<BuildFunction, 16> BuildItems;

  bool IsReload = false;

  void insertInstr(MachineInstr &MI, unsigned Opc) {
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(Opc));
  }
};

// Discovers, applies, and caches optimal constant unfolding
// transformations.
class ConstantUnfolder {
public:
  explicit ConstantUnfolder(LLVMContext *Context, SpillHelper &SpillHelper)
      : Context(Context), SpillHelper(SpillHelper) {}

  unsigned getCodeSizeReduction() const { return OverallCodeReductionSize; }

  bool tryToUnfoldConstant(MachineInstr &MI, bool OptForSize,
                           const EVMInstrInfo *TII);

private:
  LLVMContext *Context{};
  SpillHelper &SpillHelper;

  // The 'second' field can be set to 0 or 1, indicating whether to
  // optimize for performance or size.
  using TransformationKey = std::pair<APInt, int>;
  DenseMap<TransformationKey, std::unique_ptr<TransformationCandidate>>
      TransformationCache;

  unsigned OverallCodeReductionSize{0};

  const TransformationCandidate *
  findOptimalTransformation(const APInt &Imm, bool OptForSize,
                            const EVMInstrInfo *TII);

  void reduceCodeSizeOn(unsigned Size) {
    OverallCodeReductionSize += Size;
    ++StatNumOfUnfoldings;
  }
};

class EVMConstantUnfolding final : public ModulePass {
public:
  static char ID;

  EVMConstantUnfolding() : ModulePass(ID) {
    initializeEVMConstantUnfoldingPass(*PassRegistry::getPassRegistry());
  }

private:
  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }
};
} // end anonymous namespace

char EVMConstantUnfolding::ID = 0;

INITIALIZE_PASS_BEGIN(EVMConstantUnfolding, DEBUG_TYPE, PASS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineModuleInfoWrapperPass)
INITIALIZE_PASS_END(EVMConstantUnfolding, DEBUG_TYPE, PASS_NAME, false, false)

ModulePass *llvm::createEVMConstantUnfolding() {
  return new EVMConstantUnfolding();
}

static bool isBetterCandidate(const TransformationCandidate &A,
                              const TransformationCandidate &B,
                              bool OptForSize) {
  static constexpr unsigned Weight = 2;
  const StackCostModel &CostA = A.getCost();
  const StackCostModel &CostB = B.getCost();
  unsigned ScoreA = 0;
  unsigned ScoreB = 0;

  if (OptForSize) {
    ScoreA = (CostA.getByteSize() * Weight) + CostA.getGas() + A.isReload();
    ScoreB = (CostB.getByteSize() * Weight) + CostB.getGas() + B.isReload();
  } else {
    ScoreA = CostA.getByteSize() + (Weight * CostA.getGas()) + A.isReload();
    ScoreB = CostB.getByteSize() + (Weight * CostB.getGas()) + B.isReload();
  }
  if (ScoreA != ScoreB)
    return ScoreA < ScoreB;

  return CostA.getByteSize() < CostB.getByteSize();
}

const TransformationCandidate *
ConstantUnfolder::findOptimalTransformation(const APInt &Imm, bool OptForSize,
                                            const EVMInstrInfo *TII) {
  if (auto It = TransformationCache.find({Imm, OptForSize});
      It != TransformationCache.end()) {
    LLVM_DEBUG(
        { dbgs() << "      Retrieving transformation from the cache\n"; });

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

  SmallVector<std::unique_ptr<TransformationCandidate>, 8> Transformations;

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
  //
  {
    auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
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
  //
  if (IsMask && !TrailZ) {
    assert(ValLen != 256);
    auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
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
  //
  if (TrailZ) {
    auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
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
    //
    {
      auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
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
    //
    {
      auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
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
    auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
    Tr->addImm(Imm);
    Transformations.emplace_back(std::move(Tr));
  }

  // 7. Checks whether spilling the constant is appropriate.
  //
  // Cost:
  //   PUSH1 frame_off  // 2
  //   MLOAD            // 1
  //
  // Typically one byte is enough to encode the offset.
  //
  if (SpillHelper.isSpillEligible(Imm)) {
    auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
    Tr->addReload();
    Transformations.emplace_back(std::move(Tr));
  }

  // Find optimal transformation.
  auto *OptIt = std::min_element(Transformations.begin(), Transformations.end(),
                                 [OptForSize](const auto &A, const auto &B) {
                                   return isBetterCandidate(*A, *B, OptForSize);
                                 });

#ifndef NDEBUG
  LLVM_DEBUG({ dbgs() << "      Available transformations:\n"; });
  for (const auto &Tr : Transformations) {
    const StackCostModel &Cost = Tr->getCost();
    LLVM_DEBUG({
      dbgs() << "       [size: " << Cost.getByteSize()
             << ", instr count: " << Cost.getInstrCount()
             << ", gas: " << Cost.getGas();
      if (Tr->isReload())
        dbgs() << ", IsRelaod";
      dbgs() << "]\n";
    });
  }
#endif // NDEBUG

  const TransformationCandidate *Tr = OptIt->get();
  [[maybe_unused]] auto Res =
      TransformationCache.try_emplace({Imm, OptForSize}, std::move(*OptIt));
  assert(Res.second);

  return Tr;
}

static bool isProfitableToTranform(const APInt &Imm, const StackCostModel &Cost,
                                   bool OptForSize) {
  if (OptForSize)
    return true;

  unsigned OrigSize = (alignTo(Imm.getActiveBits(), 8) / 8) + 1;
  // When optimizing for speed, only unfold constants if it reduces the size
  // by a factor of at least the 'SizeReductionThreshold' and keeps the
  // instruction count to 'InstrNumLimitUnfoldInto' or fewer.
  if ((static_cast<float>(OrigSize) / static_cast<float>(Cost.getByteSize()) <
       SizeReductionThreshold) ||
      Cost.getInstrCount() > InstrNumLimitUnfoldInto)
    return false;

  return true;
}

bool ConstantUnfolder::tryToUnfoldConstant(MachineInstr &MI, bool OptForSize,
                                           const EVMInstrInfo *TII) {
  const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
  unsigned OrigSize = (alignTo(Imm.getActiveBits(), 8) / 8) + 1;
  assert(Imm.getActiveBits() > 4 * 8);

  // Check for the -1 value early
  if (Imm.isAllOnes()) {
    auto Tr = std::make_unique<TransformationCandidate>(*Context, TII);
    Tr->addImm(APInt::getZero(256));
    Tr->addNot();
    Tr->apply(MI);
    MI.eraseFromParent();
    assert(OrigSize > Tr->getCost().getByteSize());
    reduceCodeSizeOn(OrigSize - Tr->getCost().getByteSize());

    LLVM_DEBUG({ dbgs() << "      Transforming -1 to ~0\n"; });
    return true;
  }

  const TransformationCandidate *OptTransformation =
      findOptimalTransformation(Imm, OptForSize, TII);

  const StackCostModel &OptCost = OptTransformation->getCost();
  LLVM_DEBUG({
    dbgs() << "      Optimal transformation:\n"
           << "       [size: " << OptCost.getByteSize()
           << ", instr count: " << OptCost.getInstrCount()
           << ", gas: " << OptCost.getGas();
    if (OptTransformation->isReload())
      dbgs() << ", IsRelaod";
    dbgs() << "]\n";
  });

  if (OptCost.getInstrCount() == 1) {
    LLVM_DEBUG({ dbgs() << "      Skipping identity transformation\n"; });
    return false;
  }

  if (isProfitableToTranform(Imm, OptCost, OptForSize)) {
    OptTransformation->apply(MI);
    MI.eraseFromParent();
    assert(OrigSize > OptCost.getByteSize());
    reduceCodeSizeOn(OrigSize - OptCost.getByteSize());
    return true;
  }

  LLVM_DEBUG({
    dbgs() << "      Transformation is omitted as its effect does not meet the"
           << " required reduction threshold\n";
  });

  return false;
}

static bool shouldSkip(const MachineInstr &MI) {
  if (!EVMInstrInfo::isPush(&MI))
    return true;

  // It’s not practical to check small constants, since the default
  // instructions are cheapest in those cases. The limit is based on the
  // fact that the most compact transformation for representing a
  // constant requires at least 5 bytes.
  switch (MI.getOpcode()) {
  case EVM::PUSH0_S:
  case EVM::PUSH1_S:
  case EVM::PUSH2_S:
  case EVM::PUSH3_S:
  case EVM::PUSH4_S: {
    return true;
  }
  default:
    return false;
  }
}

// This helper class organizes a MF’s instructions by their loop nesting depth,
// as reported by MachineLoopInfo. It builds a SmallVector of buckets, where
// each bucket contains the MIs for a specific loop depth (0 = out of loop,
// 1 - top level loop, etc.).
class LoopDepthInstrCache {
public:
  LoopDepthInstrCache(MachineFunction &MF, const MachineLoopInfo &MLI) {
    unsigned MaxLoopDepth = 0;
    for (auto *ML : MLI.getLoopsInPreorder())
      MaxLoopDepth = std::max(MaxLoopDepth, ML->getLoopDepth());

    InstrsPerLoopDepth.resize(MaxLoopDepth + 1);
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (shouldSkip(MI))
          continue;

        unsigned Depth = MLI.getLoopDepth(&MBB);
        assert(Depth < InstrsPerLoopDepth.size());
        InstrsPerLoopDepth[Depth].push_back(&MI);
      }
    }

    LLVM_DEBUG({
      dbgs() << "Instructions in function: " << MF.getName() << '\n';
      for (unsigned Depth = 0; Depth < InstrsPerLoopDepth.size(); ++Depth) {
        dbgs() << "  loop depth: " << Depth << '\n';
        for (const MachineInstr *MI : InstrsPerLoopDepth[Depth])
          dbgs() << "    " << *MI << '\n';
      }
    });
  }

  LoopDepthInstrCache(const LoopDepthInstrCache &) = delete;
  LoopDepthInstrCache &operator=(const LoopDepthInstrCache &) = delete;
  LoopDepthInstrCache(LoopDepthInstrCache &&) noexcept = default;
  LoopDepthInstrCache &operator=(LoopDepthInstrCache &&) noexcept = default;
  ~LoopDepthInstrCache() = default;

  const InstrsPerLoopDepthTy &getInstructionsPerLoopDepth() const {
    return InstrsPerLoopDepth;
  }

  SmallVector<MachineInstr *> getAllInstructions() const {
    SmallVector<MachineInstr *> Instrs;
    for (const SmallVector<MachineInstr *> &Bucket : InstrsPerLoopDepth)
      Instrs.append(Bucket.begin(), Bucket.end());

    return Instrs;
  }

  DenseSet<const MachineInstr *> &getVisited() { return Visited; }

private:
  InstrsPerLoopDepthTy InstrsPerLoopDepth;
  DenseSet<const MachineInstr *> Visited;
};

static bool processInstructions(ConstantUnfolder &Unfolder,
                                const SmallVector<MachineInstr *> &Instrs,
                                DenseSet<const MachineInstr *> &Visited,
                                bool OptForSize, const EVMInstrInfo *TII) {
  bool Changed = false;
  for (MachineInstr *MI : Instrs) {
    if (Visited.count(MI))
      continue;

    LLVM_DEBUG({ dbgs() << "     Checking " << *MI; });

    if (Unfolder.tryToUnfoldConstant(*MI, OptForSize, TII)) {
      Visited.insert(MI);
      Changed = true;
    }
  }

  return Changed;
}

static bool runImpl(Module &M, MachineModuleInfo &MMI) {
  bool Changed = false;

  SmallVector<MachineFunction *> MFs;
  for_each(M.getFunctionList(), [&MFs, &MMI](Function &F) {
    if (MachineFunction *MF = MMI.getMachineFunction(F))
      MFs.push_back(MF);
  });

  SpillHelper SpillHelper(MFs, M.getNamedMetadata("llvm.evm.hasunsafeasm"));
  ConstantUnfolder Unfolder(&M.getContext(), SpillHelper);

  // Metadata size is included into the bytecode size.
  const unsigned ModuleCodeSize =
      EVM::calculateModuleCodeSize(M, MMI) + MetadataSize;

  // Collect PUSH instructions to process.
  SmallVector<
      std::pair<MachineFunction *, std::unique_ptr<LoopDepthInstrCache>>, 16>
      InstrCacheMap;

  for (MachineFunction *MF : MFs) {
    // Compute MachineLoopInfo on the fly, as it's not available on the
    // Module pass level.
    auto OwnedMDT = std::make_unique<MachineDominatorTree>();
    OwnedMDT->getBase().recalculate(*MF);
    MachineDominatorTree *MDT = OwnedMDT.get();

    auto OwnedMLI = std::make_unique<MachineLoopInfo>();
    OwnedMLI->analyze(MDT->getBase());
    const MachineLoopInfo *MLI = OwnedMLI.get();

    InstrCacheMap.emplace_back(
        MF, std::make_unique<LoopDepthInstrCache>(*MF, *MLI));
  }

  LLVM_DEBUG({
    dbgs() << "*** Running constant unfolding in the default mode ***\n";
    dbgs() << "*** Initial module size: " << ModuleCodeSize << " ***\n";
  });

  // First, process all PUSH instructions in the default mode, selecting
  // unfolding heuristics based on whether the OptSize flag is set for
  // the MachineFunction.
  for (auto &[MF, Cache] : InstrCacheMap) {
    LLVM_DEBUG({ dbgs() << "  Checking function: " << MF->getName() << '\n'; });

    bool OptForSize = MF->getFunction().hasOptSize();
    const EVMInstrInfo *TII = MF->getSubtarget<EVMSubtarget>().getInstrInfo();
    Changed |= processInstructions(Unfolder, Cache->getAllInstructions(),
                                   Cache->getVisited(), OptForSize, TII);
  }

  unsigned CodeSizeReduction = Unfolder.getCodeSizeReduction();
  if (ModuleCodeSize < CodeSizeLimit + CodeSizeReduction) {
    StatCodeSizeReduction = CodeSizeReduction;
    return Changed;
  }

  LLVM_DEBUG({
    dbgs() << "*** Current module size is "
           << ModuleCodeSize - CodeSizeReduction
           << ", which still exceeds the limit, falling back to "
              "size-minimization mode ***\n";
  });

  // First, process all PUSH instructions in the default mode, selecting
  // unfolding heuristics based on whether the OptSize flag is set for
  // the MachineFunction.
  for (unsigned LoopDepth = 0; LoopDepth <= LoopDepthLimit; ++LoopDepth) {
    LLVM_DEBUG({
      dbgs() << "*** Running constant unfolding in "
                "size-minimization mode at loop depth "
             << LoopDepth << " ***\n";
    });

    for (auto &[MF, Cache] : InstrCacheMap) {
      LLVM_DEBUG(
          { dbgs() << "  Checking function: " << MF->getName() << '\n'; });

      const InstrsPerLoopDepthTy &InstrsPerLoopDepth =
          Cache->getInstructionsPerLoopDepth();
      if (LoopDepth >= InstrsPerLoopDepth.size())
        continue;

      const EVMInstrInfo *TII = MF->getSubtarget<EVMSubtarget>().getInstrInfo();
      Changed |=
          processInstructions(Unfolder, InstrsPerLoopDepth[LoopDepth],
                              Cache->getVisited(), /*OptForSize=*/true, TII);

      CodeSizeReduction = Unfolder.getCodeSizeReduction();
      if (ModuleCodeSize < CodeSizeLimit + CodeSizeReduction) {
        StatCodeSizeReduction = CodeSizeReduction;
        return Changed;
      }
    }

    LLVM_DEBUG({
      dbgs() << "*** Current module size is "
             << ModuleCodeSize - CodeSizeReduction << " ***\n";
    });
  }

  return Changed;
}

bool EVMConstantUnfolding::runOnModule(Module &M) {
  LLVM_DEBUG({ dbgs() << "********** " << PASS_NAME << " **********\n"; });

  return runImpl(M, getAnalysis<MachineModuleInfoWrapperPass>().getMMI());
}
