// Sha3ConstFolding.cpp - Replace __sha3 call with calculated hash -*- C++ -*-//
//===----------------------------------------------------------------------===//
//
// Replacing of __sha3 (that calucates keccak256 hash) calls with constant
// arguments with the calculated hash values.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/KECCAK.h"

#define DEBUG_TYPE "syncvm-sha3-const-folding"

using namespace llvm;

namespace {

enum OverlapResult {
  OL_Complete,
  OL_MaybePartial,
  OL_None,
  OL_Unknown
};

class SyncVMSha3ConstFolding final : public FunctionPass {
private:
  StringRef getPassName() const override {
    return "SyncVM sha3 constant folding";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<MemorySSAWrapperPass>();
    AU.addPreserved<PostDominatorTreeWrapperPass>();
    AU.addPreserved<MemorySSAWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    FunctionPass::getAnalysisUsage(AU);
  }

  bool runOnFunction(Function &function) override;

public:
  static char ID; // Pass ID
  SyncVMSha3ConstFolding() : FunctionPass(ID) {}
};
} // namespace

static uint64_t getPointerSize(const Value *V, const DataLayout &DL,
                               const TargetLibraryInfo &TLI,
                               const Function *F) {
  uint64_t Size;
  ObjectSizeOpts Opts;
  Opts.NullIsUnknownSize = NullPointerIsDefined(F);

  if (getObjectSize(V, Size, DL, &TLI, Opts))
    return Size;
  return MemoryLocation::UnknownSize;
}

struct MemClobber {
  uint64_t Start;
  uint64_t Size;
  APInt Value;
  MemClobber(uint64_t Start, uint64_t Size, APInt Value)
      : Start(Start), Size(Size), Value(Value) {}
};

struct FoldingState {
  Function &F;
  AliasAnalysis &AA;
  MemorySSA &MSSA;
  DominatorTree &DT;
  PostDominatorTree &PDT;
  const TargetLibraryInfo &TLI;
  const DataLayout &DL;
  const LoopInfo &LI;

  // Whether the function contains any irreducible control flow, useful for
  // being accurately able to detect loops.
  bool ContainsIrreducibleLoops;

  // All the keccak calls to be analyzed.
  SmallVector<MemoryUse *, 16> Sha3MemUses;

  // Post-order numbers for each basic block. Used to figure out if memory
  // accesses are executed before another access.
  DenseMap<BasicBlock *, unsigned> PostOrderNumbers;

  void getUseClobbers(MemoryUse *MU, MemoryLocation Loc,
                      SmallVectorImpl<MemoryDef *> &Clobbers);

  FoldingState(const FoldingState &) = delete;
  FoldingState &operator=(const FoldingState &) = delete;

  FoldingState(Function &F, AliasAnalysis &AA, MemorySSA &MSSA,
               DominatorTree &DT, PostDominatorTree &PDT,
               const TargetLibraryInfo &TLI, const LoopInfo &LI)
      : F(F), AA(AA), MSSA(MSSA), DT(DT), PDT(PDT), TLI(TLI),
        DL(F.getParent()->getDataLayout()), LI(LI) {
    MSSA.ensureOptimizedUses();

    unsigned PO = 0;
    for (BasicBlock *BB : post_order(&F)) {
      PostOrderNumbers[BB] = PO++;
      for (Instruction &I : *BB) {
        CallInst *Call = dyn_cast<CallInst>(&I);
        if (!Call)
          continue;

        Function *Callee = Call->getCalledFunction();
        if (!Callee || (Callee->getName() != "__keccak256"))
          continue;

        MemoryAccess *MA = MSSA.getMemoryAccess(Call);
        auto *MU = dyn_cast_or_null<MemoryUse>(MA);
        // keccak256() is passed a pointer in the first argument
        // and the memory size in the second one. It's expected that the
        // memory size to be a constant expression. In this case
        // the memory location should be precise.
        if (MU && getLocForSha3Call(Call).Size.isPrecise())
          Sha3MemUses.push_back(MU);
      }
    }

    // Collect whether there is any irreducible control flow in the function.
    ContainsIrreducibleLoops = mayContainIrreducibleControl(F, &LI);
  }

