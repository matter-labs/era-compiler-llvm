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
//        |--clobber--|
//     |------MemUse-------|
//   2. Clobbers are not intersected with each other:
//     |--cl1--|
//             |cl2|
//                 |--cl3--|
//     |------MemUse-------|
//   3.Collect clobber values
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

#include "EraVM.h"

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
#include "llvm/IR/IntrinsicsEraVM.h"
#include "llvm/Support/DebugCounter.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/KECCAK.h"
#include "llvm/Transforms/Utils/AssumeBundleBuilder.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-sha3-constant-folding"

STATISTIC(NumSHA3Folded, "Number of __sha3 calls folded");

DEBUG_COUNTER(SHA3Counter, "eravm-sha3-constant-folding",
              "Controls which instructions are removed");

namespace {

class EraVMSHA3ConstFolding final : public FunctionPass {
private:
  StringRef getPassName() const override {
    return "EraVM sha3 constant folding";
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
  EraVMSHA3ConstFolding() : FunctionPass(ID) {}
};

/// This structure holds an information about a single memory clobber.
/// While walking upwards starting at a __sha3 call (which is a MemUse),
/// we create a MemClobber instance for each dominating memory clobber
/// (in current implementation - a store instruction) and put it in a
/// list which then gets sorted (by the 'Start' value). As result, we get
/// representation of a continuous memory region the __sha3 computes a hash for.
struct MemClobber {
  // Byte offset relatively to the beginning of the __sha3 memory location.
  uint64_t Start;

  // Size in bytes of the clobber.
  uint64_t Size;

  // Value to be stored in the memory.
  APInt Clobber;
};

/// This class holds an information required for folding __sha3 calls
/// in the function F.
///
/// The workflow with the class is as follows:
///   1. Create an FoldingState instance.
///      The constructor collects the MemorySSA uses corresponding to
///      __sha3 calls in the function F.
///   2. Iterate through the collected memory uses calling the runFolding(),
///      which, on success, returns a computed keccak256
///      hash value.
///      Replace __sha3 values with the calculated hash values,
///      removing the calls and invoking removeFromMSSA() to keep up to date
///      the MemorySSA representation.
///   3. Run simplifyInstructions() to simplify/clean up the F.
///
class FoldingState {
  /// Types of the relative placement of the two memory locations.
  enum class OverlapType {
    // Loc1 is completely within the Loc2
    //    |-Loc1-|
    // |-----Loc2----|
    OL_Complete,

    // Loc1 is partially with the Loc2
    // and they both refer to the same underlying object
    //           |-Loc1-|
    // |-----Loc2----|
    OL_MaybePartial,

    // Memory locations don't intersect
    // |--Loc1--|
    //            |---Loc2--|
    OL_None,

    // Nothing can be determined
    OL_Unknown
  };

  /// Describes how the memory clobber is placed with respect
  /// to the memory use (__sha3 call).
  struct OverlapResult {
    OverlapType Type;

    // Clobber offset relative to the beginning of the memory use.
    // Unless the type is 'OL_Complete' actual offset value doesn't matter.
    uint64_t ClobberOffset;
  };

  // Whether the function contains any irreducible control flow, useful for
  // being accurately able to detect loops.
  const bool ContainsIrreducibleLoops;

  // The __sha3 calls to be analyzed.
  SmallVector<MemoryUse *, 8> SHA3MemUses;

  Function &F;
  AliasAnalysis &AA;
  AssumptionCache &AC;
  MemorySSA &MSSA;
  std::unique_ptr<MemorySSAUpdater> MSSAUpdater;
  const TargetLibraryInfo &TLI;
  const DataLayout &DL;
  const SimplifyQuery SQ;
  const LoopInfo &LI;

public:
  FoldingState(const FoldingState &) = delete;

  FoldingState &operator=(const FoldingState &) = delete;

  FoldingState(Function &F, AliasAnalysis &AA, AssumptionCache &AC,
               MemorySSA &MSSA, DominatorTree &DT, const TargetLibraryInfo &TLI,
               const LoopInfo &LI);

  /// Collect all the potential clobbering memory accesses for the
  /// given __sha3 call (\p Call).
  SmallVector<MemoryDef *, 8> collectSHA3Clobbers(const CallInst *Call);

  /// For the given __sha3 call (\p Call), walk through the collected MemorySSA
  /// definitions (\p MemDefs) and try to build a continuous memory segment with
  /// the data for which we compute the keccak256 hash. On success return
  /// the computed hash value.
  Value *runFolding(const CallInst *Call,
                    SmallVectorImpl<MemoryDef *> &MemDefs) const;

