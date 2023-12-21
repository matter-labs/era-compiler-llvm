//===-- EraVMCSE.cpp - EraVM CSE specific pass ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass performs a simple dominator tree walk that eliminates specific
// EraVM instructions.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicsEraVM.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/RecyclingAllocator.h"
#include "llvm/Transforms/Utils/AssumeBundleBuilder.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-cse"

static cl::opt<bool> DisableSha3SReqCSE(
    "eravm-disable-sha3-sreq-cse",
    cl::desc("Disable CSE for __system_request and __sha3 calls"),
    cl::init(false));

namespace {

class EraVMCSELegacyPass : public FunctionPass {
public:
  static char ID; // Pass ID
  EraVMCSELegacyPass() : FunctionPass(ID) {}

  StringRef getPassName() const override { return "EraVM CSE"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<MemorySSAWrapperPass>();
    AU.addPreserved<MemorySSAWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
    AU.addPreserved<AAResultsWrapperPass>();
    AU.setPreservesCFG();
  }

  bool runOnFunction(Function &F) override;
};

} // end anonymous namespace

namespace llvm {

/// Special DenseMapInfo traits to compare Instruction* by *value* of the
/// instruction rather than by pointer value. This is similar to what is
/// implemented for MachineInstr*.
template <> struct DenseMapInfo<Instruction> {
  static inline Instruction *getEmptyKey() {
    return DenseMapInfo<Instruction *>::getEmptyKey();
  }

  static inline Instruction *getTombstoneKey() {
    return DenseMapInfo<Instruction *>::getTombstoneKey();
  }

  static unsigned getHashValue(const Instruction *Inst);
  static bool isEqual(const Instruction *LHS, const Instruction *RHS);
};

} // end namespace llvm

unsigned DenseMapInfo<Instruction>::getHashValue(const Instruction *Inst) {
  // Hash all of the operands as pointers and mix in the opcode.
  return hash_combine(
      Inst->getOpcode(),
      hash_combine_range(Inst->value_op_begin(), Inst->value_op_end()));
}

bool DenseMapInfo<Instruction>::isEqual(const Instruction *LHS,
                                        const Instruction *RHS) {
  if (RHS == getEmptyKey() || RHS == getTombstoneKey() ||
      LHS == getEmptyKey() || LHS == getTombstoneKey())
    return LHS == RHS;
  return LHS->isIdenticalTo(RHS);
}

//===----------------------------------------------------------------------===//
// EraVMCSE implementation
//===----------------------------------------------------------------------===//

namespace {

/// A simple and fast domtree-based CSE pass.
///
/// This pass does a simple depth-first walk over the dominator tree,
/// eliminating redundant EraVM specific instructions and using instsimplify to
/// canonicalize things as it goes.
class EraVMCSE {
public:
  /// Set up the EraVMCSE runner for a particular function.
  EraVMCSE(Function &F, const TargetLibraryInfo &TLI, DominatorTree &DT,
           AssumptionCache &AC, MemorySSA &MSSA)
      : F(F), TLI(TLI), DT(DT), AC(AC),
        SQ(F.getParent()->getDataLayout(), &TLI, &DT, &AC), MSSA(MSSA),
        MSSAUpdater(std::make_unique<MemorySSAUpdater>(&MSSA)) {
    MSSA.ensureOptimizedUses();
  }

  bool run();

private:
  Function &F;
  const TargetLibraryInfo &TLI;
  DominatorTree &DT;
  AssumptionCache &AC;
  const SimplifyQuery SQ;
  MemorySSA &MSSA;
  std::unique_ptr<MemorySSAUpdater> MSSAUpdater;

  /// A scoped hash table of the current values of EraVM call
  /// values.
  using InstAType = RecyclingAllocator<
      BumpPtrAllocator,
      ScopedHashTableVal<Instruction *, std::pair<Instruction *, unsigned>>>;
  using InstHTType =
      ScopedHashTable<Instruction *, std::pair<Instruction *, unsigned>,
                      DenseMapInfo<Instruction>, InstAType>;
  InstHTType AvailableCalls;

  /// This is the current generation of the llvm.eravm.meta calls.
  /// In order to safely do CSE, two identical calls have to be in the same
  /// generation, i.e. there are no preventers between them.
  unsigned CurrentMetaGen = 0;

  /// This is the current generation of the __system_request calls.
  /// In order to safely do CSE, two identical calls have to be in the same
  /// generation, i.e. there are no preventers between them.
  unsigned CurrentSReqGen = 0;

