// SHA3ConstFolding.cpp - Replace __sha3 call with calculated hash -*- C++ -*-//
//===----------------------------------------------------------------------===//
//
// The code below tries to replace __sha3 (that calucates keccak256 hash) calls
// with the calculated hash values.
//
// It uses the following general approach: given a __sha3 call (MemoryUse),
// walk upwards to find store instructions (clobbers) with
// constant values that fully define data (in memory) for which we compute hash.
// Only __sha3 calls with the constant 'size' argument are checked.
//
// For example:
//
//   store i256 1, ptr addrspace(1) null
//   store i256 2, ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1))
//   %hash = tail call i256 @__sha3(ptr addrspace(1) null, i256 64, i1 true)
//   ret i256 %hash
//
// is transformed into:
//
//   store i256 1, ptr addrspace(1) null
//   store i256 2, ptr addrspace(1) inttoptr (i256 32 to ptr addrspace(1))
//   ret i256 -536754096339594166047489462334966539640...
//
// A bit more concretely:
//
// For all __sha3 calls:
// 1. Collect potentially dominating clobbering MemoryDefs by walking upwards.
//    Check that clobbering values are constants, otherwise bail out.
//
// 2. Check that
//   1. Each clobber is withing the __sha3 memory location:
//     |<->|--clobber--|<->|
//     |------MemUse-------|
//   2. Clobbers are not intersected with each other:
//     |--cl1--|
//             |cl2|
//                 |--cl3--|
//     |------MemUse-------|
//   3.Collect collber values
//
// 3. Create a memory array from the collected values and calculate
//    the Keccak256 hash.
//
// 4. Run simplification for each instruction in the function, as __sha3 folding
//    can provide new opportunities for this.
//
// 5. If the simplification has changed the function, run one more iteration
//    of the whole process starting from p.1.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsSyncVM.h"
#include "llvm/Support/DebugCounter.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/KECCAK.h"
#include "llvm/Transforms/Utils/AssumeBundleBuilder.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-sha3-const-folding"

STATISTIC(NumSHA3Folded, "Number of __sha3 calls folded");

DEBUG_COUNTER(SHA3Counter, "syncvm-sha3-const-folding",
              "Controls which instructions are removed");

namespace {

enum OverlapResult { OL_Complete, OL_MaybePartial, OL_None, OL_Unknown };

class SyncVMSHA3ConstFolding final : public FunctionPass {
private:
  StringRef getPassName() const override {
    return "SyncVM sha3 constant folding";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<MemorySSAWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
    AU.addPreserved<MemorySSAWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    FunctionPass::getAnalysisUsage(AU);
  }

