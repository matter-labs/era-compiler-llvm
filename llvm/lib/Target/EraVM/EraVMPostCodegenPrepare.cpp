//===-- EraVMPostCodegenPrepare.cpp - EraVM Post CodeGenPrepare -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass performs optimizations after CodeGenPrepare pass.
// Currently, it is only performing icmp slt/sgt x, C (where C is a constant)
// to fewer unsigned compare instructions.
//
// TODOs:
//   1. #524: Rename this pass to EraVMCodegenPrepare after CPR-1353.
//   2. #491: Move rearrangeOverflowHandlingBranches from overflow reverted
//      patches here, since CodeGenPrepare can generate overflow intrinsics.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/InitializePasses.h"

using namespace llvm;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "eravm-post-codegen-prepare"
#define ERAVM_POST_CODEGEN_PREPARE_NAME                                        \
  "EraVM optimizations after CodeGenPrepare pass"

static cl::opt<bool> EnableSplitLoopPHILiveRanges(
    "eravm-enable-split-loop-phi-live-ranges", cl::Hidden, cl::init(true),
    cl::desc("Enable splitting live ranges of PHI nodes in loops"));

static cl::opt<unsigned> NumOfPHIUsesToSplitLiveRanges(
    "eravm-num-of-phi-uses-to-split-live-ranges", cl::Hidden, cl::init(20),
    cl::desc("Number of uses of PHI node to consider splitting live ranges"));

