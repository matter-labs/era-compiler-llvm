//===--- EraVMIndexedMemOpsPrepare.cpp - Prepare for indexed MemOps -------===//
//
// \file
// This pass intends to utilize the SCEV info to find load/store that can be
// optimized to indexed load/store provided by EraVM.
//
// Please be noted:
//   We don't generated indexed ld/st in current pass. We just re-write the IR
//   to favor the subsequent EraVMCombineToIndexedMemops pass which will
//   generate indexed ld/st in return.
//
// The algorithm contains two steps: the first step is to search the qualified
// load/store instructions and the second step is to re-write them to favor
// the subsequent EraVMCombineToIndexedMemops pass.
//
// In the first step, looking for pattern:
//
// loop-preheader:
//   ......
//   ......
// loop-body:
//   %PtrInc = phi [ %InitVal, loop-prehead ], [ %Val, loop-body]
//   %BasePtr = GEP %BaseAddr, %index0, ..., %PtrInc
//   %LSVal = load/store %BasePtr
//   ......
//
// the requirements are:
//   1). The BasePtr of load/store instruction will be increased by one cell per
//       iteration.
//   2). The BasePtr is defined by a GEP instruction of which the last index
//       operand is defined by a PHI node.
//
// After find the pattern, re-write the instructions into below formats:
//
// loop-preheader:
//   ......
//   %BasePtrInit = GEP %BaseAddr, %index0, ..., %InitVal
//   ......
// loop-body:
//    %BasePtrNew = phi [%BasePtrInit, loop-prehead], [%BasePtrInc, loop-body]
//    %BasePtrInc = GEP %BasePtrNew, i256 1
//    %LSVal = load/store %BasePtrNew
//    ......
//
// Notes to the algorithm:
//
//   1). The BasePtr can be increased by one cell via loop index or a seperated
//       instruction.
//   2). The GEP instruction used to define BasePtr can contain one or more
//       index operands. If it contains only one index operand and
//       the initial value of this index operand is zero, then we don't need
//       to insert BasePtrInit instruction in preheader, otherwise we have to
//       add.
//   3). The memory intrinsic expansion pass will generate loops to fulfill the
//       functions of memory intrinsic, hence this pass is supposed to be run
//       after the memory intrinsic expansion pass, the generated loops can
//       benefit from this pass.
//
// TODO: CPR-1421 Remove loop index
//       The pointers inside the loop will be increased per iteration.
//       We can use them to judge whether loop should exit. In this way, if
//       loop index isn't used by any other places, we can optimize out loop
//       index along with instruction used to increase its value.
//============================================================================//

#include "EraVM.h"

#include "EraVMSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-indexed-memops-prepare"
#define ERAVM_PREPARE_INDEXED_MEMOPS_NAME                                      \
  "EraVM recognize and rewrite instructions for indexed memory operations"

namespace {

class EraVMIndexedMemOpsPrepare : public LoopPass {

  ScalarEvolution *SE = nullptr;
  LLVMContext *Ctx = nullptr;
  Loop *CurrentLoop = nullptr;

public:
  static char ID;

  EraVMIndexedMemOpsPrepare() : LoopPass(ID) {}

  bool runOnLoop(Loop *L, LPPassManager &) override;

  StringRef getPassName() const override {
    return "EraVM Recognize Indexed Load/Store";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<TargetPassConfig>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.setPreservesCFG();
  }

private:
  /// Check whether \p BasePtr is valid and increased by one cell.
  bool isValidGEPAndIncByOneCell(GetElementPtrInst *BasePtr) const;

  /// The \p BasePtr is defined by a GEP instruction. The \p MemOpInst
  /// is the load or store instruction that uses \p BasePtr as a base
  /// address. If this \p BasePtr matches our requirements, then this
  /// function will rewrite it into patterns that can be recognized by
  /// the subsequent Combine pass to generate indexed load/store.
  bool rewriteToFavorIndexedMemOps(GetElementPtrInst *BasePtr,
                                   Instruction *MemOpInst);
};

} // end namespace

