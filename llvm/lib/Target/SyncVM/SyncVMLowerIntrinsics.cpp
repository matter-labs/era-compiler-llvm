//===-- SyncVMLowerIntrinsics.cpp -----------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"

#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

#include "SyncVMSubtarget.h"

#define DEBUG_TYPE "syncvm-lower-intrinsics"

using namespace llvm;

namespace {

class SyncVMLowerIntrinsics : public ModulePass {
public:
  static char ID;

  SyncVMLowerIntrinsics() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  bool expandMemIntrinsicUses(Function &F);
  StringRef getPassName() const override { return "SyncVM Lower Intrinsics"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }
};

// Expands memcopy of known size to loop copying data cell by cell and
// residual copying the remaining bytes via load, store and bitwise operations.
// TODO: Aligned memory instructions are cheaper than misaligned, so consider
// peel + loop + residual structure for copying.
void CreateMemCpyLoopKnownSize(Instruction *InsertBefore, Value *SrcAddr,
                               Value *DstAddr, ConstantInt *CopyLen,
                               Align SrcAlign, Align DstAlign,
                               bool SrcIsVolatile, bool DstIsVolatile,
                               const TargetTransformInfo &TTI) {
  // No need to expand zero length copies.
  if (CopyLen->isZero())
    return;

  BasicBlock *PreLoopBB = InsertBefore->getParent();
  BasicBlock *PostLoopBB = nullptr;
  Function *ParentFunc = PreLoopBB->getParent();
  LLVMContext &Ctx = PreLoopBB->getContext();
  const DataLayout &DL = ParentFunc->getParent()->getDataLayout();

  unsigned SrcAS = cast<PointerType>(SrcAddr->getType())->getAddressSpace();
  unsigned DstAS = cast<PointerType>(DstAddr->getType())->getAddressSpace();

  Type *TypeOfCopyLen = CopyLen->getType();
  Type *LoopOpType = TTI.getMemcpyLoopLoweringType(
      Ctx, CopyLen, SrcAS, DstAS, SrcAlign.value(), DstAlign.value());

  unsigned LoopOpSize = DL.getTypeStoreSize(LoopOpType);
  uint64_t LoopEndCount = CopyLen->getZExtValue() / LoopOpSize;

  if (LoopEndCount != 0) {
    // Split
    PostLoopBB = PreLoopBB->splitBasicBlock(InsertBefore, "memcpy-split");
    BasicBlock *LoopBB =
        BasicBlock::Create(Ctx, "load-store-loop", ParentFunc, PostLoopBB);
    PreLoopBB->getTerminator()->setSuccessor(0, LoopBB);

    IRBuilder<> PLBuilder(PreLoopBB->getTerminator());

    PointerType *SrcOpType = PointerType::get(LoopOpType, SrcAS);
    PointerType *DstOpType = PointerType::get(LoopOpType, DstAS);
    if (SrcAddr->getType() != SrcOpType)
      SrcAddr = PLBuilder.CreateBitCast(SrcAddr, SrcOpType);
    if (DstAddr->getType() != DstOpType)
      DstAddr = PLBuilder.CreateBitCast(DstAddr, DstOpType);

    Align PartDstAlign(commonAlignment(DstAlign, LoopOpSize));
    Align PartSrcAlign(commonAlignment(SrcAlign, LoopOpSize));

    IRBuilder<> LoopBuilder(LoopBB);
    PHINode *LoopIndex = LoopBuilder.CreatePHI(TypeOfCopyLen, 2, "loop-index");
    LoopIndex->addIncoming(ConstantInt::get(TypeOfCopyLen, 0U), PreLoopBB);

    // Loop Body
    Value *SrcGEP =
        LoopBuilder.CreateInBoundsGEP(LoopOpType, SrcAddr, LoopIndex);
    Value *Load = LoopBuilder.CreateAlignedLoad(LoopOpType, SrcGEP,
                                                PartSrcAlign, SrcIsVolatile);
    Value *DstGEP =
        LoopBuilder.CreateInBoundsGEP(LoopOpType, DstAddr, LoopIndex);
    LoopBuilder.CreateAlignedStore(Load, DstGEP, PartDstAlign, DstIsVolatile);

    Value *NewIndex =
        LoopBuilder.CreateAdd(LoopIndex, ConstantInt::get(TypeOfCopyLen, 1U));
    LoopIndex->addIncoming(NewIndex, LoopBB);

    // Create the loop branch condition.
    Constant *LoopEndCI = ConstantInt::get(TypeOfCopyLen, LoopEndCount);
    LoopBuilder.CreateCondBr(LoopBuilder.CreateICmpULT(NewIndex, LoopEndCI),
                             LoopBB, PostLoopBB);
  }

  uint64_t BytesCopied = LoopEndCount * LoopOpSize;
  uint64_t RemainingBytes = CopyLen->getZExtValue() - BytesCopied;
  if (RemainingBytes) {
    IRBuilder<> RBuilder(PostLoopBB ? PostLoopBB->getFirstNonPHI()
                                    : InsertBefore);
    uint64_t GepIndex = BytesCopied / 32;
    SrcAddr = RBuilder.CreateInBoundsGEP(
        LoopOpType, SrcAddr, RBuilder.getInt(APInt(256, GepIndex, true)));

    Value *Load = RBuilder.CreateAlignedLoad(LoopOpType, SrcAddr, SrcAlign,
                                             SrcIsVolatile);
    Value *RuntimeResidual =
        RBuilder.getInt(APInt(256, 8 * RemainingBytes, false));
    Value *RuntimeResidualI =
        RBuilder.getInt(APInt(256, 256 - 8 * RemainingBytes, false));
    Value *LoadMask = RBuilder.CreateShl(RBuilder.getInt(APInt(256, -1, true)),
                                         RuntimeResidualI);
    Load = RBuilder.CreateAnd(Load, LoadMask);

    DstAddr = RBuilder.CreateInBoundsGEP(
        LoopOpType, DstAddr, RBuilder.getInt(APInt(256, GepIndex, true)));

    Value *Origin = RBuilder.CreateAlignedLoad(LoopOpType, DstAddr, DstAlign,
                                               DstIsVolatile);
    Value *OriginMask = RBuilder.CreateLShr(
        RBuilder.getInt(APInt(256, -1, true)), RuntimeResidual);
    Origin = RBuilder.CreateAnd(Origin, OriginMask);
    Load = RBuilder.CreateOr(Load, Origin);
    RBuilder.CreateAlignedStore(Load, DstAddr, DstAlign, DstIsVolatile);
    BytesCopied += RemainingBytes;
  }
  assert(BytesCopied == CopyLen->getZExtValue() &&
         "Bytes copied should match size in the call!");
}

// Expands memcopy of unknown size to loop copying data cell by cell and
// residual copying the remaining bytes via load, store and bitwise operations.
// TODO: Aligned memory instructions are cheaper than misaligned, so consider
// peel + loop + residual structure for copying.
void CreateMemCpyLoopUnknownSize(Instruction *InsertBefore, Value *SrcAddr,
                                 Value *DstAddr, Value *CopyLen, Align SrcAlign,
                                 Align DstAlign, bool SrcIsVolatile,
                                 bool DstIsVolatile,
                                 const TargetTransformInfo &TTI) {
  BasicBlock *PreLoopBB = InsertBefore->getParent();
  BasicBlock *PostLoopBB =
      PreLoopBB->splitBasicBlock(InsertBefore, "memcpy-split");

  Function *ParentFunc = PreLoopBB->getParent();
  const DataLayout &DL = ParentFunc->getParent()->getDataLayout();
  LLVMContext &Ctx = PreLoopBB->getContext();
  unsigned SrcAS = cast<PointerType>(SrcAddr->getType())->getAddressSpace();
  unsigned DstAS = cast<PointerType>(DstAddr->getType())->getAddressSpace();

  Type *LoopOpType = TTI.getMemcpyLoopLoweringType(
      Ctx, CopyLen, SrcAS, DstAS, SrcAlign.value(), DstAlign.value());
  unsigned LoopOpSize = DL.getTypeStoreSize(LoopOpType);

  IRBuilder<> PLBuilder(PreLoopBB->getTerminator());

  PointerType *SrcOpType = PointerType::get(LoopOpType, SrcAS);
  PointerType *DstOpType = PointerType::get(LoopOpType, DstAS);
  if (SrcAddr->getType() != SrcOpType)
    SrcAddr = PLBuilder.CreateBitCast(SrcAddr, SrcOpType);
  if (DstAddr->getType() != DstOpType)
    DstAddr = PLBuilder.CreateBitCast(DstAddr, DstOpType);

  // Calculate the loop trip count, and remaining bytes to copy after the loop.
  Type *CopyLenType = CopyLen->getType();
  IntegerType *ILengthType = dyn_cast<IntegerType>(CopyLenType);

  ConstantInt *CILoopOpSize = ConstantInt::get(ILengthType, LoopOpSize);
  Value *LoopCount = PLBuilder.CreateUDiv(CopyLen, CILoopOpSize, "loop-count");
  Value *ResidualBytes = PLBuilder.CreateURem(CopyLen, CILoopOpSize, "residual-bytes");
  BasicBlock *LoopBB =
      BasicBlock::Create(Ctx, "load-store-loop", ParentFunc, PostLoopBB);
  IRBuilder<> LoopBuilder(LoopBB);

  Align PartSrcAlign(commonAlignment(SrcAlign, LoopOpSize));
  Align PartDstAlign(commonAlignment(DstAlign, LoopOpSize));

  PHINode *LoopIndex = LoopBuilder.CreatePHI(CopyLenType, 2, "loop-index");
  LoopIndex->addIncoming(ConstantInt::get(CopyLenType, 0U), PreLoopBB);

  Value *SrcGEP = LoopBuilder.CreateInBoundsGEP(LoopOpType, SrcAddr, LoopIndex);
  Value *Load = LoopBuilder.CreateAlignedLoad(LoopOpType, SrcGEP, PartSrcAlign,
                                              SrcIsVolatile);
  Value *DstGEP = LoopBuilder.CreateInBoundsGEP(LoopOpType, DstAddr, LoopIndex);
  LoopBuilder.CreateAlignedStore(Load, DstGEP, PartDstAlign, DstIsVolatile);

  Value *NewIndex =
      LoopBuilder.CreateAdd(LoopIndex, ConstantInt::get(CopyLenType, 1U));
  LoopIndex->addIncoming(NewIndex, LoopBB);

  // BB for the residual copy.
  BasicBlock *ResBB = BasicBlock::Create(Ctx, "memcpy-residual",
                                         PreLoopBB->getParent(), PostLoopBB);
  // Condition check if residual copy is needed.
  BasicBlock *ResCondBB = BasicBlock::Create(Ctx, "memcpy-residual-cond",
                                             PreLoopBB->getParent(), nullptr);

  // Need to update the pre-loop basic block to branch to the correct place.
  // branch to the main loop if the count is non-zero, branch to the residual
  // condition if the copy size is smaller then 1 iteration of the main loop but
  // non-zero and finally branch to after the residual copy if the memcpy
  //  size is zero.
  ConstantInt *Zero = ConstantInt::get(ILengthType, 0U);
  PLBuilder.CreateCondBr(PLBuilder.CreateICmpNE(LoopCount, Zero), LoopBB,
                         ResCondBB);
  PreLoopBB->getTerminator()->eraseFromParent();

  LoopBuilder.CreateCondBr(LoopBuilder.CreateICmpULT(NewIndex, LoopCount),
                           LoopBB, ResCondBB);

  // Determine if we need to branch to the residual copy or bypass it.
  IRBuilder<> RHBuilder(ResCondBB);

  RHBuilder.CreateCondBr(RHBuilder.CreateICmpNE(ResidualBytes, Zero), ResBB,
                         PostLoopBB);

  // Copy the residual load/store and bitwise operations.
  IRBuilder<> ResBuilder(ResBB);

  SrcAddr = ResBuilder.CreateInBoundsGEP(LoopOpType, SrcAddr, LoopCount);
  Load = ResBuilder.CreateAlignedLoad(LoopOpType, SrcAddr, PartSrcAlign,
                                      SrcIsVolatile);
  Value *ResidualBits = ResBuilder.CreateMul(
      ResBuilder.getInt(APInt(256, 8, false)), ResidualBytes);
  Value *ResidualBitsI = ResBuilder.CreateSub(
      ResBuilder.getInt(APInt(256, 256, false)), ResidualBits);
  Value *LoadMask = ResBuilder.CreateShl(
      ResBuilder.getInt(APInt(256, -1, true)), ResidualBitsI);
  Load = ResBuilder.CreateAnd(Load, LoadMask);

  DstAddr = ResBuilder.CreateInBoundsGEP(LoopOpType, DstAddr, LoopCount);
  Value *Origin = ResBuilder.CreateAlignedLoad(LoopOpType, DstAddr,
                                               PartDstAlign, DstIsVolatile);
  Value *OriginMask = ResBuilder.CreateLShr(
      ResBuilder.getInt(APInt(256, -1, true)), ResidualBits);
  Origin = ResBuilder.CreateAnd(Origin, OriginMask);
  Load = ResBuilder.CreateOr(Load, Origin);
  ResBuilder.CreateAlignedStore(Load, DstAddr, PartDstAlign, DstIsVolatile);
  ResBuilder.CreateBr(PostLoopBB);
}

void ExpandMemCpyAsLoop(MemCpyInst *Memcpy, const TargetTransformInfo &TTI) {
  if (ConstantInt *CI = dyn_cast<ConstantInt>(Memcpy->getLength())) {
    CreateMemCpyLoopKnownSize(
        /* InsertBefore */ Memcpy,
        /* SrcAddr */ Memcpy->getRawSource(),
        /* DstAddr */ Memcpy->getRawDest(),
        /* CopyLen */ CI,
        /* SrcAlign */ Memcpy->getSourceAlign().valueOrOne(),
        /* DestAlign */ Memcpy->getDestAlign().valueOrOne(),
        /* SrcIsVolatile */ Memcpy->isVolatile(),
        /* DstIsVolatile */ Memcpy->isVolatile(),
        /* TargetTransformInfo */ TTI);
  } else {
    CreateMemCpyLoopUnknownSize(
        /* InsertBefore */ Memcpy,
        /* SrcAddr */ Memcpy->getRawSource(),
        /* DstAddr */ Memcpy->getRawDest(),
        /* CopyLen */ Memcpy->getLength(),
        /* SrcAlign */ Memcpy->getSourceAlign().valueOrOne(),
        /* DestAlign */ Memcpy->getDestAlign().valueOrOne(),
        /* SrcIsVolatile */ Memcpy->isVolatile(),
        /* DstIsVolatile */ Memcpy->isVolatile(),
        /* TargetTransfomrInfo */ TTI);
  }
}

} // namespace