  MemoryLocation getLocForSha3Call(const CallInst *I) const {
    return MemoryLocation::getForArgument(I, 0, TLI);
  }

  /// Collect all the potential clobbering \p Loc definitions
  /// for a given \p Use.
  void collectUseClobbers(MemoryUse *MU, MemoryLocation Loc,
                          SmallVectorImpl<MemoryDef *> &Clobbers) {
    MemorySSAWalker *Walker = MSSA.getWalker();
    SmallVector<MemoryAccess *> WorkList{Walker->getClobberingMemoryAccess(MU)};
    SmallSet<MemoryAccess *, 8> Visited;

    // Start with a nearest dominating clobbering access, it will be either
    // live on entry (nothing to do, load is not clobbered), MemoryDef, or
    // MemoryPhi if several MemoryDefs can define this memory state. In that
    // case stop the search.
    // FIXME: we can relax this constraint in the future if it turns out we miss
    // optimization opportunities.
    while (!WorkList.empty()) {
      MemoryAccess *MA = WorkList.pop_back_val();
      if (!Visited.insert(MA).second)
        continue;

      if (MSSA.isLiveOnEntryDef(MA))
        continue;

      if (MemoryDef *Def = dyn_cast<MemoryDef>(MA)) {
        Clobbers.push_back(Def);
        WorkList.push_back(
            Walker->getClobberingMemoryAccess(Def->getDefiningAccess(), Loc));
        continue;
      }

      if (isa<MemoryPhi>(MA))
        break;
    }
  }

  /// Returns true if a dependency between \p Clobber and \p MemUse is
  /// guaranteed to be loop invariant for the loops that they are in. Either
  /// because they are known to be in the same block, in the same loop level or
  /// by guaranteeing that \p CurrentLoc only references a single MemoryLocation
  /// during execution of the containing function.
  bool isGuaranteedLoopIndependent(const Instruction *MemUse,
                                   const Instruction *Clobber,
                                   const MemoryLocation &MemUseLoc) {
    // If the dependency is within the same block or loop level (being careful
    // of irreducible loops), we know that AA will return a valid result for the
    // memory dependency. (Both at the function level, outside of any loop,
    // would also be valid but we currently disable that to limit compile time).
    if (MemUse->getParent() == Clobber->getParent())
      return true;
    const Loop *CurrentLI = LI.getLoopFor(MemUse->getParent());
    if (!ContainsIrreducibleLoops && CurrentLI &&
        CurrentLI == LI.getLoopFor(Clobber->getParent()))
      return true;
    // Otherwise check the memory location is invariant to any loops.
    return isGuaranteedLoopInvariant(MemUseLoc.Ptr);
  }

  /// Returns true if \p Ptr is guaranteed to be loop invariant for any possible
  /// loop. In particular, this guarantees that it only references a single
  /// MemoryLocation during execution of the containing function.
  bool isGuaranteedLoopInvariant(const Value *Ptr) {
    Ptr = Ptr->stripPointerCasts();
    if (auto *GEP = dyn_cast<GEPOperator>(Ptr))
      if (GEP->hasAllConstantIndices())
        Ptr = GEP->getPointerOperand()->stripPointerCasts();

    if (auto *I = dyn_cast<Instruction>(Ptr))
      return I->getParent()->isEntryBlock();
    return true;
  }

