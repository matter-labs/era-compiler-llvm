//===------------- MergeSimilarBB.cpp - Similar BB merging pass -----------===//
//
// This file implements merge of similar basic blocks.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/MergeSimilarBB.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include <utility>
using namespace llvm;

#define DEBUG_TYPE "mergebb"

/// Maximum number of candidate blocks with same hash to consider for merging.
static const unsigned MaxBlockMergeCandidates = 32;

STATISTIC(NumBlocksMerged, "Number of blocks merged");

/// Check whether replacing BB with ReplacementBB would result in a CallBr
/// instruction with a duplicate destination in one of the predecessors.
// FIXME: See note in CodeGenPrepare.cpp.
bool wouldDuplicateCallBrDest(const BasicBlock &BB,
                              const BasicBlock &ReplacementBB) {
  for (const BasicBlock *Pred : predecessors(&BB)) {
    if (auto *CBI = dyn_cast<CallBrInst>(Pred->getTerminator()))
      for (const BasicBlock *Succ : successors(CBI))
        if (&ReplacementBB == Succ)
          return true;
  }
  return false;
}

class HashAccumulator64 {
  uint64_t Hash;

public:
  HashAccumulator64() { Hash = 0x6acaa36bef8325c5ULL; }
  void add(uint64_t V) { Hash = hashing::detail::hash_16_bytes(Hash, V); }
  uint64_t getHash() { return Hash; }
};

static uint64_t hashBlock(const BasicBlock &BB) {
  HashAccumulator64 Acc;
  for (const Instruction &I : BB)
    Acc.add(I.getOpcode());
  for (const BasicBlock *Succ : successors(&BB))
    Acc.add((uintptr_t)Succ);
  return Acc.getHash();
}

static bool canMergeBlocks(const BasicBlock &BB1, const BasicBlock &BB2) {
  // Quickly bail out if successors don't match.
  if (!std::equal(succ_begin(&BB1), succ_end(&BB1), succ_begin(&BB2),
                  [](const BasicBlock* S1, const BasicBlock* S2){return S1 == S2;}))
    return false;

  // Map from instructions in one block to instructions in the other.
  SmallDenseMap<const Instruction *, const Instruction *> Map;
  auto ValuesEqual = [&Map](const Value *V1, const Value *V2) {
    if (V1 == V2)
      return true;

    if (const auto *I1 = dyn_cast<Instruction>(V1))
      if (const auto *I2 = dyn_cast<Instruction>(V2))
        if (const Instruction *Mapped = Map.lookup(I1))
          if (Mapped == I2)
            return true;

    return false;
  };

  auto InstructionsEqual = [&](const Instruction &I1, const Instruction &I2) {
    if (!I1.isSameOperationAs(&I2))
      return false;

    if (const auto *Call = dyn_cast<CallBase>(&I1))
      if (Call->cannotMerge())
        return false;

    if (!std::equal(I1.op_begin(), I1.op_end(), I2.op_begin(), I2.op_end(),
                    ValuesEqual))
      return false;

    if (const PHINode *PHI1 = dyn_cast<PHINode>(&I2)) {
      const PHINode *PHI2 = cast<PHINode>(&I2);
      return std::equal(PHI1->block_begin(), PHI1->block_end(),
                        PHI2->block_begin(), PHI2->block_end());
    }

    if (!I1.use_empty())
      Map.insert({&I1, &I2});
    return true;
  };
  auto It1 = BB1.instructionsWithoutDebug();
  auto It2 = BB2.instructionsWithoutDebug();
  if (!std::equal(It1.begin(), It1.end(), It2.begin(), It2.end(),
                  InstructionsEqual))
    return false;

  // Make sure phi values in successor blocks match.
  for (const BasicBlock *Succ : successors(&BB1)) {
    for (const PHINode &Phi : Succ->phis()) {
      const Value *Incoming1 = Phi.getIncomingValueForBlock(&BB1);
      const Value *Incoming2 = Phi.getIncomingValueForBlock(&BB2);
      if (!ValuesEqual(Incoming1, Incoming2))
        return false;
    }
  }

  if (wouldDuplicateCallBrDest(BB1, BB2))
    return false;

  return true;
}