  /// Cache whether we found CSE preventers for __system_request and
  /// llvm.eravm.meta in a particular BB.
  DenseMap<BasicBlock *, std::pair<bool, bool>> CSEPreventers;

  // This is RAII to track scope of a single value, so it can
  // restore it when scope gets popped.
  class ValueScope {
  public:
    explicit ValueScope(unsigned *Val) : Ptr(Val), OldValue(*Val) {
      assert(Val && "Value can't be nullptr!");
    }
    ValueScope(const ValueScope &) = delete;
    ValueScope &operator=(const ValueScope &) = delete;
    ValueScope(ValueScope &&) = delete;
    ValueScope &&operator=(ValueScope &&) = delete;
    ~ValueScope() { *Ptr = OldValue; }

  private:
    unsigned *Ptr;
    unsigned OldValue;
  };

  // Almost a POD, but needs to call the constructors for the scoped hash
  // tables so that a new scope gets pushed on. These are RAII so that the
  // scope gets popped when the NodeScope is destroyed.
  class NodeScope {
  public:
    NodeScope(InstHTType &AvailableCalls, unsigned *CurrentMetaGen,
              unsigned *CurrentSReqGen)
        : StdlibCallScope(AvailableCalls), CurrentMetaGenScope(CurrentMetaGen),
          CurrentSReqGenScope(CurrentSReqGen) {}
    NodeScope(const NodeScope &) = delete;
    NodeScope &operator=(const NodeScope &) = delete;
    NodeScope(NodeScope &&) = delete;
    NodeScope &&operator=(NodeScope &&) = delete;
    ~NodeScope() = default;

  private:
    InstHTType::ScopeTy StdlibCallScope;
    ValueScope CurrentMetaGenScope;
    ValueScope CurrentSReqGenScope;
  };

  // Contains all the needed information to create a stack for doing a depth
  // first traversal of the tree. This includes scopes and there is a child
  // iterator so that the children do not need to be store separately.
  class StackNode {
  public:
    StackNode(InstHTType &AvailableCalls, unsigned *CurrentMetaGen,
              unsigned *CurrentSReqGen, DomTreeNode *N,
              DomTreeNode::const_iterator Child,
              DomTreeNode::const_iterator End)
        : Node(N), ChildIter(Child), EndIter(End),
          Scopes(AvailableCalls, CurrentMetaGen, CurrentSReqGen) {}
    StackNode(const StackNode &) = delete;
    StackNode &operator=(const StackNode &) = delete;
    StackNode(StackNode &&) = delete;
    StackNode &&operator=(StackNode &&) = delete;
    ~StackNode() = default;

    // Accessors.
    DomTreeNode *node() { return Node; }
    DomTreeNode::const_iterator childIter() const { return ChildIter; }

    DomTreeNode *nextChild() {
      DomTreeNode *Child = *ChildIter;
      ++ChildIter;
      return Child;
    }

    DomTreeNode::const_iterator end() const { return EndIter; }
    bool isProcessed() const { return Processed; }
    void process() { Processed = true; }

  private:
    DomTreeNode *Node;
    DomTreeNode::const_iterator ChildIter;
    DomTreeNode::const_iterator EndIter;
    NodeScope Scopes;
    bool Processed = false;
  };

  bool processNode(DomTreeNode *Node);

  /// Determine if the memory referenced by LaterInst is from the same heap
  /// version as EarlierInst.
  /// This is similar to what is implemented in the EarlyCSE pass.
  bool isSameMemGeneration(Instruction *EarlierInst,
                           Instruction *LaterInst) const;

  /// Exclude the instruction from MSSA if it's going to be removed from a BB.
  void removeMSSA(Instruction &Inst) const;

  /// Remove instruction only if it's dead. Return true if successful.
  bool removeInstructionOnlyIfDead(Instruction &Inst, bool SalvageDbgInfo,
                                   bool PrintDCE) const;

  /// Remove an Old instruction, and replace it with a New one. In case New
  /// is nullptr, just do the removal.
  void removeAndReplaceInstruction(Instruction &Old, Instruction *New,
                                   bool SalvageDbgInfo = false) const;

  /// Traverse CFG and try to find preventers. Since we know that Begin
  /// dominates End, we are traversing over predecessors starting from End and
  /// stopping when we reach to Begin. Return whether we found CSE preventers
  /// for __system_request and llvm.eravm.meta.
  std::pair<bool, bool> hasPreventersInCFG(BasicBlock *Begin, BasicBlock *End);