  /// Return 'OW_Complete' if a store to the 'ClobberLoc' location (by \p
  /// Clobber instruction) is completely within the 'MemUseLoc'
  /// location (by \p MemUseLoc). Return 'OW_MaybePartial' if \p ClobberLoc
  /// is partially with the \p MemUseLoc they both refer to the same underlying
  /// object. Returns 'OR_None' if \p Clobber is known to not write to the
  /// memory reffering by \p MemUse. Returns 'OW_Unknown' if nothing can be
  /// determined.
  OverlapResult isCompleteOverlap(const Instruction *MemUse,
                              const Instruction *Clobber,
                              const MemoryLocation &MemUseLoc,
                              const MemoryLocation &ClobberLoc,
                              int64_t &MemUseOff, int64_t &ClobberOff) {
    // AliasAnalysis does not always account for loops. Limit overlap checks
    // to dependencies for which we can guarantee they are independent of any
    // loops they are in.
    if (!isGuaranteedLoopIndependent(Clobber, MemUse, ClobberLoc))
      return OL_Unknown;

    const Value *ClobberPtr = ClobberLoc.Ptr->stripPointerCasts();
    const Value *MemUsePtr = MemUseLoc.Ptr->stripPointerCasts();
    const Value *ClobberUndObj = getUnderlyingObject(ClobberPtr);
    const Value *MemUseUndObj = getUnderlyingObject(MemUsePtr);
    const uint64_t MemUseSize = MemUseLoc.Size.getValue();
    const uint64_t ClobberSize = ClobberLoc.Size.getValue();

    ClobberOff = MemUseOff = 0;

    // Try to conver a pointer to the integer value.
    auto getPointerAsInt = [](const Value *Ptr) -> std::optional<uint64_t> {
      if (auto *CE = dyn_cast<ConstantExpr>(Ptr)) {
        if (CE->getOpcode() == Instruction::IntToPtr) {
          const Value *C = CE->op_begin()->get();
          assert(isa<ConstantInt>(C));
          return cast<ConstantInt>(C)->getZExtValue();
        }
      } else if (isa<ConstantPointerNull>(Ptr)) {
        return UINT64_C(0);
      }
      return std::nullopt;
    };

    // If both clobber and memuse pointers are constant we expect two cases:
    //  - pointer is just a nullptr
    //  - pointer is a cast of an integer constant
    if (isa<Constant>(ClobberPtr) && isa<Constant>(MemUsePtr)) {
      if (!getPointerAsInt(ClobberPtr) || !getPointerAsInt(MemUsePtr))
        return OL_Unknown;

      uint64_t ClobberPtrInt = *getPointerAsInt(ClobberPtr);
      uint64_t MemUsePtrInt = *getPointerAsInt(MemUsePtr);
      if (MemUsePtrInt <= ClobberPtrInt &&
          (ClobberPtrInt + ClobberSize) <= (MemUsePtrInt + MemUseSize)) {
        MemUseOff = static_cast<uint64_t>(MemUsePtrInt);
        ClobberOff = static_cast<uint64_t>(ClobberPtrInt);
        return OL_Complete;
      }
    }

    // Check whether the clobber store overwrites the whole object, in which
    // case the size/offset of the dead store does not matter.
    if (ClobberUndObj == MemUseUndObj) {
      uint64_t MemUseUndObjSize = getPointerSize(MemUseUndObj, DL, TLI, &F);
      if (MemUseUndObjSize != MemoryLocation::UnknownSize &&
          MemUseUndObjSize == MemUseSize && MemUseSize == ClobberSize)
        return OL_Complete;
    }

    // Query the alias information
    AliasResult AAR = AA.alias(MemUseLoc, ClobberLoc);

    // If the start pointers are the same, we just have to compare sizes to see
    // if the memuse was larger than the clobber.
    if (AAR == AliasResult::MustAlias) {
      // Make sure that the MemUseSize size is >= the ClobberSize size.
      if (MemUseSize >= ClobberSize)
        return OL_Complete;
    }

    // If we hit a partial alias we may have a full overwrite
    if (AAR == AliasResult::PartialAlias && AAR.hasOffset()) {
      int32_t Off = AAR.getOffset();
      if (Off >= 0 && static_cast<uint64_t>(Off) + ClobberSize <= MemUseSize)
        return OL_Complete;
    }

    // If we can't resolve the same pointers to the same object, then we can't
    // analyze them at all.
    if (MemUseUndObj != ClobberUndObj) {
      if (AAR == AliasResult::NoAlias)
        return OL_None;
      return OL_Unknown;
    }

    // Okay, we have stores to two completely different pointers.  Try to
    // decompose the pointer into a "base + constant_offset" form.  If the base
    // pointers are equal, then we can reason about the memuse and the clobber.
    const Value *ClobberBasePtr =
        GetPointerBaseWithConstantOffset(ClobberPtr, ClobberOff, DL);
    const Value *MemUseBasePtr =
        GetPointerBaseWithConstantOffset(MemUsePtr, MemUseOff, DL);

    // If the base pointers still differ, we have two completely different
    // stores.
    if (ClobberBasePtr != MemUseBasePtr)
      return OL_Unknown;

    // The killing access completely overlaps the dead store if and only if
    // both start and end of the dead one is "inside" the killing one:
    //    |<->|--clobber-|<->|
    //    |------MemUse------|
    // We have to be careful here as *Off is signed while *.Size is unsigned.

    // Check if the clobber access starts "not before" the memuse one.
    if (ClobberOff >= MemUseOff) {
      // If the clobber access ends "not after" the memuse access then the
      // clobber one is completely overlaped by the memuse one.
      if (uint64_t(ClobberOff - MemUseOff) + ClobberSize <= MemUseSize) {
        MemUseOff = ClobberOff - MemUseOff;
        ClobberOff = 0;
        return OL_Complete;
      }
      // If start of the clobber access is "before" end of the memuse access
      // then accesses overlap.
      else if ((uint64_t)(ClobberOff - MemUseOff) < MemUseSize)
        return OL_MaybePartial;
    }
    // If start of the memuse access is "before" end of the clobber access then
    // accesses overlap.
    else if ((uint64_t)(MemUseOff - ClobberOff) < ClobberSize) {
      return OL_MaybePartial;
    }

    // Can reach here only if accesses are known not to overlap.
    return OL_None;
  }

