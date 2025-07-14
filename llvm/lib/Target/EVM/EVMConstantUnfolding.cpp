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
#include "EVMInstrInfo.h"
#include "EVMTargetMachine.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include <functional>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "evm-constant-unfolding"
#define PASS_NAME "EVM constant unfolding"

STATISTIC(NumOfUnfoldings, "Number of unfolded constants");
STATISTIC(CodeSizeReduction, "Total size reduction across all functions");

// The initial values of the following options were determined experimentally
// to allow constant unfolding in non-OptForSize mode without noticeably
// impacting performance.
static cl::opt<float>
    SizeReductionThreshold("evm-const-unfolding-size-reduction-threshold",
                           cl::Hidden, cl::init(2.5),
                           cl::desc("Minimum original-to-unfolded size ratio"
                                    "required for constant unfolding when"
                                    "optimizing for speed"));

static cl::opt<unsigned>
    InstrNumLimitUnfolInto("evm-const-unfolding-inst-num-limit", cl::Hidden,
                           cl::init(4),
                           cl::desc("Maximum number of instructions an original"
                                    "instruction can be unfolded into"));

static cl::opt<unsigned>
    LoopDepthLimit("evm-const-loop-depth-limit", cl::Hidden, cl::init(2),
                   cl::desc("The maximum loop depth at which constant"
                            "unfolding is still considered beneficial"));

static cl::opt<unsigned>
    CodeSizeLimit("evm-bytecode-sizelimit", cl::Hidden, cl::init(24576),
                  cl::desc("EVM contract bytecode size limit"));

namespace {

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
// allowing the TransformationBuilder object to be reused for repeated
// constants. As new instructions are added, the StackCostModel is used
// to track the accumulated cost.
class TransformationBuilder {
public:
  TransformationBuilder(LLVMContext &Context, const TargetInstrInfo *TII)
      : Context(Context), TII(TII) {}

  void addSub() {
    constexpr unsigned Opc = EVM::SUB_S;
    CostModel.accountInstr(Opc, TII);
    BuildItems.push_back([this](MachineInstr &MI) { insertInstr(MI, Opc); });
  }

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

  void addImm(const APInt &Val) {
    unsigned Opc = EVM::getStackOpcode(EVM::getPUSHOpcode(Val));
    CostModel.accountInstr(Opc, TII);
    BuildItems.push_back([this, Opc, Val = Val](MachineInstr &MI) {
      insertImmInstr(MI, Opc, Val);
    });
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
  const TargetInstrInfo *TII{};

  StackCostModel CostModel;
  SmallVector<BuildFunction, 16> BuildItems;

  void insertInstr(MachineInstr &MI, unsigned Opc) {
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(Opc));
  }

  void insertImmInstr(MachineInstr &MI, unsigned Opc, const APInt &Val) {
    auto NewMI = BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(Opc));
    if (Opc != EVM::PUSH0_S)
      NewMI.addCImm(ConstantInt::get(Context, Val));
  }
};

// Discovers, applies, and caches optimal constant unfolding
// transformations.
class ConstantUnfolder {
public:
  ConstantUnfolder(LLVMContext *Context, const TargetInstrInfo *TII)
      : Context(Context), TII(TII) {}

  unsigned getCodeSizeReduction() const { return OverallCodeReductionSize; }

  const TransformationBuilder *findOptimalTransformation(const APInt &Imm,
                                                         bool OptForSize);

  bool tryToUnfoldConstant(MachineInstr &MI, bool OptForSize);

private:
  LLVMContext *Context{};
  const TargetInstrInfo *TII{};

  // The 'second' field can be set to 0 or 1, indicating whether to
  // optimize for performance or size.
  using TransformationKey = std::pair<APInt, int>;
  DenseMap<TransformationKey, std::unique_ptr<TransformationBuilder>>
      TransformationCache;
  unsigned OverallCodeReductionSize{0};

  void reduceCodeSizeOn(unsigned Size) {
    OverallCodeReductionSize += Size;
    ++NumOfUnfoldings;
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

static bool isBetterCandidate(const TransformationBuilder &A,
                              const TransformationBuilder &B, bool OptForSize) {
  static constexpr unsigned Weight = 2;
  const StackCostModel &CostA = A.getCost();
  const StackCostModel &CostB = B.getCost();
  unsigned ScoreA = 0;
  unsigned ScoreB = 0;

  if (OptForSize) {
    ScoreA = (CostA.getByteSize() * Weight) + CostA.getGas();
    ScoreB = (CostB.getByteSize() * Weight) + CostB.getGas();
  } else {
    ScoreA = CostA.getByteSize() + (Weight * CostA.getGas());
    ScoreB = CostB.getByteSize() + (Weight * CostB.getGas());
  }
  if (ScoreA != ScoreB)
    return ScoreA < ScoreB;

  return CostA.getInstrCount() < CostB.getInstrCount();
}

const TransformationBuilder *
ConstantUnfolder::findOptimalTransformation(const APInt &Imm, bool OptForSize) {
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
  //
  {
    auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
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
    auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
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
    auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
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
      auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
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
      auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
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
    auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
    Tr->addImm(Imm);
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
             << ", gas: " << Cost.getGas() << "]\n";
    });
  }
#endif // NDEBUG