  /// Inspect instructions and return whether we found CSE preventers for
  /// __system_request and llvm.eravm.meta in a particular BB.
  /// Cache that result, so we can use it to speed up later inspections.
  std::pair<bool, bool> hasPreventersInBB(BasicBlock *BB);
};

} // end anonymous namespace

/// Return whether is safe to eliminate duplicated __system_request calls. In
/// case they are not in the same generation (there is a preventer between two
/// calls), return whether is safe to do elimination based on the signature.
static bool isSafeToEliminateSReq(unsigned EarlierGeneration,
                                  unsigned LaterGeneration,
                                  const Instruction *Inst) {
  // Check the simple generation tracking first.
  if (EarlierGeneration == LaterGeneration)
    return true;

  const auto *Call = dyn_cast<CallInst>(Inst);
  assert(Call && "Expected call instruction.");
  auto *Signature = dyn_cast<ConstantInt>(Call->getArgOperand(1));

  // If this isn't valid constant signature, assume we can't eliminate it.
  if (!Signature || Signature->getBitWidth() != 256)
    return false;

  // TODO: CPR-1386 Move __system_request signature detection to a common code.
  const DenseSet<APInt> PreventingSignatures = {
      // SELFBALANCE, BALANCE
      {256,
       {0x125fc972a6507f39, 0x6b1951864fbfc14e, 0x829bd487b90b7253,
        0x9cc7f708afc65944}},
      // EXTCODESIZE, CODESIZE
      {256,
       {0x023a8d90e8508b83, 0x02500962caba6a15, 0x68e884a7374b41e0,
        0x1806aa1896bbf265}},
      // EXTCODEHASH
      {256,
       {0x45c64697555efe0e, 0xa063a94b6d404b2f, 0xea1b3ecd64121a3f,
        0xe03fe177bb050a40}}};
  return !PreventingSignatures.count(Signature->getValue());
}

/// Return true if an instruction is a call to a library function.
/// TODO: CPR-1386 Move this to a common code.
static bool isLibFuncCall(const Instruction *Inst,
                          const StringSet<> &LibFuncNames) {
  const auto *Call = dyn_cast<CallInst>(Inst);
  if (!Call)
    return false;

  const auto *Callee = Call->getCalledFunction();
  if (!Callee)
    return false;
  return LibFuncNames.count(Callee->getName());
}

/// Return true if a result of an instruction is not used.
static bool isInstructionDead(Instruction *Inst, const TargetLibraryInfo *TLI) {
  if (isInstructionTriviallyDead(Inst, TLI))
    return true;

  if (!Inst->use_empty())
    return false;

  if (DisableSha3SReqCSE)
    return false;
  return isLibFuncCall(Inst, {"__sha3", "__system_request"});
}

/// Check if instruction prevents elimination of some __system_request calls.
static bool preventsSReqElimination(const Instruction &Inst) {
  if (const auto *II = dyn_cast<IntrinsicInst>(&Inst))
    return II->getIntrinsicID() == Intrinsic::eravm_nearcall;
  // TODO: CPR-1511 Check for preventer custom function attribute.
  return isa<CallBase>(&Inst) &&
         !isLibFuncCall(&Inst, {"__staticcall", "__staticcall_byref"});
}

/// Check if instruction prevents elimination of llvm.eravm.meta.
static bool preventsMetaElimination(Instruction &Inst) {
  auto IsHeapAS = [](unsigned AS) {
    return AS == EraVMAS::AS_HEAP || AS == EraVMAS::AS_HEAP_AUX;
  };

  if (const auto *Load = dyn_cast<LoadInst>(&Inst))
    return IsHeapAS(Load->getPointerAddressSpace());

  if (const auto *Store = dyn_cast<StoreInst>(&Inst))
    return IsHeapAS(Store->getPointerAddressSpace());

  if (const auto *AMTI = dyn_cast<AnyMemTransferInst>(&Inst))
    return IsHeapAS(AMTI->getDestAddressSpace()) ||
           IsHeapAS(AMTI->getSourceAddressSpace());

  if (const auto *AMSI = dyn_cast<AnyMemSetInst>(&Inst))
    return IsHeapAS(AMSI->getDestAddressSpace());

  if (const auto *II = dyn_cast<IntrinsicInst>(&Inst))
    return II->getIntrinsicID() == Intrinsic::eravm_precompile ||
           II->getIntrinsicID() == Intrinsic::eravm_decommit;
  // TODO: CPR-1511 Check for preventer custom function attribute.
  return isa<CallBase>(&Inst);
}