  // Sort memory clobbers is the ascending order of their starting addresses
  // Ensure they do not overlap with each other and thier total size is equal
  // to the data size of __sha3.
  bool sortAndCheckMemoryClobbers(SmallVector<MemClobber> &MemClobbers,
                                  const uint64_t SHA3MemSize) {
    std::sort(
        MemClobbers.begin(), MemClobbers.end(),
        [](const auto &Lhs, const auto &Rhs) { return Lhs.Start < Rhs.Start; });
    // Ensure the array of clobbers forms a continuous memory region.
    uint64_t TotalSize = 0;
    auto Begin = MemClobbers.begin(), End = MemClobbers.end();
    auto It = Begin, ItPrev = Begin;
    for (; It != End; ++It) {
      TotalSize += It->Size;
      if (It == Begin)
        continue;

      if ((ItPrev->Start + ItPrev->Size) != It->Start)
        return false;

      ++ItPrev;
    }

    return TotalSize == SHA3MemSize;
  }

  /// Walk through the memory segments trying to build continuous memory area
  /// with the data for which we compute sha3 hash.
  Value *getKeccak256Hash(const CallInst *Call,
                          SmallVectorImpl<MemoryDef *> &MemDefs) {
    uint64_t ToatalSizeOfClobbers = 0;
    SmallVector<MemClobber> MemClobbers;
    auto UseMemLoc = getLocForSha3Call(Call);
    const uint64_t UseMemSize = UseMemLoc.Size.getValue();

    for (MemoryDef *MemDef : MemDefs) {
      const StoreInst *SI = cast<StoreInst>(MemDef->getMemoryInst());
      LLVM_DEBUG(dbgs() << "  found clobber: " << *SI << '\n');

      MemoryLocation ClobberLoc = MemoryLocation::get(SI);
      if (!ClobberLoc.Size.isPrecise())
        return nullptr;

      int64_t MemUseOff = 0, ClobberOff = 0;
      auto OverlapRes = isCompleteOverlap(Call, SI, UseMemLoc, ClobberLoc,
                                          MemUseOff, ClobberOff);

      // This clobber doesn't write to the memory __sha3 is reading from.
      // Just skip it and come to the next clobber.
      if (OverlapRes == OL_None)
        continue;

      // We give up in these cases, as it's  difficult or impossible
      // to determine the full memory data for sha3.
      if (OverlapRes == OL_MaybePartial || OverlapRes == OL_Unknown)
        return nullptr;

      // Handle OL_Complete. Try to add new clobber to the memory clobbers.

      // We cannot perform constant folding, if the stored value is not
      // a constant expressiosn.
      auto ClobberVal = dyn_cast<ConstantInt>(SI->getValueOperand());
      if (!ClobberVal)
        return nullptr;

      uint64_t ClobberSize = ClobberLoc.Size.getValue();
      ToatalSizeOfClobbers += ClobberSize;
      assert(ClobberSize * 8 == ClobberVal->getBitWidth());

      MemClobbers.emplace_back(ClobberOff, ClobberSize, ClobberVal->getValue());

      // If we have added clobbers that fully cover the sha3 memory area,
      // sort them in the ascending order of their starting addresses and
      // perform several checks.
      if (ToatalSizeOfClobbers < UseMemSize)
        continue;

      if (!sortAndCheckMemoryClobbers(MemClobbers, UseMemSize))
        return nullptr;

      SmallVector<char, 512> MemBuf;
      raw_svector_ostream OS(MemBuf);
      // Put all the clobber values to the buffer in the BE order.
      for (const auto &Clobber : MemClobbers) {
        // Words of the APInt are in the LE order, so we need to
        // iterate them starting from the end.
        const auto *ValRawData = Clobber.Value.getRawData();
        for (unsigned I = 0, E = Clobber.Value.getNumWords(); I != E; ++I)
          support::endian::write<APInt::WordType>(OS, ValRawData[E - I - 1],
                                                  support::endianness::big);
      }
      LLVM_DEBUG(dbgs() << "  input sha3 data: " << toHex(OS.str()) << '\n');
      auto Hash = KECCAK::KECCAK_256(OS.str());
      LLVM_DEBUG(dbgs() << "  keccak256 hash: " << toHex(Hash) << '\n');
      assert(Call->getType()->getIntegerBitWidth() == 256);
      Value *HashVal =
          ConstantInt::get(Call->getType(), APInt(256, toHex(Hash), 16));
      return HashVal;
    }

    return nullptr;
  }

}; // end FoldingState struct