bool EraVMIndexedMemOpsPrepare::rewriteToFavorIndexedMemOps(
    GetElementPtrInst *BasePtr, Instruction *MemOpInst) {

  Type *TypeOfCopyLen = IntegerType::getInt256Ty(*Ctx);
  BasicBlock *LoopPreheader = CurrentLoop->getLoopPreheader();

  Value *SrcOperand = getPointerOperand(BasePtr);
  Type *RstType = BasePtr->getResultElementType();
  Type *GEPType = BasePtr->getType();
  const bool IsGEPInBounds = BasePtr->isInBounds();
  const unsigned GEPElementSize =
      BasePtr->getResultElementType()->getPrimitiveSizeInBits();

  // Create a new PHI node to represent the change of BasePtr.
  IRBuilder<> Builder(BasePtr);
  Builder.SetInsertPoint(CurrentLoop->getHeader()->getFirstNonPHI());
  auto *NewBasePtr = Builder.CreatePHI(GEPType, 2);
  BasePtr->replaceAllUsesWith(NewBasePtr);

  // If the BasePtr GEP inst has only one index operand and its initial value is
  // zero. Then we can use GEP's source operand as NewBasePtr's initial
  // value. No need to add instruction to initialize NewBasePtr in loop
  // preheader. Otherwise we need to initialize NewBasePtr in loop preheader.
  const unsigned LastIndexOperandId = BasePtr->getNumOperands() - 1;
  const auto *PHI = cast<PHINode>(BasePtr->getOperand(LastIndexOperandId));
  Value *GEPLastIndexOperandInitValue =
      PHI->getIncomingValueForBlock(LoopPreheader);
  const auto *IndexVal = dyn_cast<ConstantInt>(GEPLastIndexOperandInitValue);

  if ((LastIndexOperandId == 1) && IndexVal && IndexVal->isZero()) {
    NewBasePtr->addIncoming(SrcOperand, LoopPreheader);
    BasePtr->eraseFromParent();
  } else {
    // Turn BasePtr GEP inst into a plain initialization inst
    // and move it into loop preheader.
    BasePtr->setOperand(LastIndexOperandId, GEPLastIndexOperandInitValue);
    BasePtr->moveBefore(LoopPreheader->getTerminator());
    NewBasePtr->addIncoming(BasePtr, LoopPreheader);
  }

  // Add GEP instruction to increase the NewBasePtr by one cell.
  Builder.SetInsertPoint(MemOpInst);
  auto *IncNewBasePtr = Builder.CreateInBoundsGEP(
      RstType, NewBasePtr,
      ConstantInt::get(TypeOfCopyLen, (TypeOfCopyLen->getScalarSizeInBits() /
                                       GEPElementSize)));
  if (!IsGEPInBounds)
    cast<GetElementPtrInst>(IncNewBasePtr)->setIsInBounds(false);

  // Add to the PHI
  NewBasePtr->addIncoming(IncNewBasePtr, MemOpInst->getParent());

  return true;
}

bool EraVMIndexedMemOpsPrepare::isValidGEPAndIncByOneCell(
    GetElementPtrInst *BasePtr) const {
  // FIXME: Once we support address space generic, we can remove it.
  if (BasePtr->getPointerAddressSpace() == EraVMAS::AS_GENERIC)
    return false;

  // Use SCEV info to check whether this BasePtr is increased
  // by one cell per iteration.
  const SCEV *SCEVPtr = SE->getSCEVAtScope(BasePtr, CurrentLoop);

  if (SCEVPtr) {
    const SCEVAddRecExpr *AddRec = dyn_cast<SCEVAddRecExpr>(SCEVPtr);
    if (!AddRec)
      return false;
    const SCEVConstant *Step =
        dyn_cast<SCEVConstant>(AddRec->getStepRecurrence(*SE));
    if (!Step)
      return false;
    const APInt StrideVal = Step->getAPInt();
    if (StrideVal != 32)
      return false;
  }

  // The last index operand of this BasePtr GEP instruction must be
  // defined by a PHI node. Only via this PHI node, the BasePtr
  // can be increased along with loop iteration.
  const unsigned LastIndexOperandId = BasePtr->getNumOperands() - 1;
  Value *LastIndexOperand = BasePtr->getOperand(LastIndexOperandId);
  const PHINode *PHI = dyn_cast<PHINode>(LastIndexOperand);
  if (!PHI)
    return false;

  return true;
}

bool EraVMIndexedMemOpsPrepare::runOnLoop(Loop *L, LPPassManager &) {
  bool Changed = false;

  if (skipLoop(L))
    return false;

  if (!L->isLoopSimplifyForm())
    return false;

  SE = &getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  Ctx = &L->getLoopPreheader()->getContext();
  CurrentLoop = L;

  for (auto *const BB : L->blocks()) {
    for (auto &I : *BB) {
      GetElementPtrInst *BasePtrValue = nullptr;
      if (LoadInst *LMemI = dyn_cast<LoadInst>(&I)) {
        BasePtrValue = dyn_cast<GetElementPtrInst>(LMemI->getPointerOperand());
      } else if (StoreInst *SMemI = dyn_cast<StoreInst>(&I)) {
        BasePtrValue = dyn_cast<GetElementPtrInst>(SMemI->getPointerOperand());
      } else {
        continue; // Skip if current inst isn't load nor store inst.
      }

      if (!BasePtrValue)
        continue;

      // Use SCEV info to check whether baseptr is increased by one cell
      if (!isValidGEPAndIncByOneCell(BasePtrValue))
        continue;

      // Let's try to rewrite the GEP instruction in a way that will
      // favor the subsequent CombineToIndexedMemops pass.
      Changed |= rewriteToFavorIndexedMemOps(BasePtrValue, &I);
    }
  }

  return Changed;
}

Pass *llvm::createEraVMIndexedMemOpsPreparePass() {
  return new EraVMIndexedMemOpsPrepare();
}

char EraVMIndexedMemOpsPrepare::ID = 0;

INITIALIZE_PASS_BEGIN(EraVMIndexedMemOpsPrepare, DEBUG_TYPE,
                      ERAVM_PREPARE_INDEXED_MEMOPS_NAME, false, false)
INITIALIZE_PASS_END(EraVMIndexedMemOpsPrepare, DEBUG_TYPE,
                    ERAVM_PREPARE_INDEXED_MEMOPS_NAME, false, false)