/// Return whether we can handle EraVM specific libcall.
static bool canHandleStdlibCall(const Instruction *Inst) {
  const StringSet<> LibFuncNames = {
      "__addmod", "__clz",      "__ulongrem", "__mulmod", "__signextend",
      "__exp",    "__exp_pow2", "__div",      "__sdiv",   "__mod",
      "__smod",   "__byte",     "__shl",      "__shr",    "__sar"};
  // Stdlib functions return value, so filter out instructions that
  // don't return anything.
  return !Inst->getType()->isVoidTy() && isLibFuncCall(Inst, LibFuncNames);
}

/// Return whether we can handle __system_request call.
static bool canHandleSystemRequestCall(Instruction *Inst) {
  // __system_request returns value, so filter out instructions that
  // don't return anything.
  return !Inst->getType()->isVoidTy() &&
         isLibFuncCall(Inst, {"__system_request"});
}

/// Return whether we can handle __sha3 call.
static bool canHandleSha3Call(Instruction *Inst) {
  // __sha3 returns value, so filter out instructions that
  // don't return anything.
  return !Inst->getType()->isVoidTy() && isLibFuncCall(Inst, {"__sha3"});
}

void EraVMCSE::removeMSSA(Instruction &Inst) const {
  if (VerifyMemorySSA)
    MSSA.verifyMemorySSA();

  MSSAUpdater->removeMemoryAccess(&Inst, true);
}

void EraVMCSE::removeAndReplaceInstruction(Instruction &Old, Instruction *New,
                                           bool SalvageDbgInfo) const {
  if (New)
    Old.replaceAllUsesWith(New);

  salvageKnowledge(&Old, &AC);
  if (SalvageDbgInfo)
    salvageDebugInfo(Old);
  removeMSSA(Old);
  Old.eraseFromParent();
}

bool EraVMCSE::removeInstructionOnlyIfDead(Instruction &Inst,
                                           bool SalvageDbgInfo,
                                           bool PrintDCE) const {
  if (!isInstructionDead(&Inst, &TLI))
    return false;

  if (PrintDCE)
    LLVM_DEBUG(dbgs() << "EraVMCSE DCE: " << Inst << '\n');

  removeAndReplaceInstruction(Inst, nullptr, SalvageDbgInfo);
  return true;
}

bool EraVMCSE::isSameMemGeneration(Instruction *EarlierInst,
                                   Instruction *LaterInst) const {
  // If MemorySSA has determined that one of EarlierInst or LaterInst does not
  // read/write memory, then we can safely return true here.
  auto *EarlierMA = MSSA.getMemoryAccess(EarlierInst);
  if (!EarlierMA)
    return true;
  auto *LaterMA = MSSA.getMemoryAccess(LaterInst);
  if (!LaterMA)
    return true;

  // Since we know LaterDef dominates LaterInst and EarlierInst dominates
  // LaterInst, if LaterDef dominates EarlierInst, then LaterDef can't occur
  // between EarlierInst and LaterInst and neither can any other write that
  // potentially clobbers LaterInst.
  // TODO: CPR-1511 If function is a clobber, skip it if it has no preventer
  // custom attribute.
  MemoryAccess *LaterDef =
      MSSA.getWalker()->getClobberingMemoryAccess(LaterInst);
  return MSSA.dominates(LaterDef, EarlierMA);
}

std::pair<bool, bool> EraVMCSE::hasPreventersInBB(BasicBlock *BB) {
  // Return if we already have the result.
  auto PreventersIt = CSEPreventers.find(BB);
  if (PreventersIt != CSEPreventers.end())
    return PreventersIt->second;

  bool HasSReqPreventer = false;
  bool HasMetaPreventer = false;
  for (auto &Inst : *BB) {
    // Don't take into account instructions that we are handling during CSE.
    if (isInstructionDead(&Inst, &TLI) || canHandleStdlibCall(&Inst) ||
        canHandleSha3Call(&Inst) || canHandleSystemRequestCall(&Inst))
      continue;

    // We are also handling llvm.eravm.meta during CSE.
    if (auto *II = dyn_cast<IntrinsicInst>(&Inst))
      if (II->getIntrinsicID() == Intrinsic::eravm_meta)
        continue;

    if (preventsSReqElimination(Inst))
      HasSReqPreventer = true;

    if (preventsMetaElimination(Inst))
      HasMetaPreventer = true;

    // Stop the search if we found both preventers.
    if (HasSReqPreventer && HasMetaPreventer)
      break;
  }

  // Cache the result, so we can use it later.
  CSEPreventers[BB] = {HasSReqPreventer, HasMetaPreventer};
  return {HasSReqPreventer, HasMetaPreventer};
}