char SyncVMLowerIntrinsics::ID = 0;

INITIALIZE_PASS(SyncVMLowerIntrinsics, DEBUG_TYPE, "Lower intrinsics", false,
                false)

bool SyncVMLowerIntrinsics::expandMemIntrinsicUses(Function &F) {
  Intrinsic::ID ID = F.getIntrinsicID();
  bool Changed = false;

  for (auto I = F.user_begin(), E = F.user_end(); I != E;) {
    Instruction *Inst = cast<Instruction>(*I);
    ++I;

    switch (ID) {
    case Intrinsic::memcpy: {
      auto *Memcpy = cast<MemCpyInst>(Inst);
      Function *ParentFunc = Memcpy->getParent()->getParent();
      const TargetTransformInfo &TTI =
          getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*ParentFunc);
      ExpandMemCpyAsLoop(Memcpy, TTI);
      Changed = true;
      Memcpy->eraseFromParent();

      break;
    }
    case Intrinsic::memmove: {
      auto *Memmove = cast<MemMoveInst>(Inst);
      expandMemMoveAsLoop(Memmove);
      Changed = true;
      Memmove->eraseFromParent();

      break;
    }
    case Intrinsic::memset: {
      auto *Memset = cast<MemSetInst>(Inst);
      expandMemSetAsLoop(Memset);
      Changed = true;
      Memset->eraseFromParent();

      break;
    }
    default:
      break;
    }
  }

  return Changed;
}

bool SyncVMLowerIntrinsics::runOnModule(Module &M) {
  bool Changed = false;

  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;

    switch (F.getIntrinsicID()) {
    case Intrinsic::memcpy:
    case Intrinsic::memmove:
    case Intrinsic::memset:
      if (expandMemIntrinsicUses(F))
        Changed = true;
      break;

    default:
      break;
    }
  }

  return Changed;
}

ModulePass *llvm::createSyncVMLowerIntrinsicsPass() {
  return new SyncVMLowerIntrinsics();
}