  /// Try to simplify instructions in the function F. The candidates for
  /// simplification may appear after replacing __sha3 calls with the
  /// calculated hash values.
  bool simplifyInstructions();

  /// Exclude the instruction from MSSA if it's going to be removed from a BB.
  void removeFromMSSA(Instruction *Inst) {
    if (VerifyMemorySSA)
      MSSA.verifyMemorySSA();

    MSSAUpdater->removeMemoryAccess(Inst, true);
  }

  /// Return collected MemorySSa uses corresponding to __sha3 calls.
  const SmallVectorImpl<MemoryUse *> &getSHA3MemUses() const {
    return SHA3MemUses;
  }

private:
  /// Return true if the \p I is a call to the library function \p F.
  /// Consider moving it to a common code. TODO: CPR-1386.
  bool isLibFuncCall(const Instruction *I, LibFunc F) const;

  /// Return true if a dependency between \p Clobber and \p MemUse is
  /// guaranteed to be loop invariant for the loops that they are in.
  bool isGuaranteedLoopIndependent(const Instruction *Clobber,
                                   const Instruction *MemUse,
                                   const MemoryLocation &ClobberLoc) const;

  /// Return true if \p Ptr is guaranteed to be loop invariant for any possible
  /// loop. In particular, this guarantees that it only references a single
  /// MemoryLocation during execution of the containing function.
  bool isGuaranteedLoopInvariant(const Value *Ptr) const;

  /// Check how the two memory locations (\p MemUseLoc and \p ClobberLoc)
  /// are located with respect to each other.
  OverlapResult isOverlap(const Instruction *MemUse, const Instruction *Clobber,
                          const MemoryLocation &MemUseLoc,
                          const MemoryLocation &ClobberLoc) const;

  /// Try to cast the constant pointer value \p Ptr to the integer offset.
  /// Consider moving it to a common code. TODO: CPR-1380.
  std::optional<uint64_t> tryToCastPtrToInt(const Value *Ptr) const;

  /// Ensure the sorted array of clobbers forms a continuous memory region.
  /// On success return the size the memory region.
  std::optional<uint64_t>
  checkMemoryClobbers(const SmallVector<MemClobber> &MemClobbers) const;

  /// Compute keccak256 hash for the memory segment, formed by the sorted list
  /// of memory clobbers passed in \p MemClobbers.
  std::array<uint8_t, 32>
  computeKeccak256Hash(const SmallVectorImpl<MemClobber> &MemClobbers) const;

  /// Check if we can ignore non store clobber instruction that doesn't actually
  /// clobber heap memory. For example, a memcpy to AS other than heap.
  /// Probably we should check more cases here. TODO: CPR-1370.
  bool shouldSkipClobber(const Instruction *MemInst) const {
    if (!MemInst->mayWriteToMemory())
      return true;

    if (const auto *Intr = dyn_cast<AnyMemIntrinsic>(MemInst))
      return Intr->getDestAddressSpace() != EraVMAS::AS_HEAP;

    return false;
  }