std::pair<bool, bool> EraVMCSE::hasPreventersInCFG(BasicBlock *Begin,
                                                   BasicBlock *End) {
  assert(DT.dominates(Begin, End) && "Begin doesn't dominate End.");

  bool HasSReqPreventer = false;
  bool HasMetaPreventer = false;
  SmallPtrSet<BasicBlock *, 32> Visited;
  SmallVector<BasicBlock *, 32> WorkList{predecessors(End)};
  while (!WorkList.empty()) {
    BasicBlock *BB = WorkList.pop_back_val();
    if (BB == Begin || !Visited.insert(BB).second)
      continue;

    auto Preventers = hasPreventersInBB(BB);
    HasSReqPreventer |= Preventers.first;
    HasMetaPreventer |= Preventers.second;

    // Stop the search, if we found both preventers.
    if (HasSReqPreventer && HasMetaPreventer)
      return {true, true};

    append_range(WorkList, predecessors(BB));
  }
  return {HasSReqPreventer, HasMetaPreventer};
}

bool EraVMCSE::processNode(DomTreeNode *Node) {
  bool Changed = false;
  BasicBlock *BB = Node->getBlock();

  // If this block has multiple predecessors, then they could have instructions
  // that can prevent elimination of some instructions.
  if (BB->hasNPredecessorsOrMore(2)) {
    assert(Node->getIDom() && "Expected parent dominator node.");

    auto Preventers = hasPreventersInCFG(Node->getIDom()->getBlock(), BB);
    CurrentSReqGen += Preventers.first;
    CurrentMetaGen += Preventers.second;
  }

  // TODO: CPR-1512 Simplify handling in this loop.
  for (auto &Inst : make_early_inc_range(*BB)) {
    // Dead instructions should just be removed.
    if (removeInstructionOnlyIfDead(Inst, true, true)) {
      Changed = true;
      continue;
    }

    // If the instruction can be simplified (e.g. X+0 = X) then replace it with
    // its simpler value.
    if (Value *V = simplifyInstruction(&Inst, SQ)) {
      LLVM_DEBUG(dbgs() << "EraVMCSE Simplify: " << Inst << "  to: " << *V
                        << '\n');
      Changed = true;
      if (!Inst.use_empty())
        Inst.replaceAllUsesWith(V);
      if (removeInstructionOnlyIfDead(Inst, false, false))
        continue;
    }

    // If this is an EraVM stdlib call, process it.
    if (canHandleStdlibCall(&Inst)) {
      // If we have an available version of this call, try to replace it.
      auto InVal = AvailableCalls.lookup(&Inst);
      if (InVal.first) {
        LLVM_DEBUG(dbgs() << "EraVMCSE STDLIB CALL: " << Inst
                          << "  to: " << *InVal.first << '\n');
        removeAndReplaceInstruction(Inst, InVal.first);
        Changed = true;
        continue;
      }

      // Otherwise, remember that we have this call.
      AvailableCalls.insert(&Inst, std::make_pair(&Inst, 0));
      continue;
    }

    // If this is a call to __sha3, process it.
    if (canHandleSha3Call(&Inst)) {
      if (DisableSha3SReqCSE)
        continue;

      // If we have an available version of this call, try to replace it.
      auto InVal = AvailableCalls.lookup(&Inst);
      if (InVal.first && isSameMemGeneration(InVal.first, &Inst)) {
        LLVM_DEBUG(dbgs() << "EraVMCSE SHA3 CALL: " << Inst
                          << "  to: " << *InVal.first << '\n');
        removeAndReplaceInstruction(Inst, InVal.first);
        Changed = true;
        continue;
      }

      // TODO: CPR-1510 Eliminate calls based on the input data.

      // Otherwise, remember that we have this call.
      AvailableCalls.insert(&Inst, std::make_pair(&Inst, 0));
      continue;
    }

    // If this is a call to __system_request, process it.
    if (canHandleSystemRequestCall(&Inst)) {
      if (DisableSha3SReqCSE)
        continue;

      // If we have an available version of this call, try to replace it.
      auto InVal = AvailableCalls.lookup(&Inst);
      if (InVal.first &&
          isSafeToEliminateSReq(InVal.second, CurrentSReqGen, &Inst) &&
          isSameMemGeneration(InVal.first, &Inst)) {
        LLVM_DEBUG(dbgs() << "EraVMCSE SYSTEM REQUEST CALL: " << Inst
                          << "  to: " << *InVal.first << '\n');
        removeAndReplaceInstruction(Inst, InVal.first);
        Changed = true;
        continue;
      }

      // TODO: CPR-1509 Eliminate calls based on the input data.

      // Otherwise, remember that we have this call.
      AvailableCalls.insert(&Inst, std::make_pair(&Inst, CurrentSReqGen));
      continue;
    }

    // Handle EraVM llvm.eravm.meta intrinsic.
    if (auto *II = dyn_cast<IntrinsicInst>(&Inst)) {
      if (II->getIntrinsicID() == Intrinsic::eravm_meta) {
        // If we have an available version of this call, try to replace it.
        auto InVal = AvailableCalls.lookup(&Inst);
        if (InVal.first && InVal.second == CurrentMetaGen) {
          LLVM_DEBUG(dbgs() << "EraVMCSE META: " << Inst
                            << "  to: " << *InVal.first << '\n');
          removeAndReplaceInstruction(Inst, InVal.first);
          Changed = true;
          continue;
        }

        // Otherwise, remember this.
        AvailableCalls.insert(&Inst, std::make_pair(&Inst, CurrentMetaGen));
        continue;
      }
    }

    // Bump generation if this instruction prevents elimination of some
    // duplicated __system_request calls.
    if (preventsSReqElimination(Inst))
      ++CurrentSReqGen;

    // Bump generation if this instruction prevents elimination of duplicated
    // llvm.eravm.meta.
    if (preventsMetaElimination(Inst))
      ++CurrentMetaGen;
  }
  return Changed;
}