static bool runSyncVMSha3ConstFolding(Function &F, AliasAnalysis &AA,
                                      MemorySSA &MSSA, DominatorTree &DT,
                                      PostDominatorTree &PDT,
                                      const TargetLibraryInfo &TLI,
                                      const LoopInfo &LI) {
  FoldingState State(F, AA, MSSA, DT, PDT, TLI, LI);
  bool Changed = false;
  for (MemoryUse *Sha3MemUse : State.Sha3MemUses) {
    SmallVector<MemoryDef *, 8> Clobbers;
    auto *Sha3Call = dyn_cast<CallInst>(Sha3MemUse->getMemoryInst());
    assert(Sha3Call != nullptr);

    auto Sha3MemLoc = State.getLocForSha3Call(Sha3Call);
    State.collectUseClobbers(Sha3MemUse, Sha3MemLoc, Clobbers);

    LLVM_DEBUG(dbgs() << "Analyzing: " << *Sha3Call << '\n');

    if (Value *HashVal = State.getKeccak256Hash(Sha3Call, Clobbers)) {
      Sha3Call->replaceAllUsesWith(HashVal);
      Sha3Call->eraseFromParent();
      Changed |= true;

      LLVM_DEBUG(dbgs() << "  replacing with the value: " << *HashVal << '\n');
    }
  }
  return Changed;
}

bool SyncVMSha3ConstFolding::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  const TargetLibraryInfo &TLI =
      getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  MemorySSA &MSSA = getAnalysis<MemorySSAWrapperPass>().getMSSA();
  PostDominatorTree &PDT =
      getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  return runSyncVMSha3ConstFolding(F, AA, MSSA, DT, PDT, TLI, LI);
}

char SyncVMSha3ConstFolding::ID = 0;

INITIALIZE_PASS(
    SyncVMSha3ConstFolding, "syncvm-keccak-const-folding",
    "Replace 'keccak' calls with calculated hash values if arguments are known",
    false, false)

FunctionPass *llvm::createSyncVMSha3ConstFoldingPass() {
  return new SyncVMSha3ConstFolding;
}

PreservedAnalyses SyncVMSha3ConstFoldingPass::run(Function &F,
                                                  FunctionAnalysisManager &AM) {
  AliasAnalysis &AA = AM.getResult<AAManager>(F);
  const TargetLibraryInfo &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  MemorySSA &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();
  PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
  LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

  return runSyncVMSha3ConstFolding(F, AA, MSSA, DT, PDT, TLI, LI)
             ? PreservedAnalyses::none()
             : PreservedAnalyses::all();
}