  /// Return MemoryLocation corresponding to the pointer argument of
  /// __sha3 call.
  MemoryLocation getLocForSHA3Call(const CallInst *I) const {
    return MemoryLocation::getForArgument(I, 0, TLI);
  }
}; // end FoldingState struct

} // end anonymous namespace

static uint64_t getPointerSize(const Value *V, const DataLayout &DL,
                               const TargetLibraryInfo &TLI,
                               const Function *F) {
  uint64_t Size = 0;
  ObjectSizeOpts Opts;
  Opts.NullIsUnknownSize = NullPointerIsDefined(F);

  if (getObjectSize(V, Size, DL, &TLI, Opts))
    return Size;
  return MemoryLocation::UnknownSize;
}

bool FoldingState::isLibFuncCall(const Instruction *I, LibFunc F) const {
  const CallInst *Call = dyn_cast<CallInst>(I);
  if (!Call)
    return false;

  Function *Callee = Call->getCalledFunction();
  if (!Callee)
    return false;

  LibFunc Func = NotLibFunc;
  const StringRef Name = Callee->getName();
  if (!TLI.getLibFunc(Name, Func) || !TLI.has(Func) || Func != F)
    return false;

  return true;
}

FoldingState::FoldingState(Function &F, AliasAnalysis &AA, AssumptionCache &AC,
                           MemorySSA &MSSA, DominatorTree &DT,
                           const TargetLibraryInfo &TLI, const LoopInfo &LI)
    : ContainsIrreducibleLoops(mayContainIrreducibleControl(F, &LI)), F(F),
      AA(AA), AC(AC), MSSA(MSSA),
      MSSAUpdater(std::make_unique<MemorySSAUpdater>(&MSSA)), TLI(TLI),
      DL(F.getParent()->getDataLayout()), SQ(DL, &TLI, &DT, &AC), LI(LI) {
  MSSA.ensureOptimizedUses();

  const ReversePostOrderTraversal<BasicBlock *> RPOT(&*F.begin());
  for (BasicBlock *BB : RPOT) {
    for (Instruction &I : *BB) {
      if (!isLibFuncCall(&I, LibFunc_xvm_sha3))
        continue;

      const auto *Call = cast<CallInst>(&I);
      auto *MA = MSSA.getMemoryAccess(Call);
      auto *MU = dyn_cast_or_null<MemoryUse>(MA);
      // __sha3() is passed a pointer in the first argument
      // and the memory size in the second one. It's expected that the
      // memory size to be a constant expression. In this case
      // the memory location should be precise.
      if (MU && getLocForSHA3Call(Call).Size.isPrecise())
        SHA3MemUses.push_back(MU);
    }
  }
}

SmallVector<MemoryDef *, 8>
FoldingState::collectSHA3Clobbers(const CallInst *Call) {
  SmallVector<MemoryDef *, 8> Clobbers;
  const MemoryLocation Loc = getLocForSHA3Call(Call);
  MemorySSAWalker *Walker = MSSA.getWalker();

  // For the given __sha3 call (which is MemoryUse in terms of MemorySSA) we
  // need to collect all memory clobbering accesses. Usually these are just
  // 'store' instructions. Other cases are not handled by the current
  // implementation. For this we employ MemorySSA representation that maps
  // memory clobbering Instructions to three access types:
  //  - live on entry (nothing to do, __sha3 is not clobbered),
  //  - MemoryDef,
  //  - MemoryPhi
  //
  // We start with a nearest to the __sha3 call dominating clobbering access.
  // Then we do the same for the just found clobber and so on until we find the
  // 'live on entry'.
  // For simplicity, in case of a MemoryPhi we also stop the search. This
  // constraint can be relaxed. TODO: CPR-1370.

  MemoryAccess *MA =
      Walker->getClobberingMemoryAccess(MSSA.getMemoryAccess(Call));
  for (; !MSSA.isLiveOnEntryDef(MA) && !isa<MemoryPhi>(MA);) {
    if (auto *Def = dyn_cast<MemoryDef>(MA)) {
      Clobbers.push_back(Def);
      MA = Walker->getClobberingMemoryAccess(Def->getDefiningAccess(), Loc);
    }
  }

  return Clobbers;
}

bool FoldingState::simplifyInstructions() {
  bool Changed = false;
  for (auto &Inst : make_early_inc_range(instructions(F))) {
    if (Value *V = simplifyInstruction(&Inst, SQ)) {
      LLVM_DEBUG(dbgs() << " EraVMSHA3ConstFolding simplify: " << Inst
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

bool FoldingState::isGuaranteedLoopIndependent(
    const Instruction *Clobber, const Instruction *MemUse,
    const MemoryLocation &ClobberLoc) const {
  // If the dependency is within the same block or loop level (being careful
  // of irreducible loops), we know that AA will return a valid result for the
  // memory dependency.
  if (MemUse->getParent() == Clobber->getParent())
    return true;

  // Check if both Clobber and MemUse are known to be in the same loop level.
  const Loop *CurrentLI = LI.getLoopFor(MemUse->getParent());
  if (!ContainsIrreducibleLoops && CurrentLI &&
      CurrentLI == LI.getLoopFor(Clobber->getParent()))
    return true;

  // Otherwise check the memory location is invariant to any loops.
  return isGuaranteedLoopInvariant(ClobberLoc.Ptr);
}

bool FoldingState::isGuaranteedLoopInvariant(const Value *Ptr) const {
  Ptr = Ptr->stripPointerCasts();
  if (const auto *GEP = dyn_cast<GEPOperator>(Ptr))
    if (GEP->hasAllConstantIndices())
      Ptr = GEP->getPointerOperand()->stripPointerCasts();

  if (const auto *I = dyn_cast<Instruction>(Ptr))
    return I->getParent()->isEntryBlock();

  return true;
}

static std::optional<uint64_t> getNullOrInt(const APInt &APVal) {
  if (APVal.getActiveBits() <= 64)
    return APVal.getZExtValue();

  return std::nullopt;
}

std::optional<uint64_t>
FoldingState::tryToCastPtrToInt(const Value *Ptr) const {
  if (isa<ConstantPointerNull>(Ptr))
    return UINT64_C(0);

  if (const auto *CE = dyn_cast<ConstantExpr>(Ptr)) {
    if (CE->getOpcode() == Instruction::IntToPtr) {
      if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(0))) {
        // Give up in case of a huge offsset, as this shouldn't happen
        // in a real life.
        return getNullOrInt(CI->getValue());
      }
    }
  }

  if (const auto *IntToPtr = dyn_cast<IntToPtrInst>(Ptr)) {
    if (auto *CI = dyn_cast<ConstantInt>(IntToPtr->getOperand(0))) {
      return getNullOrInt(CI->getValue());
    }
  }

  return std::nullopt;
}

FoldingState::OverlapResult
FoldingState::isOverlap(const Instruction *MemUse, const Instruction *Clobber,
                        const MemoryLocation &MemUseLoc,
                        const MemoryLocation &ClobberLoc) const {
  // AliasAnalysis does not always account for loops. Limit overlap checks
  // to dependencies for which we can guarantee they are independent of any
  // loops they are in.
  if (!isGuaranteedLoopIndependent(Clobber, MemUse, ClobberLoc))
    return {OverlapType::OL_Unknown, 0};

  const Value *ClobberPtr = ClobberLoc.Ptr->stripPointerCasts();
  const Value *MemUsePtr = MemUseLoc.Ptr->stripPointerCasts();
  const Value *ClobberUndObj = getUnderlyingObject(ClobberPtr);
  const Value *MemUseUndObj = getUnderlyingObject(MemUsePtr);
  const uint64_t MemUseSize = MemUseLoc.Size.getValue();
  const uint64_t ClobberSize = ClobberLoc.Size.getValue();

  // If both the Clobber and MemUse pointers are constant we expect two cases:
  //  - pointer is just a nullptr
  //  - pointer is a cast of an integer constant

  if (isa<Constant>(ClobberPtr) && isa<Constant>(MemUsePtr)) {
    if (!tryToCastPtrToInt(ClobberPtr) || !tryToCastPtrToInt(MemUsePtr))
      return {OverlapType::OL_Unknown, 0};

    const uint64_t ClobberPtrInt = *tryToCastPtrToInt(ClobberPtr);
    const uint64_t MemUsePtrInt = *tryToCastPtrToInt(MemUsePtr);
    if (MemUsePtrInt <= ClobberPtrInt &&
        (ClobberPtrInt + ClobberSize) <= (MemUsePtrInt + MemUseSize)) {
      return {OverlapType::OL_Complete, ClobberPtrInt - MemUsePtrInt};
    }
  }

  // Check whether the Clobber overwrites the MemUse object.
  if (ClobberUndObj == MemUseUndObj) {
    const uint64_t MemUseUndObjSize = getPointerSize(MemUseUndObj, DL, TLI, &F);
    if (MemUseUndObjSize != MemoryLocation::UnknownSize &&
        MemUseUndObjSize == MemUseSize && MemUseSize == ClobberSize)
      return {OverlapType::OL_Complete, 0};
  }

  // Query the alias information
  const AliasResult AAR = AA.alias(MemUseLoc, ClobberLoc);

  // If the start pointers are the same, we just have to compare sizes to see
  // if the MemUse was larger than the Clobber.
  if (AAR == AliasResult::MustAlias) {
    // Make sure that the MemUseSize size is >= the ClobberSize size.
    if (MemUseSize >= ClobberSize)
      return {OverlapType::OL_Complete, 0};
  }

  // If we hit a partial alias we may have a full overwrite
  if (AAR == AliasResult::PartialAlias && AAR.hasOffset()) {
    const int32_t Offset = AAR.getOffset();
    if (Offset >= 0 &&
        static_cast<uint64_t>(Offset) + ClobberSize <= MemUseSize) {
      return {OverlapType::OL_Complete, static_cast<uint64_t>(Offset)};
    }
  }

  // If we can't resolve the same pointers to the same object, then we can't
  // analyze them at all.
  if (MemUseUndObj != ClobberUndObj) {
    if (AAR == AliasResult::NoAlias)
      return {OverlapType::OL_None, 0};
    return {OverlapType::OL_Unknown, 0};
  }

  // Okay, we have stores to two completely different pointers. Try to
  // decompose the pointer into a "base + constant_offset" form. If the base
  // pointers are equal, then we can reason about the MemUse and the Clobber.
  int64_t ClobberOffset = 0;
  int64_t MemUseOffset = 0;
  const Value *ClobberBasePtr =
      GetPointerBaseWithConstantOffset(ClobberPtr, ClobberOffset, DL);
  const Value *MemUseBasePtr =
      GetPointerBaseWithConstantOffset(MemUsePtr, MemUseOffset, DL);

  // If the base pointers still differ, we have two completely different
  // stores.
  if (ClobberBasePtr != MemUseBasePtr)
    return {OverlapType::OL_Unknown, 0};

  // The MemUse completely overlaps the clobber if both the start and the end
  // of the Clobber are inside the MemUse:
  //    |   |--Clobber-|   |
  //    |------MemUse------|
  // We have to be careful here as *Off is signed while *.Size is unsigned.

  // Check if the Clobber access starts not before the MemUse one.
  if (ClobberOffset >= MemUseOffset) {
    // If the Clobber access ends not after the MemUse access then the
    // clobber one is completely overlapped by the MemUse one.
    if (static_cast<uint64_t>(ClobberOffset - MemUseOffset) + ClobberSize <=
        MemUseSize)
      return {OverlapType::OL_Complete,
              static_cast<uint64_t>(ClobberOffset - MemUseOffset)};

    // If start of the Clobber access is before end of the MemUse access
    // then accesses overlap.
    if (static_cast<uint64_t>(ClobberOffset - MemUseOffset) < MemUseSize)
      return {OverlapType::OL_MaybePartial, 0};
  }
  // If start of the MemUse access is before end of the Clobber access then
  // accesses overlap.
  else if (static_cast<uint64_t>(MemUseOffset - ClobberOffset) < ClobberSize) {
    return {OverlapType::OL_MaybePartial, 0};
  }

  // Can reach here only if accesses are known not to overlap.
  return {OverlapType::OL_None, 0};
}

std::optional<uint64_t> FoldingState::checkMemoryClobbers(
    const SmallVector<MemClobber> &MemClobbers) const {
  const auto *Begin = MemClobbers.begin();
  const auto *End = MemClobbers.end();
  assert(Begin != End);

  uint64_t TotalSize = Begin->Size;
  for (const auto *It = std::next(Begin); It != End; ++It) {
    TotalSize += It->Size;
    const auto *ItPrev = std::prev(It);
    if ((ItPrev->Start + ItPrev->Size) != It->Start) {
      LLVM_DEBUG(dbgs() << "\tclobbers do alias, or there is a gap: ["
                        << ItPrev->Start << ", " << ItPrev->Start + ItPrev->Size
                        << "] -> [" << It->Start << ", " << It->Start + It->Size
                        << "]" << '\n');
      return std::nullopt;
    }
  }

  return TotalSize;
}

std::array<uint8_t, 32> FoldingState::computeKeccak256Hash(
    const SmallVectorImpl<MemClobber> &MemClobbers) const {
  SmallVector<char, 512> MemBuf;
  raw_svector_ostream OS(MemBuf);
  // Put all the clobber values to the buffer in the BE order.
  for (const auto &MemClobber : MemClobbers) {
    // Words of the APInt are in the LE order, so we need to
    // iterate them starting from the end.
    const auto *ValRawData = MemClobber.Clobber.getRawData();
    for (unsigned I = 0, E = MemClobber.Clobber.getNumWords(); I != E; ++I)
      support::endian::write<APInt::WordType>(OS, ValRawData[E - I - 1],
                                              support::endianness::big);
  }
  LLVM_DEBUG(dbgs() << "\tinput sha3 data: " << toHex(OS.str()) << '\n');

  return KECCAK::KECCAK_256(OS.str());
}

Value *FoldingState::runFolding(const CallInst *Call,
                                SmallVectorImpl<MemoryDef *> &MemDefs) const {
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

    auto OverlapRes = isOverlap(Call, SI, UseMemLoc, ClobberLoc);

    // This clobber doesn't write to the memory __sha3 is reading from.
    // Just skip it and come to the next clobber.
    if (OverlapRes.Type == OverlapType::OL_None) {
      LLVM_DEBUG(dbgs() << "\t\twrites out of sha3 memory, offset: "
                        << OverlapRes.ClobberOffset << '\n');
      continue;
    }

    // We give up in these cases, as it's  difficult or impossible
    // to determine the full memory data for the __sha3.
    // If required, we could try to support some cases. TODO: CPR-1370.
    if (OverlapRes.Type == OverlapType::OL_MaybePartial ||
        OverlapRes.Type == OverlapType::OL_Unknown) {
      LLVM_DEBUG(dbgs() << "\t\tpartially or unknow overlap" << '\n');
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
    // by existing one. In fact this is similar to what DSE pass does.
    if (std::any_of(MemClobbers.begin(), MemClobbers.end(),
                    [OverlapRes, ClobberSize](const auto &C) {
                      return C.Start == OverlapRes.ClobberOffset &&
                             C.Size == ClobberSize;
                    })) {
      LLVM_DEBUG(dbgs() << "\t\tstored value is killed by the"
                           "consequent clobber"
                        << '\n');
      continue;
    }

    TotalSizeOfClobbers += ClobberSize;
    assert(ClobberSize * 8 == ClobberVal->getBitWidth());

    MemClobbers.push_back(
        {OverlapRes.ClobberOffset, ClobberSize, ClobberVal->getValue()});

    // If we have collected clobbers that fully cover the __sha3 memory
    // location, sort them in the ascending order of their starting addresses
    // and perform some checks.
    if (TotalSizeOfClobbers < UseMemSize)
      continue;

    // Sort memory clobbers in the ascending order of their starting
    // positions.
    std::sort(
        MemClobbers.begin(), MemClobbers.end(),
        [](const auto &Lhs, const auto &Rhs) { return Lhs.Start < Rhs.Start; });

    // Ensure the sorted array of clobbers forms a continuous memory region.
    auto TotalSize = checkMemoryClobbers(MemClobbers);
    if (!TotalSize)
      return nullptr;

    assert(TotalSize == UseMemSize);
    auto Hash = computeKeccak256Hash(MemClobbers);

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

bool runEraVMSHA3ConstFolding(Function &F, AliasAnalysis &AA,
                              AssumptionCache &AC, MemorySSA &MSSA,
                              DominatorTree &DT, const TargetLibraryInfo &TLI,
                              const LoopInfo &LI) {
  LLVM_DEBUG(dbgs() << "********** EraVM sha3 constant folding **********\n"
                    << "********** Function: " << F.getName() << '\n');

  FoldingState State(F, AA, AC, MSSA, DT, TLI, LI);
  SmallSet<MemoryUse *, 16> RemovedSHA3;

  bool Changed = false;
  bool ChangedOnIter = false;
  do {
    LLVM_DEBUG(dbgs() << "Running new iteration of sha3 constant folding...\n");

    for (MemoryUse *SHA3MemUse : State.getSHA3MemUses()) {
      if (RemovedSHA3.count(SHA3MemUse))
        continue;

      auto *SHA3Call = dyn_cast<CallInst>(SHA3MemUse->getMemoryInst());
      assert(SHA3Call != nullptr);

      LLVM_DEBUG(dbgs() << "Analyzing: " << *SHA3Call << '\n');
      SmallVector<MemoryDef *, 8> Clobbers =
          State.collectSHA3Clobbers(SHA3Call);

      if (Value *HashVal = State.runFolding(SHA3Call, Clobbers)) {
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
  } while (ChangedOnIter && State.simplifyInstructions());

  return Changed;
}

bool EraVMSHA3ConstFolding::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
  auto &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  const auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  auto &MSSA = getAnalysis<MemorySSAWrapperPass>().getMSSA();
  auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  return runEraVMSHA3ConstFolding(F, AA, AC, MSSA, DT, TLI, LI);
}

char EraVMSHA3ConstFolding::ID = 0;

INITIALIZE_PASS(
    EraVMSHA3ConstFolding, "eravm-sha3-constant-folding",
    "Replace '__sha3' calls with calculated hash values if arguments are known",
    false, false)

FunctionPass *llvm::createEraVMSHA3ConstFoldingPass() {
  return new EraVMSHA3ConstFolding;
}

PreservedAnalyses EraVMSHA3ConstFoldingPass::run(Function &F,
                                                 FunctionAnalysisManager &AM) {
  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  auto &AA = AM.getResult<AAManager>(F);
  const auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();
  auto &LI = AM.getResult<LoopAnalysis>(F);

  return runEraVMSHA3ConstFolding(F, AA, AC, MSSA, DT, TLI, LI)
             ? PreservedAnalyses::none()
             : PreservedAnalyses::all();
}