  const TransformationBuilder *Tr = OptIt->get();
  [[maybe_unused]] auto Res =
      TransformationCache.try_emplace({Imm, OptForSize}, std::move(*OptIt));
  assert(Res.second);

  return Tr;
}

static bool isProfitableToTranform(const APInt &Imm, const StackCostModel &Cost,
                                   bool OptForSize) {
  if (!OptForSize) {
    unsigned OrigSize = (alignTo(Imm.getActiveBits(), 8) / 8) + 1;
    // When optimizing for speed, only unfold constants if it reduces the size
    // by a factor of at least the 'SizeReductionThreshold' and keeps the
    // instruction count to 'InstrNumLimitUnfolInto' or fewer.
    if ((static_cast<float>(OrigSize) / static_cast<float>(Cost.getByteSize()) <
         SizeReductionThreshold) ||
        Cost.getInstrCount() > InstrNumLimitUnfolInto)
      return false;
  }

  return true;
}

bool ConstantUnfolder::tryToUnfoldConstant(MachineInstr &MI, bool OptForSize) {
  const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
  unsigned OrigSize = (alignTo(Imm.getActiveBits(), 8) / 8) + 1;
  assert(Imm.getActiveBits() > 4 * 8);

  // Check for the -1 value early
  if (Imm.isAllOnes()) {
    auto Tr = std::make_unique<TransformationBuilder>(*Context, TII);
    Tr->addImm(APInt::getZero(256));
    Tr->addNot();
    Tr->apply(MI);
    MI.eraseFromParent();
    assert(OrigSize > Tr->getCost().getByteSize());
    reduceCodeSizeOn(OrigSize - Tr->getCost().getByteSize());

    LLVM_DEBUG({ dbgs() << "      Transforming -1 to ~0\n"; });
    return true;
  }

  const TransformationBuilder *OptTransformation =
      findOptimalTransformation(Imm, OptForSize);

  const StackCostModel &OptCost = OptTransformation->getCost();
  LLVM_DEBUG({
    dbgs() << "      Optimal transformation:\n"
           << "       [size: " << OptCost.getByteSize()
           << ", instr count: " << OptCost.getInstrCount()
           << ", gas: " << OptCost.getGas() << "]\n";
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

// Process MF's blocks as follows:
//  - If MinimizeSizeAtLoopDepthOnly is null, process both loop and non-loop
//    blocks. Apply transformations based on the presence of the 'OptSize' flag.
//  - If MinimizeSizeAtLoopDepthOnly is non null, process only blocks that
//    belong to loops at the specified depth. A depth of 0 refers to non-loop
//    blocks.
//
static bool processFunction(ConstantUnfolder &Unfolder, MachineFunction &MF,
                            std::optional<unsigned> MinimizeSizeAtLoopDepthOnly,
                            MachineLoopInfo *MLI) {
  LLVM_DEBUG({ dbgs() << "  Checking function: " << MF.getName() << '\n'; });

  bool OptForSize =
      !MinimizeSizeAtLoopDepthOnly ? MF.getFunction().hasOptSize() : true;

  auto ProcessMBB = [&Unfolder, OptForSize](MachineBasicBlock &MBB) -> bool {
    LLVM_DEBUG({
      dbgs() << "   Checking block: ";
      MBB.printName(dbgs());
      dbgs() << '\n';
    });

    bool Changed = false;
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      if (shouldSkip(MI))
        continue;

      LLVM_DEBUG({ dbgs() << "     Checking " << MI; });
      Changed |= Unfolder.tryToUnfoldConstant(MI, OptForSize);
    }
    return Changed;
  };

  bool Changed = false;
  if (MinimizeSizeAtLoopDepthOnly) {
    // Restrict processing to MBBs outside of all loops.
    if (*MinimizeSizeAtLoopDepthOnly == 0) {
      for (MachineBasicBlock &MBB : MF)
        if (!MLI->getLoopDepth(&MBB))
          Changed |= ProcessMBB(MBB);

      return Changed;
    }
    // Limit processing to MBBs within loops at the specified nesting depth.
    for (const MachineLoop *L : MLI->getLoopsInPreorder()) {
      if (L->getLoopDepth() != *MinimizeSizeAtLoopDepthOnly)
        continue;

      for (MachineBasicBlock *MBB : L->getBlocks())
        if (MLI->getLoopDepth(MBB) == *MinimizeSizeAtLoopDepthOnly)
          Changed |= ProcessMBB(*MBB);
    }
    return Changed;
  }

  // By default, process both loop and non-loop MBBs.
  for (MachineBasicBlock &MBB : MF) {
    if (MLI->getLoopDepth(&MBB) <= LoopDepthLimit)
      Changed |= ProcessMBB(MBB);
  }

  return Changed;
}

static bool processModule(unsigned ModuleCodeSize, ConstantUnfolder &Unfolder,
                          std::optional<unsigned> MinimizeSizeAtLoopDepthOnly,
                          Module &M, MachineModuleInfo &MMI,
                          MachineFunctionAnalysisManager &MFAM) {
  bool Changed = false;
  for (Function &F : M) {
    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;

    MachineLoopInfo &MLI = MFAM.getResult<MachineLoopAnalysis>(*MF);

    Changed |=
        processFunction(Unfolder, *MF, MinimizeSizeAtLoopDepthOnly, &MLI);
    if (MinimizeSizeAtLoopDepthOnly &&
        ModuleCodeSize < (CodeSizeLimit + Unfolder.getCodeSizeReduction()))
      break;
  }

  return Changed;
}

static bool runImpl(Module &M, ModuleAnalysisManager &AM) {
  bool Changed = false;
  MachineModuleInfo &MMI = AM.getResult<MachineModuleAnalysis>(M).getMMI();
  MachineFunctionAnalysisManager &MFAM =
      AM.getResult<MachineFunctionAnalysisManagerModuleProxy>(M).getManager();

  const auto &EVMTM = static_cast<const EVMTargetMachine &>(MMI.getTarget());
  ConstantUnfolder Unfolder(&M.getContext(),
                            EVMTM.getSubtarget()->getInstrInfo());

  const unsigned ModuleCodeSize = EVM::calculateModuleCodeSize(M, MMI);

  LLVM_DEBUG({
    dbgs() << "*** Running constant unfolding in the default"
              " mode ***\n";
  });
  LLVM_DEBUG(
      { dbgs() << "*** Initial module size: " << ModuleCodeSize << " ***\n"; });

  std::optional<unsigned> MinSizeAtLoopDepthOnly{};
  Changed |= processModule(ModuleCodeSize, Unfolder, MinSizeAtLoopDepthOnly, M,
                           MMI, MFAM);
  CodeSizeReduction = Unfolder.getCodeSizeReduction();
  if (ModuleCodeSize < CodeSizeLimit + CodeSizeReduction)
    return Changed;

  LLVM_DEBUG({
    dbgs() << "*** Current module size is "
           << ModuleCodeSize - CodeSizeReduction
           << ", which still exceeds the limit, falling back to "
              "size-minimization mode ***\n";
  });

  for (unsigned LoopDepth = 0; LoopDepth <= LoopDepthLimit; ++LoopDepth) {
    LLVM_DEBUG({
      dbgs() << "*** Running constant unfolding in "
                "size-minimization mode at loop depth "
             << LoopDepth << " ***\n";
    });

    MinSizeAtLoopDepthOnly = LoopDepth;
    Changed |= processModule(ModuleCodeSize, Unfolder, MinSizeAtLoopDepthOnly,
                             M, MMI, MFAM);
    CodeSizeReduction = Unfolder.getCodeSizeReduction();

    LLVM_DEBUG({
      dbgs() << "*** Current module size is "
             << ModuleCodeSize - CodeSizeReduction << " ***\n";
    });

    if (ModuleCodeSize < CodeSizeLimit + CodeSizeReduction)
      return Changed;
  }
  return Changed;
}

struct EVMConstantUnfoldingPass : PassInfoMixin<EVMConstantUnfoldingPass> {
  EVMConstantUnfoldingPass() = default;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    runImpl(M, AM);
    return PreservedAnalyses::all();
  }
};

bool EVMConstantUnfolding::runOnModule(Module &M) {
  LLVM_DEBUG({ dbgs() << "********** " << PASS_NAME << " **********\n"; });

  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

  // This pass operates at the Module level but requires access to
  // MachineLoopInfo, which is a MachineFunction-level analysis.
  // Due to the CodeGen pipeline still relying on the legacy pass manager,
  // cross-level analysis access is not supported. To work around this,
  // we construct a local new pass manager and invoke the EVMConstantUnfolding
  // pass through it.
  // This approach has at least two drawbacks:
  //   - MachineLoopAnalysis and MachineDominatorTreeAnalysis are recomputed
  //     for every MachineFunction, leading to redundant analysis overhead.
  //   - The local pass manager structure is not visible when using the
  //     '-debug-pass=Structure' option, limiting debugging transparency.
  // TODO: remove this workaround once the EVM CodeGen pipeline adopts
  // the new pass manager, #862.

  // The order of object construction is important to avoid
  // stack-use-after-scope error.
  MachineFunctionAnalysisManager MFAM;
  ModuleAnalysisManager MAM;

  // Register module analyses
  MAM.registerPass([&MMI] { return MachineModuleAnalysis(MMI); });
  MAM.registerPass([&] { return PassInstrumentationAnalysis(); });
  MAM.registerPass(
      [&MFAM] { return MachineFunctionAnalysisManagerModuleProxy(MFAM); });

  // Register machine function analyses
  MFAM.registerPass([&] { return PassInstrumentationAnalysis(); });
  MFAM.registerPass([&] { return MachineDominatorTreeAnalysis(); });
  MFAM.registerPass([&] { return MachineLoopAnalysis(); });

  ModulePassManager MPM;
  MPM.addPass(EVMConstantUnfoldingPass());
  MPM.run(M, MAM);

  return NumOfUnfoldings > 0;
}