namespace {
struct EraVMPostCodegenPrepare : public FunctionPass {
public:
  static char ID; // Pass ID
  EraVMPostCodegenPrepare() : FunctionPass(ID) {
    initializeEraVMPostCodegenPreparePass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return ERAVM_POST_CODEGEN_PREPARE_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // end anonymous namespace

// Generate unsigned compare instructions based on the predicates and extend
// first operand if needed. If Pred2 is valid, generate second compare
// instruction with binary operation Opc.
static Value *generateUnsignedCmps(ICmpInst &Cmp, const APInt &Val,
                                   CmpInst::Predicate Pred1,
                                   CmpInst::Predicate Pred2,
                                   Instruction::BinaryOps Opc) {
  IRBuilder<> Builder(&Cmp);
  assert(Val.getBitWidth() == 256 && "Expected 256-bit value for comparison");
  assert(ICmpInst::isUnsigned(Pred1) && "Expected unsigned predicate in Pred1");

  auto *Val256 = ConstantInt::get(Builder.getInt256Ty(), Val);
  auto *Op256 = Builder.CreateZExt(Cmp.getOperand(0), Builder.getInt256Ty());
  auto *NewCmp = Builder.CreateICmp(Pred1, Op256, Val256);

  // Generate second compare instruction if needed.
  if (Pred2 != CmpInst::BAD_ICMP_PREDICATE) {
    auto *CmpVal = dyn_cast<ConstantInt>(Cmp.getOperand(1));
    assert(CmpVal && "Expected constant operand for signed compare");
    assert(ICmpInst::isUnsigned(Pred2) &&
           "Expected unsigned predicate in Pred2");
    auto *Cmp0 = Builder.CreateICmp(
        Pred2, Op256, Builder.getInt(CmpVal->getValue().zext(256)));
    NewCmp = Builder.CreateBinOp(Opc, NewCmp, Cmp0);
  }
  return NewCmp;
}

// Since we don't have signed compare instructions and we are doing expensive
// lowering of them, try to transform them into fewer unsigned compares where
// possible. Currently, it is trying to do following transformations:
//   1. x <s 0 -> x >u MaxSignedValue
//   2. x <s C (where C > 0) -> (x >u MaxSignedValue) | (x <u C)
//   3. x >s -1 -> x <u MinSignedValue
//   4. x >s C (where C > -1) -> (x <u MinSignedValue) & (x >u C)
static bool optimizeICmp(ICmpInst &Cmp) {
  auto *CmpVal = dyn_cast<ConstantInt>(Cmp.getOperand(1));
  if (!CmpVal || CmpVal->getBitWidth() > 256)
    return false;

  Value *NewCmp = nullptr;
  if (Cmp.getPredicate() == CmpInst::ICMP_SLT && !CmpVal->isNegative()) {
    APInt MaxSignedValue =
        APInt(CmpVal->getBitWidth(), -1, true).lshr(1).zext(256);
    NewCmp = generateUnsignedCmps(
        Cmp, MaxSignedValue, CmpInst::ICMP_UGT,
        !CmpVal->isZero() ? CmpInst::ICMP_ULT : CmpInst::BAD_ICMP_PREDICATE,
        Instruction::Or);
  } else if (Cmp.getPredicate() == CmpInst::ICMP_SGT &&
             (CmpVal->isMinusOne() || !CmpVal->isNegative())) {
    APInt MinSignedValue = APInt(CmpVal->getBitWidth(), 1)
                               .shl(CmpVal->getBitWidth() - 1)
                               .zext(256);
    NewCmp = generateUnsignedCmps(
        Cmp, MinSignedValue, CmpInst::ICMP_ULT,
        !CmpVal->isMinusOne() ? CmpInst::ICMP_UGT : CmpInst::BAD_ICMP_PREDICATE,
        Instruction::And);
  }

  if (!NewCmp)
    return false;

  Cmp.replaceAllUsesWith(NewCmp);
  Cmp.eraseFromParent();
  return true;
}

// This optimization tries to convert:
//  %c = icmp ult %x, imm
//  br %c, bla, blb
//  %tc = lshr %x, LogBase2(imm)
// to
//  %tc = lshr %x, LogBase2(imm)
//  %c = icmp eq %tc, 0
//  br %c, bla, blb
//
// or
//
//  %c = icmp eq/ne %x, imm
//  br %c, bla, blb
//  %tc = add/sub %x, -imm/imm
// to
//  %tc = add/sub %x, -imm/imm
//  %c = icmp eq/ne %tc, 0
//  br %c, bla, blb
//
// It is beneficial to do this transformation since lshr/add/sub produce flags
// and cmp eq/ne 0 can be combined with them.
// This is the same implementation as in the CodeGenPrepare pass (function
// optimizeBranch) with the fix from upstream to drop poison generating
// flags (PR #90382, commit ab12bba). Since this optimization can
// create opportunities to generate overflow intrinsics (in
// CodeGenPrepare::combineToUAddWithOverflow and
// CodeGenPrepare::combineToUSubWithOverflow functions, where the latter is not
// enabled atm since TLI->shouldFormOverflowOp returns true only for add), we
// are moving this optimization here to prevent that. Benchmarks have shown that
// creating more overflow intrinsics is not beneficial.
// TODO #625: When ticket is resolved, remove this function and use
// preferZeroCompareBranch TLI hook.
static bool optimizeBranch(BranchInst *Branch) {
  if (!Branch->isConditional())
    return false;

  auto *Cmp = dyn_cast<ICmpInst>(Branch->getCondition());
  if (!Cmp || !isa<ConstantInt>(Cmp->getOperand(1)) || !Cmp->hasOneUse())
    return false;

  Value *X = Cmp->getOperand(0);
  APInt CmpC = cast<ConstantInt>(Cmp->getOperand(1))->getValue();

  for (auto *U : X->users()) {
    auto *UI = dyn_cast<Instruction>(U);
    // A quick dominance check
    if (!UI ||
        (UI->getParent() != Branch->getParent() &&
         UI->getParent() != Branch->getSuccessor(0) &&
         UI->getParent() != Branch->getSuccessor(1)) ||
        (UI->getParent() != Branch->getParent() &&
         !UI->getParent()->getSinglePredecessor()))
      continue;

    if (CmpC.isPowerOf2() && Cmp->getPredicate() == ICmpInst::ICMP_ULT &&
        match(UI, m_Shr(m_Specific(X), m_SpecificInt(CmpC.logBase2())))) {
      IRBuilder<> Builder(Branch);
      if (UI->getParent() != Branch->getParent())
        UI->moveBefore(Branch);
      UI->dropPoisonGeneratingFlags();
      Value *NewCmp = Builder.CreateCmp(ICmpInst::ICMP_EQ, UI,
                                        ConstantInt::get(UI->getType(), 0));
      LLVM_DEBUG(dbgs() << "Converting " << *Cmp << "\n");
      LLVM_DEBUG(dbgs() << " to compare on zero: " << *NewCmp << "\n");
      Cmp->replaceAllUsesWith(NewCmp);
      return true;
    }
    if (Cmp->isEquality() &&
        (match(UI, m_Add(m_Specific(X), m_SpecificInt(-CmpC))) ||
         match(UI, m_Sub(m_Specific(X), m_SpecificInt(CmpC))))) {
      IRBuilder<> Builder(Branch);
      if (UI->getParent() != Branch->getParent())
        UI->moveBefore(Branch);
      UI->dropPoisonGeneratingFlags();
      Value *NewCmp = Builder.CreateCmp(Cmp->getPredicate(), UI,
                                        ConstantInt::get(UI->getType(), 0));
      LLVM_DEBUG(dbgs() << "Converting " << *Cmp << "\n");
      LLVM_DEBUG(dbgs() << " to compare on zero: " << *NewCmp << "\n");
      Cmp->replaceAllUsesWith(NewCmp);
      return true;
    }
  }
  return false;
}

static bool isUnsignedArithmeticOverflowInstruction(Instruction &I) {
  auto *Call = dyn_cast<CallInst>(&I);
  if (!Call)
    return false;
  Intrinsic::ID IntID = Call->getIntrinsicID();
  if (IntID != Intrinsic::uadd_with_overflow &&
      IntID != Intrinsic::usub_with_overflow &&
      IntID != Intrinsic::umul_with_overflow) {
    return false;
  }
  return true;
}

// This function is an optimization for overflow arithmetic intrinsics.
// For every branch that utilizes the overflow i1 output of the
// intrinsic, it does two things:
// 1. make sure FBB is adjacent to the branch in layout. This is
//    required to make sure ISEL not going to flip the branch
//    condition by adding XOR to the result. ISEL does this to create a
//    fallthrough optimization opportunity for MachineBlockPlacement pass.
//    In ISEL we have a specific pattern to match so that we can custom lower
//    the overflow handling, so we do not want ISEL do extra work for us.
// 2. move TBB out of the way to cold section. This is needed to achieve
//    good code sequence for non-overflow handling.
//    This is done by giving minimal probability to TBB so that
//    MachineBlockPlacement pass will rearrange it to cold section.
static bool rearrangeOverflowHandlingBranches(Function &F) {
  bool Changed = false;
  // iterate over all basic blocks:
  auto BBI = F.begin();
  auto BBE = F.end();
  while (BBI != BBE) {
    auto *BB = &*BBI;
    BBI = std::next(BBI);
    for (auto &I : *BB) {
      if (!isUnsignedArithmeticOverflowInstruction(I))
        continue;

      // now we've found an overflow handling intrinsic
      // get the overflow branching block:

      // we are going to match structure like this:
      // %5 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %4, i32 1)
      // %7 = extractvalue { i32, i1 } %5, 1
      // br i1 %7, label %8, label %10

      auto *Call = dyn_cast<CallInst>(&I);
      for (User *U : Call->users()) {

        // check extractvalue: there must be at least one use which is
        // extractvalue and index 1
        auto *ExtractValue = dyn_cast<ExtractValueInst>(U);
        if (!ExtractValue ||
            (!ExtractValue->hasIndices() || ExtractValue->getIndices()[0] != 1))
          continue;

        // check that the extracted value is used by a conditional branch in the
        // same basicblock:
        auto it = llvm::find_if(ExtractValue->users(), [&](User *U) {
          auto *IteratingBranch = dyn_cast<BranchInst>(U);
          if (IteratingBranch && IteratingBranch->getParent() == BB &&
              IteratingBranch->isConditional())
            return true;
          return false;
        });
        if (it == ExtractValue->user_end())
          continue;

        // we have found a use which is conditional branch that uses the
        // result of extractvalue.
        auto *Branch = cast<BranchInst>(*it);
        BasicBlock *TBB = Branch->getSuccessor(0);
        BasicBlock *FBB = Branch->getSuccessor(1);

        // now we've found the conversion candidate, and its branching TBB and
        // FBB.
        // We now will ensure that FBB is next to current BB in layout. This
        // will create an opportunity for MachineBlockPlacement to fall through
        // to FBB, and is necessary for the desired code sequence.
        FBB->moveAfter(BB);

        // also make TBB a very low weight branch, so it can be moved to
        // cold section.
        LLVMContext &Ctx = TBB->getContext();
        llvm::MDString *mdName = llvm::MDString::get(Ctx, "branch_weights");
        // set first successor's branching weight to minimal and second
        // successor's branching weight to maximal
        llvm::MDTuple *ColdWeights = llvm::MDTuple::get(
            Ctx, {mdName,
                  llvm::ConstantAsMetadata::get(
                      llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1)),
                  llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(
                      llvm::Type::getInt32Ty(Ctx), UINT32_MAX))});
        Branch->setMetadata(llvm::LLVMContext::MD_prof, ColdWeights);

        Changed = true;
      }
    }
  }
  return Changed;
}

struct InstSplitInfo {
  using InstInfo = std::pair<Instruction *, APInt>;
  // Vector of instructions with their original constant operand.
  SmallVector<InstInfo, 4> Insts;
  // Index of the last instruction that will be used to split the live range
  // in the dominatee blocks.
  uint64_t LastSplitIdx = UINT64_MAX;