bool EraVMCSE::run() {
  std::deque<StackNode *> NodesToProcess;
  bool Changed = false;

  // Process the root node.
  NodesToProcess.push_back(new StackNode(
      AvailableCalls, &CurrentMetaGen, &CurrentSReqGen, DT.getRootNode(),
      DT.getRootNode()->begin(), DT.getRootNode()->end()));

  // Process the stack.
  while (!NodesToProcess.empty()) {
    StackNode *NodeToProcess = NodesToProcess.back();

    // Check if the node needs to be processed.
    if (!NodeToProcess->isProcessed()) {
      // Process the node.
      Changed |= processNode(NodeToProcess->node());
      NodeToProcess->process();
    } else if (NodeToProcess->childIter() != NodeToProcess->end()) {
      // Push the next child onto the stack.
      DomTreeNode *Child = NodeToProcess->nextChild();
      NodesToProcess.push_back(new StackNode(AvailableCalls, &CurrentMetaGen,
                                             &CurrentSReqGen, Child,
                                             Child->begin(), Child->end()));
    } else {
      // It has been processed, and there are no more children to process,
      // so delete it and pop it off the stack.
      delete NodeToProcess;
      NodesToProcess.pop_back();
    }
  }

  // If we changed something, run DCE to remove leftovers. This can happen when
  // instructions were simplified after CSE.
  if (Changed)
    for (auto &Inst : make_early_inc_range(instructions(F)))
      removeInstructionOnlyIfDead(Inst, true, true);

  return Changed;
}

bool EraVMCSELegacyPass::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
  auto &MSSA = getAnalysis<MemorySSAWrapperPass>().getMSSA();

  EraVMCSE CSE(F, TLI, DT, AC, MSSA);
  return CSE.run();
}

char EraVMCSELegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(EraVMCSELegacyPass, "eravm-cse", "EraVM CSE", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(EraVMCSELegacyPass, "eravm-cse", "EraVM CSE", false, false)

FunctionPass *llvm::createEraVMCSEPass() { return new EraVMCSELegacyPass; }

PreservedAnalyses EraVMCSEPass::run(Function &F, FunctionAnalysisManager &AM) {
  auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  auto &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();

  EraVMCSE CSE(F, TLI, DT, AC, MSSA);
  if (!CSE.run())
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<MemorySSAAnalysis>();
  return PA;
}