static bool tryMergeTwoBlocks(BasicBlock &BB1, BasicBlock &BB2) {
  if (!canMergeBlocks(BB1, BB2))
    return false;

  // We will keep BB1 and drop BB2. Merge metadata and attributes.
  for (auto Insts : llvm::zip(BB1, BB2)) {
    Instruction &I1 = std::get<0>(Insts);
    Instruction &I2 = std::get<1>(Insts);

    I1.andIRFlags(&I2);
    combineMetadataForCSE(&I1, &I2, true);
    if (!isa<DbgInfoIntrinsic>(&I1))
      I1.applyMergedLocation(I1.getDebugLoc(), I2.getDebugLoc());
  }

  // Store predecessors, because they will be modified in this loop.
  SmallVector<BasicBlock *, 4> Preds(predecessors(&BB2));
  for (BasicBlock *Pred : Preds)
    Pred->getTerminator()->replaceSuccessorWith(&BB2, &BB1);

  for (BasicBlock *Succ : successors(&BB2))
    Succ->removePredecessor(&BB2);

  BB2.eraseFromParent();
  return true;
}

static bool mergeIdenticalBlocks(Function &F) {
  bool Changed = false;
  SmallDenseMap<uint64_t, SmallVector<BasicBlock *, 2>> SameHashBlocks;

  for (BasicBlock &BB : make_early_inc_range(F)) {
    // The entry block cannot be merged.
    if (&BB == &F.getEntryBlock())
      continue;

    // Identify potential merging candidates based on a basic block hash.
    bool Merged = false;
    auto &Blocks = SameHashBlocks.try_emplace(hashBlock(BB)).first->second;
    for (BasicBlock *Block : Blocks) {
      if (tryMergeTwoBlocks(*Block, BB)) {
        Merged = true;
        ++NumBlocksMerged;
        break;
      }
    }

    Changed |= Merged;
    if (!Merged && Blocks.size() < MaxBlockMergeCandidates)
      Blocks.push_back(&BB);
  }

  // TODO: Merge iteratively.
  return Changed;
}

static bool mergeSimilarBBImpl(Function &F, const TargetTransformInfo &TTI,
                                    DominatorTree *DT) {
  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Eager);

  bool EverChanged = mergeIdenticalBlocks(F);
  EverChanged |= removeUnreachableBlocks(F, DT ? &DTU : nullptr);

  return EverChanged;
}

static bool mergeSimilarBB(Function &F, const TargetTransformInfo &TTI,
                                DominatorTree *DT) {
  assert((!RequireAndPreserveDomTree ||
          (DT && DT->verify(DominatorTree::VerificationLevel::Full))) &&
         "Original domtree is invalid?");

  bool Changed = mergeSimilarBBImpl(F, TTI, DT);

  assert((!RequireAndPreserveDomTree ||
          (DT && DT->verify(DominatorTree::VerificationLevel::Full))) &&
         "Failed to maintain validity of domtree!");

  return Changed;
}

PreservedAnalyses MergeSimilarBB::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  auto &TTI = AM.getResult<TargetIRAnalysis>(F);
  DominatorTree *DT = nullptr;
  if (RequireAndPreserveDomTree)
    DT = &AM.getResult<DominatorTreeAnalysis>(F);
  if (!mergeSimilarBB(F, TTI, DT))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  if (RequireAndPreserveDomTree)
    PA.preserve<DominatorTreeAnalysis>();
  return PA;
}

namespace {
struct MergeSimilarBBPass : public FunctionPass {
  static char ID;
  std::function<bool(const Function &)> PredicateFtor;

  MergeSimilarBBPass() : FunctionPass(ID) {
    initializeMergeSimilarBBPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;

    DominatorTree *DT = nullptr;
    if (RequireAndPreserveDomTree)
      DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
    return mergeSimilarBB(F, TTI, DT);
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    if (RequireAndPreserveDomTree)
      AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    if (RequireAndPreserveDomTree)
      AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
  }
};
}

char MergeSimilarBBPass::ID = 0;
INITIALIZE_PASS_BEGIN(MergeSimilarBBPass, "mergebb", "Merge Similar BB", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(MergeSimilarBBPass, "mergebb", "Merge Similar BB", false,
                    false)

// Public interface to the MergeSimilarBB pass
FunctionPass *
llvm::createMergeSimilarBBPass() {
  return new MergeSimilarBBPass();
}