  // Add instruction and update the last split index if needed.
  void addInst(Instruction *I) {
    assert(isa<ConstantInt>(I->getOperand(1)) && "Expected constant operand");

    // Update the index if this instruction is closer to the end of the block.
    if (LastSplitIdx == UINT64_MAX ||
        !I->comesBefore(Insts[LastSplitIdx].first))
      LastSplitIdx = Insts.size();

    Insts.push_back({I, cast<ConstantInt>(I->getOperand(1))->getValue()});
  }

  // Get the last instruction that will be used to split the live range.
  InstInfo &getLastSplitInst() {
    assert(LastSplitIdx < Insts.size() && "Expected valid index");
    return Insts[LastSplitIdx];
  }
};

// Update operands of instruction in InstInfo with the instruction in
// DomInstInfo. Basically, we are doing the following transformation:
//   %Dominator = add %In, DomImm
//   %I         = add %In, Imm
// to
//   %Dominator = add %In, DomImm
//   %I         = add %Dominator, Imm - DomImm
void updateInstOperands(InstSplitInfo::InstInfo &InstInfo,
                        InstSplitInfo::InstInfo &DomInstInfo,
                        const DominatorTree &DT) {
  auto [Inst, Imm] = InstInfo;
  auto [DomInst, DomImm] = DomInstInfo;
  assert(DT.dominates(DomInst, Inst) && "Expected dominator instruction");

  Inst->setOperand(0, DomInst);
  Inst->setOperand(1, ConstantInt::get(Inst->getType(), Imm - DomImm));

  // TODO: Relax this check, since we don't need to drop poison
  // flags in all cases.
  Inst->dropPoisonGeneratingFlags();
}

// This function splits PHI nodes live ranges, if users are add instructions
// with a constant operand. This is useful for loops with large switch
// statements where PHI nodes are used frequently, and we want to keep these
// variables in a single register.
// In case regalloc is not able to keep these variables in a single register,
// we will get something like this in all cases of the switch where variable
// is used:
//   preheader:
//     %r1 = def
//   header:
//     jump @JTI
//   ...
//   bb1:
//     %r2 = add %r1, 1
//     bcc bb2
//   bb2:
//     %r1 = copy %r2 <- regalloc is not able to keep variable in the same reg
//     b latch
//   ...
//   latch:
//     bcc header
//
// Ideally, we would like to have something like this:
//   preheader:
//     %r1 = def
//   header:
//     jump @JTI
//   ...
//   bb1:
//     %r1 = add %r1, 1 <- regalloc managed to keep variable in the same reg
//     bcc bb2
//   bb2:
//                      <- no need for copy instruction
//     b latch
//   ...
//   latch:
//     bcc header
//
// To help regalloc to try to preserve frequently used PHI nodes in a single
// register we are finding add instructions with constant operands that are
// users of the PHI, and changing first operand of the add instruction to the
// nearest dominating add instruction while updating the constant operand. This
// way, regalloc will have a better chance to keep the variable in the same
// register, since we changed the intervals of the variable.
// For example, we are transforming this:
//   header:
//     %phi = phi
//     ...
//   bb1:
//     %add1 = add %phi, 64
//     ...
//     %add2 = add %phi, -64
//     bcc bb2
//   bb2:
//     %add3 = add %phi, -32
//     ...
//     %add4 = add %phi, -96
//     ...
//     b latch
//   latch:
//     bcc header
//
// To this (where add3 and add4 are updated with the nearest dominating add,
// which is add2):
//   header:
//     %phi = phi
//     ...
//   bb1:
//     %add1 = add %phi, 64
//     ...
//     %add2 = add %phi, -64
//     bcc bb2
//   bb2:
//     %add3 = add %add2, 32
//     ...
//     %add4 = add %add2, -32
//     ...
//     b latch
//   latch:
//     bcc header
//
// In order to do so, we are doing the following steps:
//   1. Find all users of the PHI node in the loop that are add instructions
//      with constant operands, and updating the index of the instruction that
//      is closer to the end of the block. This instruction will be used to
//      split ranges in the dominatee blocks.
//   2. For each block, we are finding the nearest dominator block from which
//      we can split the live range.
//   3. Update the instructions in the block with the nearest dominator by
//      changing the first operands to the dominator instruction and updating
//      the constant operands.
static bool splitPHILiveRange(PHINode &Phi, const LoopInfo &LI, const Loop &L,
                              const DominatorTree &DT) {
  assert(Phi.getParent() == L.getHeader() &&
         "Expected PHI node in a loop header");
  DomTreeNode *LoopHeaderNode = DT.getNode(L.getHeader());
  if (!LoopHeaderNode)
    return false;

  DenseMap<BasicBlock *, InstSplitInfo> Splits;
  for (auto *U : Phi.users()) {
    // Only collect add instructions with constant operands.
    auto *UI = cast<Instruction>(U);
    if (!UI || UI->getOpcode() != Instruction::Add ||
        !isa<ConstantInt>(UI->getOperand(1)) ||
        LI.getLoopFor(UI->getParent()) != &L)
      continue;
    Splits[UI->getParent()].addInst(UI);
  }

  // If there are no at least two blocks, we can't split the live range,
  // since we need dominator and dominatee blocks to do so.
  if (Splits.size() < 2)
    return false;

  // Split ranges across blocks. This is done by finding the nearest
  // dominator block, from which we can split the live range.
  bool Changed = false;
  for (auto &[BB, Infos] : Splits) {
    DomTreeNode *Node = DT.getNode(BB);
    if (!Node)
      continue;

    // Find the nearest dominator, from which we can split the live range.
    InstSplitInfo::InstInfo *NearestDominator = nullptr;
    while ((Node = Node->getIDom())) {
      auto I = Splits.find(Node->getBlock());
      if (I != Splits.end()) {
        // We found the nearest dominator block, so take the last
        // instruction from it.
        NearestDominator = &I->second.getLastSplitInst();
        break;
      }

      // Bail out if we reached the start of the loop.
      if (Node == LoopHeaderNode)
        break;
    }

    // If we didn't find any dominator, skip this BB.
    if (!NearestDominator)
      continue;

    // Update instructions in the block with the nearest dominator.
    for (auto &Info : Infos.Insts)
      updateInstOperands(Info, *NearestDominator, DT);

    // TODO: Relax this check, since we don't need to drop poison
    // flags in all cases.
    NearestDominator->first->dropPoisonGeneratingFlags();
    Changed = true;
  }
  return Changed;
}

// This optimization tries to split live ranges of PHI nodes in a loop,
// with a large number of users.
static bool splitLoopPHILiveRanges(Function &F, LoopInfo &LI,
                                   DominatorTree &DT) {
  if (!EnableSplitLoopPHILiveRanges)
    return false;

  bool Changed = false;
  for (auto *L : LI) {
    for (auto &Phi : L->getHeader()->phis()) {
      if (Phi.getNumUses() <= NumOfPHIUsesToSplitLiveRanges)
        continue;
      Changed |= splitPHILiveRange(Phi, LI, *L, DT);
    }
  }
  return Changed;
}

static bool runImpl(Function &F, LoopInfo &LI, DominatorTree &DT) {
  bool Changed = false;
  for (auto &BB : F) {
    for (auto &I : llvm::make_early_inc_range(BB)) {
      switch (I.getOpcode()) {
      default:
        break;
      case Instruction::ICmp:
        Changed |= optimizeICmp(cast<ICmpInst>(I));
        break;
      case Instruction::Br:
        Changed |= optimizeBranch(cast<BranchInst>(&I));
        break;
      }
    }
  }

  Changed |= splitLoopPHILiveRanges(F, LI, DT);
  Changed |= rearrangeOverflowHandlingBranches(F);
  return Changed;
}

bool EraVMPostCodegenPrepare::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  return runImpl(F, LI, DT);
}

char EraVMPostCodegenPrepare::ID = 0;

INITIALIZE_PASS_BEGIN(EraVMPostCodegenPrepare, DEBUG_TYPE,
                      ERAVM_POST_CODEGEN_PREPARE_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(EraVMPostCodegenPrepare, DEBUG_TYPE,
                    ERAVM_POST_CODEGEN_PREPARE_NAME, false, false)

FunctionPass *llvm::createEraVMPostCodegenPreparePass() {
  return new EraVMPostCodegenPrepare();
}

PreservedAnalyses
EraVMPostCodegenPreparePass::run(Function &F, FunctionAnalysisManager &AM) {
  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  if (runImpl(F, LI, DT))
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