  bool runOnFunction(Function &function) override;

public:
  static char ID; // Pass ID
  SyncVMSHA3ConstFolding() : FunctionPass(ID) {}
};

uint64_t getPointerSize(const Value *V, const DataLayout &DL,
                        const TargetLibraryInfo &TLI, const Function *F) {
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
  AssumptionCache &AC;
  MemorySSA &MSSA;
  std::unique_ptr<MemorySSAUpdater> MSSAUpdater;
  DominatorTree &DT;
  const TargetLibraryInfo &TLI;
  const DataLayout &DL;
  const SimplifyQuery SQ;
  const LoopInfo &LI;

  // Whether the function contains any irreducible control flow, useful for
  // being accurately able to detect loops.
  bool ContainsIrreducibleLoops;

  // All the sha3 calls to be analyzed.
  SmallVector<MemoryUse *, 16> SHA3MemUses;

  void getUseClobbers(MemoryUse *MU, MemoryLocation Loc,
                      SmallVectorImpl<MemoryDef *> &Clobbers);

  FoldingState(const FoldingState &) = delete;
  FoldingState &operator=(const FoldingState &) = delete;

  FoldingState(Function &F, AliasAnalysis &AA, AssumptionCache &AC,
               MemorySSA &MSSA, DominatorTree &DT, const TargetLibraryInfo &TLI,
               const LoopInfo &LI)
      : F(F), AA(AA), AC(AC), MSSA(MSSA),
        MSSAUpdater(std::make_unique<MemorySSAUpdater>(&MSSA)), DT(DT),
        TLI(TLI), DL(F.getParent()->getDataLayout()), SQ(DL, &TLI, &DT, &AC),
        LI(LI) {
    MSSA.ensureOptimizedUses();

    const ReversePostOrderTraversal<BasicBlock *> RPOT(&*F.begin());
    for (BasicBlock *BB : RPOT) {
      for (Instruction &I : *BB) {
        CallInst *Call = dyn_cast<CallInst>(&I);
        if (!Call)
          continue;

        Function *Callee = Call->getCalledFunction();
        if (!Callee)
          continue;

        LibFunc Func = NotLibFunc;
        const StringRef Name = Callee->getName();
        if (!TLI.getLibFunc(Name, Func) || !TLI.has(Func) ||
            Func != LibFunc_xvm_sha3)
          continue;

        MemoryAccess *MA = MSSA.getMemoryAccess(Call);
        auto *MU = dyn_cast_or_null<MemoryUse>(MA);
        // __sha3() is passed a pointer in the first argument
        // and the memory size in the second one. It's expected that the
        // memory size to be a constant expression. In this case
        // the memory location should be precise.
        if (MU && getLocForSHA3Call(Call).Size.isPrecise())
          SHA3MemUses.push_back(MU);
      }
    }

    // Collect whether there is any irreducible control flow in the function.
    ContainsIrreducibleLoops = mayContainIrreducibleControl(F, &LI);
  }

  MemoryLocation getLocForSHA3Call(const CallInst *I) const {
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

  /// Exclude the instruction from MSSA if it's going to be removed from a BB.
  void removeFromMSSA(Instruction *Inst) {
    if (VerifyMemorySSA)
      MSSA.verifyMemorySSA();
    MSSAUpdater->removeMemoryAccess(Inst, true);
  }

  /// Try to simplify instructions of the functions. Candidates for
  /// simplification may appear after replacing __sha3 call with the
  /// calculated hash value.
  bool simplifyInstructions() {
    bool Changed = false;
    for (auto &Inst : make_early_inc_range(instructions(F))) {
      if (Value *V = simplifyInstruction(&Inst, SQ)) {
        LLVM_DEBUG(dbgs() << " SyncVMSHA3ConstFolding simplify: " << Inst
                          << "  to: " << *V << '\n');
        if (!DebugCounter::shouldExecute(SHA3Counter)) {
          LLVM_DEBUG(dbgs() << "Skipping due to debug counter\n");
          continue;
        }

        if (!Inst.use_empty()) {
          Inst.replaceAllUsesWith(V);
          Changed = true;
        }
        if (isInstructionTriviallyDead(&Inst, &TLI)) {
          removeFromMSSA(&Inst);
          salvageKnowledge(&Inst, &AC);
          Inst.eraseFromParent();
          Changed = true;
        }
      }
    }
    return Changed;
  }

  /// Returns true if a dependency between \p Clobber and \p MemUse is
  /// guaranteed to be loop invariant for the loops that they are in. Either
  /// because they are known to be in the same block, in the same loop level or
  /// by guaranteeing that \p MemUseLoc only references a single MemoryLocation
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

  /// Return 'OW_Complete' if a store to the \p ClobberLoc location (by \p
  /// Clobber instruction) is completely within the \p MemUseLoc
  /// location (by \p MemUseLoc). Return 'OW_MaybePartial' if \p ClobberLoc
  /// is partially with the \p MemUseLoc they both refer to the same underlying
  /// object. Returns 'OR_None' if \p Clobber is known to not write to the
  /// memory referring by \p MemUse. Returns 'OW_Unknown' if nothing can be
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

    // Try to convert a pointer to the integer value.
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

      const uint64_t ClobberPtrInt = *getPointerAsInt(ClobberPtr);
      const uint64_t MemUsePtrInt = *getPointerAsInt(MemUsePtr);
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
      const uint64_t MemUseUndObjSize =
          getPointerSize(MemUseUndObj, DL, TLI, &F);
      if (MemUseUndObjSize != MemoryLocation::UnknownSize &&
          MemUseUndObjSize == MemUseSize && MemUseSize == ClobberSize)
        return OL_Complete;
    }

    // Query the alias information
    const AliasResult AAR = AA.alias(MemUseLoc, ClobberLoc);

    // If the start pointers are the same, we just have to compare sizes to see
    // if the memuse was larger than the clobber.
    if (AAR == AliasResult::MustAlias) {
      // Make sure that the MemUseSize size is >= the ClobberSize size.
      if (MemUseSize >= ClobberSize)
        return OL_Complete;
    }

    // If we hit a partial alias we may have a full overwrite
    if (AAR == AliasResult::PartialAlias && AAR.hasOffset()) {
      const int32_t Off = AAR.getOffset();
      if (Off >= 0 && static_cast<uint64_t>(Off) + ClobberSize <= MemUseSize) {
        ClobberOff = static_cast<uint64_t>(Off);
        return OL_Complete;
      }
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
      // clobber one is completely overlapped by the memuse one.
      if (uint64_t(ClobberOff - MemUseOff) + ClobberSize <= MemUseSize) {
        MemUseOff = ClobberOff - MemUseOff;
        ClobberOff = 0;
        return OL_Complete;
      }
      // If start of the clobber access is "before" end of the memuse access
      // then accesses overlap.
      if ((uint64_t)(ClobberOff - MemUseOff) < MemUseSize)
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

  /// Sort memory clobbers is the ascending order of their starting addresses
  /// Ensure they do not overlap with each other.
  bool sortAndCheckMemoryClobbers(SmallVector<MemClobber> &MemClobbers,
                                  const uint64_t SHA3MemSize) {
    std::sort(
        MemClobbers.begin(), MemClobbers.end(),
        [](const auto &Lhs, const auto &Rhs) { return Lhs.Start < Rhs.Start; });
    // Ensure the array of clobbers forms a continuous memory region.
#ifndef NDEBUG
    uint64_t TotalSize = 0;
#endif
    auto Begin = MemClobbers.begin(), End = MemClobbers.end();
    auto It = Begin, ItPrev = Begin;
    for (; It != End; ++It) {
#ifndef NDEBUG
      TotalSize += It->Size;
#endif
      if (It == Begin)
        continue;

      if ((ItPrev->Start + ItPrev->Size) != It->Start) {
        LLVM_DEBUG(dbgs() << "\tclobbers do alias: [" << ItPrev->Start << ", "
                          << ItPrev->Start + ItPrev->Size << "] -> ["
                          << It->Start << ", " << It->Start + It->Size << "]"
                          << '\n');
        return false;
      }

      ++ItPrev;
    }
    assert(TotalSize == SHA3MemSize);

    return true;
  }

  /// Check if we can ignore non store clobber instruction that doesn't actually
  /// clobber heap memory. For example, a memcpy to AS other than heap.
  bool shouldSkipClobber(const Instruction *MemInst) {
    if (const auto *Intr = dyn_cast<AnyMemIntrinsic>(MemInst))
      return Intr->getDestAddressSpace() != SyncVMAS::AS_HEAP;

    return false;
  }

  /// Walk through the memory segments passed in \p MemDefs trying to build
  /// continuous memory area with the data for which we compute keccak256 hash.
  Value *getKeccak256Hash(const CallInst *Call,
                          SmallVectorImpl<MemoryDef *> &MemDefs) {
    uint64_t TotalSizeOfClobbers = 0;
    SmallVector<MemClobber> MemClobbers;
    auto UseMemLoc = getLocForSHA3Call(Call);
    const uint64_t UseMemSize = UseMemLoc.Size.getValue();

    for (MemoryDef *MemDef : MemDefs) {
      const auto *MemInstr = MemDef->getMemoryInst();
      if (shouldSkipClobber(MemInstr))
        continue;

      const StoreInst *SI = dyn_cast<StoreInst>(MemInstr);
      if (!SI) {
        LLVM_DEBUG(dbgs() << "\tunknown clobber: " << *MemInstr << '\n');
        return nullptr;
      }

      LLVM_DEBUG(dbgs() << "\tfound clobber: " << *SI << '\n');

      const MemoryLocation ClobberLoc = MemoryLocation::get(SI);
      if (!ClobberLoc.Size.isPrecise())
        return nullptr;

      int64_t MemUseOff = 0, ClobberOff = 0;
      auto OverlapRes = isCompleteOverlap(Call, SI, UseMemLoc, ClobberLoc,
                                          MemUseOff, ClobberOff);

      // This clobber doesn't write to the memory __sha3 is reading from.
      // Just skip it and come to the next clobber.
      if (OverlapRes == OL_None) {
        LLVM_DEBUG(dbgs() << "\t\twrites out of sha3 memory, offset: "
                          << ClobberOff << '\n');
        continue;
      }

      // We give up in these cases, as it's  difficult or impossible
      // to determine the full memory data for sha3.
      if (OverlapRes == OL_MaybePartial || OverlapRes == OL_Unknown) {
        LLVM_DEBUG(dbgs() << "\t\tpartially or unknow overlap "
                          << static_cast<int>(OverlapRes) << '\n');
        return nullptr;
      }

      // Handle OL_Complete. Try to add new clobber to the memory clobbers.

      // We cannot perform constant folding, if the stored value is not
      // a constant expression.
      const auto *ClobberVal = dyn_cast<ConstantInt>(SI->getValueOperand());
      if (!ClobberVal) {
        LLVM_DEBUG(dbgs() << "\t\tstored value isn't constant" << '\n');
        return nullptr;
      }

      const uint64_t ClobberSize = ClobberLoc.Size.getValue();
      // If we have already seen a clobber with the same start position
      // and the size, we can ignore the current clobber, as it's killed
      // by existing one.
      // TODO: needs to check for flaws in this logic. In fact this is
      // similar to what DSE pass does.
      if (std::any_of(MemClobbers.begin(), MemClobbers.end(),
                      [ClobberOff, ClobberSize](const auto &C) {
                        return C.Start == static_cast<uint64_t>(ClobberOff) &&
                               C.Size == static_cast<uint64_t>(ClobberSize);
                      })) {
        LLVM_DEBUG(dbgs() << "\t\tstored value is killed by the"
                             "consequent clobber"
                          << '\n');
        continue;
      }

      TotalSizeOfClobbers += ClobberSize;
      assert(ClobberSize * 8 == ClobberVal->getBitWidth());

      MemClobbers.emplace_back(ClobberOff, ClobberSize, ClobberVal->getValue());

      // If we have got clobbers that fully cover the sha3 memory area,
      // sort them in the ascending order of their starting addresses and
      // perform several checks.
      if (TotalSizeOfClobbers < UseMemSize)
        continue;

      if (!sortAndCheckMemoryClobbers(MemClobbers, UseMemSize)) {
        return nullptr;
      }

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
      LLVM_DEBUG(dbgs() << "\tinput sha3 data: " << toHex(OS.str()) << '\n');

      auto Hash = KECCAK::KECCAK_256(OS.str());

      LLVM_DEBUG(dbgs() << "\tkeccak256 hash: " << toHex(Hash) << '\n');
      assert(Call->getType()->getIntegerBitWidth() == 256);

      Value *HashVal =
          ConstantInt::get(Call->getType(), APInt(256, toHex(Hash), 16));
      return HashVal;
    }

    LLVM_DEBUG(dbgs() << "\tcouldn't find enough clobbers that would fully"
                         "cover sha3 memory"
                      << '\n');
    return nullptr;
  }

}; // end FoldingState struct

bool runSyncVMSHA3ConstFolding(Function &F, AliasAnalysis &AA,
                               AssumptionCache &AC, MemorySSA &MSSA,
                               DominatorTree &DT, const TargetLibraryInfo &TLI,
                               const LoopInfo &LI) {
  LLVM_DEBUG(dbgs() << "********** SyncVM sha3 constant folding **********\n"
                    << "********** Function: " << F.getName() << '\n');

  FoldingState State(F, AA, AC, MSSA, DT, TLI, LI);
  SmallSet<MemoryUse *, 16> RemovedSHA3;

  bool Changed = false;
  for (;;) {
    LLVM_DEBUG(dbgs() << "Running new iteration of sha3 constant folding...\n");

    bool ChangedOnIter = false;
    for (MemoryUse *SHA3MemUse : State.SHA3MemUses) {
      if (RemovedSHA3.count(SHA3MemUse))
        continue;

      SmallVector<MemoryDef *, 8> Clobbers;
      auto *SHA3Call = dyn_cast<CallInst>(SHA3MemUse->getMemoryInst());
      assert(SHA3Call != nullptr);

      auto SHA3MemLoc = State.getLocForSHA3Call(SHA3Call);
      State.collectUseClobbers(SHA3MemUse, SHA3MemLoc, Clobbers);

      LLVM_DEBUG(dbgs() << "Analyzing: " << *SHA3Call << '\n');

      if (Value *HashVal = State.getKeccak256Hash(SHA3Call, Clobbers)) {
        if (!DebugCounter::shouldExecute(SHA3Counter)) {
          LLVM_DEBUG(dbgs() << "Skipping due to debug counter\n");
          return Changed;
        }

        SHA3Call->replaceAllUsesWith(HashVal);
        State.removeFromMSSA(SHA3Call);
        SHA3Call->eraseFromParent();
        RemovedSHA3.insert(SHA3MemUse);
        Changed = ChangedOnIter = true;
        NumSHA3Folded++;

        LLVM_DEBUG(dbgs() << "\treplacing with the value: " << *HashVal
                          << '\n');
      }
    }
    // If we simplified some instructions after folding __sha3 calls,
    // run the folding again, as there may be new opportunities for this.
    if (!ChangedOnIter || !State.simplifyInstructions())
      break;
  }
  return Changed;
}

} // end anonymous namespace

bool SyncVMSHA3ConstFolding::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
  auto &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  const auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  auto &MSSA = getAnalysis<MemorySSAWrapperPass>().getMSSA();
  auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  return runSyncVMSHA3ConstFolding(F, AA, AC, MSSA, DT, TLI, LI);
}

char SyncVMSHA3ConstFolding::ID = 0;

INITIALIZE_PASS(
    SyncVMSHA3ConstFolding, "syncvm-sha3-const-folding",
    "Replace '__sha3' calls with calculated hash values if arguments are known",
    false, false)

FunctionPass *llvm::createSyncVMSHA3ConstFoldingPass() {
  return new SyncVMSHA3ConstFolding;
}

PreservedAnalyses SyncVMSHA3ConstFoldingPass::run(Function &F,
                                                  FunctionAnalysisManager &AM) {
  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  auto &AA = AM.getResult<AAManager>(F);
  const auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();
  auto &LI = AM.getResult<LoopAnalysis>(F);

  return runSyncVMSHA3ConstFolding(F, AA, AC, MSSA, DT, TLI, LI)
             ? PreservedAnalyses::none()
             : PreservedAnalyses::all();
}
