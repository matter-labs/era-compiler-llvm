//===-- SyncVMLowerIntrinsics.cpp -----------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "SyncVMSubtarget.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

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

} // namespace

char SyncVMLowerIntrinsics::ID = 0;

INITIALIZE_PASS(SyncVMLowerIntrinsics, DEBUG_TYPE, "Lower intrinsics", false,
                false)

static void createMemCpyLoopKnownSize(Instruction *InsertBefore, Value *SrcAddr,
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

    // Cast the Src and Dst pointers to pointers to the loop operand type (if
    // needed).
    PointerType *SrcOpType = PointerType::get(LoopOpType, SrcAS);
    PointerType *DstOpType = PointerType::get(LoopOpType, DstAS);
    if (SrcAddr->getType() != SrcOpType) {
      SrcAddr = PLBuilder.CreateBitCast(SrcAddr, SrcOpType);
    }
    if (DstAddr->getType() != DstOpType) {
      DstAddr = PLBuilder.CreateBitCast(DstAddr, DstOpType);
    }

    Align PartDstAlign(commonAlignment(DstAlign, LoopOpSize));
    Align PartSrcAlign(commonAlignment(SrcAlign, LoopOpSize));

    IRBuilder<> LoopBuilder(LoopBB);
    PHINode *LoopIndex = LoopBuilder.CreatePHI(TypeOfCopyLen, 2, "loop-index");
    LoopIndex->addIncoming(ConstantInt::get(TypeOfCopyLen, 0U), PreLoopBB);
    // Loop Body
    Value *Offset = LoopBuilder.CreateMul(
        LoopBuilder.getInt(APInt(256, 32, false)), LoopIndex);
    Value *SrcGEP =
        LoopBuilder.CreatePtrToInt(SrcAddr, LoopBuilder.getInt256Ty());
    SrcGEP = LoopBuilder.CreateAdd(SrcGEP, Offset);
    SrcGEP = LoopBuilder.CreateIntToPtr(SrcGEP, SrcAddr->getType());
    Value *Load = LoopBuilder.CreateAlignedLoad(LoopOpType, SrcGEP,
                                                PartSrcAlign, SrcIsVolatile);
    Value *DstGEP =
        LoopBuilder.CreatePtrToInt(DstAddr, LoopBuilder.getInt256Ty());
    DstGEP = LoopBuilder.CreateAdd(DstGEP, Offset);
    DstGEP = LoopBuilder.CreateIntToPtr(DstGEP, DstAddr->getType());
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

    Value *SrcAddrInt =
        RBuilder.CreatePtrToInt(SrcAddr, RBuilder.getInt256Ty());
    SrcAddrInt = RBuilder.CreateNUWAdd(
        SrcAddrInt, RBuilder.getInt(APInt(256, BytesCopied, false)));
    SrcAddr = RBuilder.CreateIntToPtr(SrcAddrInt, SrcAddr->getType());
    Value *Load = RBuilder.CreateAlignedLoad(LoopOpType, SrcAddr, SrcAlign,
                                             SrcIsVolatile);
    Value *RuntimeResidual =
        RBuilder.getInt(APInt(256, 8 * RemainingBytes, false));
    Value *RuntimeResidualI =
        RBuilder.getInt(APInt(256, 256 - 8 * RemainingBytes, false));
    Value *LoadMask = RBuilder.CreateShl(RBuilder.getInt(APInt(256, -1, true)),
                                         RuntimeResidualI);
    Load = RBuilder.CreateAnd(Load, LoadMask);

    Value *DstAddrInt =
        RBuilder.CreatePtrToInt(DstAddr, RBuilder.getInt256Ty());
    DstAddrInt = RBuilder.CreateNUWAdd(
        DstAddrInt, RBuilder.getInt(APInt(256, BytesCopied, false)));
    DstAddr = RBuilder.CreateIntToPtr(DstAddrInt, DstAddr->getType());
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

static void createMemCpyLoopUnknownSize(Instruction *InsertBefore,
                                        Value *SrcAddr, Value *DstAddr,
                                        Value *CopyLen, Align SrcAlign,
                                        Align DstAlign, bool SrcIsVolatile,
                                        bool DstIsVolatile,
                                        const TargetTransformInfo &TTI) {
  BasicBlock *PreLoopBB = InsertBefore->getParent();
  BasicBlock *PostLoopBB =
      PreLoopBB->splitBasicBlock(InsertBefore, "post-loop-memcpy-expansion");

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
  if (SrcAddr->getType() != SrcOpType) {
    SrcAddr = PLBuilder.CreateBitCast(SrcAddr, SrcOpType);
  }
  if (DstAddr->getType() != DstOpType) {
    DstAddr = PLBuilder.CreateBitCast(DstAddr, DstOpType);
  }

  // Calculate the loop trip count, and remaining bytes to copy after the loop.
  Type *CopyLenType = CopyLen->getType();
  IntegerType *ILengthType = dyn_cast<IntegerType>(CopyLenType);
  assert(ILengthType &&
         "expected size argument to memcpy to be an integer type!");
  Type *Int8Type = Type::getInt8Ty(Ctx);
  bool LoopOpIsInt8 = LoopOpType == Int8Type;
  ConstantInt *CILoopOpSize = ConstantInt::get(ILengthType, LoopOpSize);
  Value *RuntimeLoopCount =
      LoopOpIsInt8 ? CopyLen : PLBuilder.CreateUDiv(CopyLen, CILoopOpSize);
  BasicBlock *LoopBB =
      BasicBlock::Create(Ctx, "loop-memcpy-expansion", ParentFunc, PostLoopBB);
  IRBuilder<> LoopBuilder(LoopBB);

  Align PartSrcAlign(commonAlignment(SrcAlign, LoopOpSize));
  Align PartDstAlign(commonAlignment(DstAlign, LoopOpSize));

  PHINode *LoopIndex = LoopBuilder.CreatePHI(CopyLenType, 2, "loop-index");
  LoopIndex->addIncoming(ConstantInt::get(CopyLenType, 0U), PreLoopBB);

  Value *Offset = LoopBuilder.CreateMul(
      LoopBuilder.getInt(APInt(256, 32, false)), LoopIndex);
  Value *SrcGEP =
      LoopBuilder.CreatePtrToInt(SrcAddr, LoopBuilder.getInt256Ty());
  SrcGEP = LoopBuilder.CreateAdd(SrcGEP, Offset);
  SrcGEP = LoopBuilder.CreateIntToPtr(SrcGEP, SrcAddr->getType());
  Value *Load = LoopBuilder.CreateAlignedLoad(LoopOpType, SrcGEP, PartSrcAlign,
                                              SrcIsVolatile);
  Value *DstGEP =
      LoopBuilder.CreatePtrToInt(DstAddr, LoopBuilder.getInt256Ty());
  DstGEP = LoopBuilder.CreateAdd(DstGEP, Offset);
  DstGEP = LoopBuilder.CreateIntToPtr(DstGEP, DstAddr->getType());
  LoopBuilder.CreateAlignedStore(Load, DstGEP, PartDstAlign, DstIsVolatile);

  Value *NewIndex =
      LoopBuilder.CreateAdd(LoopIndex, ConstantInt::get(CopyLenType, 1U));
  LoopIndex->addIncoming(NewIndex, LoopBB);

  if (!LoopOpIsInt8) {
    // Add in the
    Value *RuntimeResidual = PLBuilder.CreateURem(CopyLen, CILoopOpSize);
    Value *RuntimeBytesCopied = PLBuilder.CreateSub(CopyLen, RuntimeResidual);

    // Loop body for the residual copy.
    BasicBlock *ResLoopBB = BasicBlock::Create(
        Ctx, "loop-memcpy-residual", PreLoopBB->getParent(), PostLoopBB);
    // Residual loop header.
    BasicBlock *ResHeaderBB = BasicBlock::Create(
        Ctx, "loop-memcpy-residual-header", PreLoopBB->getParent(), nullptr);

    // Need to update the pre-loop basic block to branch to the correct place.
    // branch to the main loop if the count is non-zero, branch to the residual
    // loop if the copy size is smaller then 1 iteration of the main loop but
    // non-zero and finally branch to after the residual loop if the memcpy
    //  size is zero.
    ConstantInt *Zero = ConstantInt::get(ILengthType, 0U);
    PLBuilder.CreateCondBr(PLBuilder.CreateICmpNE(RuntimeLoopCount, Zero),
                           LoopBB, ResHeaderBB);
    PreLoopBB->getTerminator()->eraseFromParent();

    LoopBuilder.CreateCondBr(
        LoopBuilder.CreateICmpULT(NewIndex, RuntimeLoopCount), LoopBB,
        ResHeaderBB);

    // Determine if we need to branch to the residual loop or bypass it.
    IRBuilder<> RHBuilder(ResHeaderBB);
    RHBuilder.CreateCondBr(RHBuilder.CreateICmpNE(RuntimeResidual, Zero),
                           ResLoopBB, PostLoopBB);

    // Copy the residual with single byte load/store loop.
    IRBuilder<> ResBuilder(ResLoopBB);

    Value *SrcAddrInt =
        ResBuilder.CreatePtrToInt(SrcAddr, ResBuilder.getInt256Ty());
    SrcAddrInt = ResBuilder.CreateNUWAdd(SrcAddrInt, RuntimeBytesCopied);
    SrcAddr = ResBuilder.CreateIntToPtr(SrcAddrInt, SrcAddr->getType());
    Load = ResBuilder.CreateAlignedLoad(LoopOpType, SrcAddr, PartSrcAlign,
                                        SrcIsVolatile);
    RuntimeResidual = ResBuilder.CreateMul(
        ResBuilder.getInt(APInt(256, 8, false)), RuntimeResidual);
    Value *RuntimeResidualI = ResBuilder.CreateSub(
        ResBuilder.getInt(APInt(256, 256, false)), RuntimeResidual);
    Value *LoadMask = ResBuilder.CreateShl(
        ResBuilder.getInt(APInt(256, -1, true)), RuntimeResidualI);
    Load = ResBuilder.CreateAnd(Load, LoadMask);

    Value *DstAddrInt =
        ResBuilder.CreatePtrToInt(DstAddr, ResBuilder.getInt256Ty());
    DstAddrInt = ResBuilder.CreateNUWAdd(DstAddrInt, RuntimeBytesCopied);
    DstAddr = ResBuilder.CreateIntToPtr(DstAddrInt, DstAddr->getType());
    Value *Origin = ResBuilder.CreateAlignedLoad(LoopOpType, DstAddr,
                                                 PartDstAlign, DstIsVolatile);
    Value *OriginMask = ResBuilder.CreateLShr(
        ResBuilder.getInt(APInt(256, -1, true)), RuntimeResidual);
    Origin = ResBuilder.CreateAnd(Origin, OriginMask);
    Load = ResBuilder.CreateOr(Load, Origin);
    ResBuilder.CreateAlignedStore(Load, DstAddr, PartDstAlign, DstIsVolatile);
    ResBuilder.CreateBr(PostLoopBB);
  } else {
    // In this case the loop operand type was a byte, and there is no need for a
    // residual loop to copy the remaining memory after the main loop.
    // We do however need to patch up the control flow by creating the
    // terminators for the preloop block and the memcpy loop.
    ConstantInt *Zero = ConstantInt::get(ILengthType, 0U);
    PLBuilder.CreateCondBr(PLBuilder.CreateICmpNE(RuntimeLoopCount, Zero),
                           LoopBB, PostLoopBB);
    PreLoopBB->getTerminator()->eraseFromParent();
    LoopBuilder.CreateCondBr(
        LoopBuilder.CreateICmpULT(NewIndex, RuntimeLoopCount), LoopBB,
        PostLoopBB);
  }
}

// Lower memmove to IR. memmove is required to correctly copy overlapping memory
// regions; therefore, it has to check the relative positions of the source and
// destination pointers and choose the copy direction accordingly.
//
// The code below is an IR rendition of this C function:
//
// void* memmove(void* dst, const void* src, size_t n) {
//   unsigned char* d = dst;
//   const unsigned char* s = src;
//   if (s < d) {
//     // copy backwards
//     while (n--) {
//       d[n] = s[n];
//     }
//   } else {
//     // copy forward
//     for (size_t i = 0; i < n; ++i) {
//       d[i] = s[i];
//     }
//   }
//   return dst;
// }
static void createMemMoveLoop(Instruction *InsertBefore, Value *SrcAddr,
                              Value *DstAddr, Value *CopyLen, Align SrcAlign,
                              Align DstAlign, bool SrcIsVolatile,
                              bool DstIsVolatile) {
  Type *TypeOfCopyLen = CopyLen->getType();
  BasicBlock *OrigBB = InsertBefore->getParent();
  Function *F = OrigBB->getParent();
  const DataLayout &DL = F->getParent()->getDataLayout();

  Type *EltTy = cast<PointerType>(SrcAddr->getType())->getElementType();

  // Create the a comparison of src and dst, based on which we jump to either
  // the forward-copy part of the function (if src >= dst) or the backwards-copy
  // part (if src < dst).
  // SplitBlockAndInsertIfThenElse conveniently creates the basic if-then-else
  // structure. Its block terminators (unconditional branches) are replaced by
  // the appropriate conditional branches when the loop is built.
  ICmpInst *PtrCompare = new ICmpInst(InsertBefore, ICmpInst::ICMP_ULT, SrcAddr,
                                      DstAddr, "compare_src_dst");
  Instruction *ThenTerm, *ElseTerm;
  SplitBlockAndInsertIfThenElse(PtrCompare, InsertBefore, &ThenTerm, &ElseTerm);

  // Each part of the function consists of two blocks:
  //   copy_backwards:        used to skip the loop when n == 0
  //   copy_backwards_loop:   the actual backwards loop BB
  //   copy_forward:          used to skip the loop when n == 0
  //   copy_forward_loop:     the actual forward loop BB
  BasicBlock *CopyBackwardsBB = ThenTerm->getParent();
  CopyBackwardsBB->setName("copy_backwards");
  BasicBlock *CopyForwardBB = ElseTerm->getParent();
  CopyForwardBB->setName("copy_forward");
  BasicBlock *ExitBB = InsertBefore->getParent();
  ExitBB->setName("memmove_done");

  unsigned PartSize = DL.getTypeStoreSize(EltTy);
  Align PartSrcAlign(commonAlignment(SrcAlign, PartSize));
  Align PartDstAlign(commonAlignment(DstAlign, PartSize));

  // Initial comparison of n == 0 that lets us skip the loops altogether. Shared
  // between both backwards and forward copy clauses.
  ICmpInst *CompareN =
      new ICmpInst(OrigBB->getTerminator(), ICmpInst::ICMP_EQ, CopyLen,
                   ConstantInt::get(TypeOfCopyLen, 0), "compare_n_to_0");

  // Copying backwards.
  BasicBlock *LoopBB = BasicBlock::Create(
      F->getContext(), "copy_backwards_loop", F, CopyForwardBB);
  IRBuilder<> LoopBuilder(LoopBB);
  PHINode *LoopPhi = LoopBuilder.CreatePHI(TypeOfCopyLen, 0);
  Value *IndexPtr = LoopBuilder.CreateSub(
      LoopPhi, ConstantInt::get(TypeOfCopyLen, 1), "index_ptr");
  Value *Element = LoopBuilder.CreateAlignedLoad(
      EltTy, LoopBuilder.CreateInBoundsGEP(EltTy, SrcAddr, IndexPtr),
      PartSrcAlign, "element");
  LoopBuilder.CreateAlignedStore(
      Element, LoopBuilder.CreateInBoundsGEP(EltTy, DstAddr, IndexPtr),
      PartDstAlign);
  LoopBuilder.CreateCondBr(
      LoopBuilder.CreateICmpEQ(IndexPtr, ConstantInt::get(TypeOfCopyLen, 0)),
      ExitBB, LoopBB);
  LoopPhi->addIncoming(IndexPtr, LoopBB);
  LoopPhi->addIncoming(CopyLen, CopyBackwardsBB);
  BranchInst::Create(ExitBB, LoopBB, CompareN, ThenTerm);
  ThenTerm->eraseFromParent();

  // Copying forward.
  BasicBlock *FwdLoopBB =
      BasicBlock::Create(F->getContext(), "copy_forward_loop", F, ExitBB);
  IRBuilder<> FwdLoopBuilder(FwdLoopBB);
  PHINode *FwdCopyPhi = FwdLoopBuilder.CreatePHI(TypeOfCopyLen, 0, "index_ptr");
  Value *SrcGEP = FwdLoopBuilder.CreateInBoundsGEP(EltTy, SrcAddr, FwdCopyPhi);
  Value *FwdElement =
      FwdLoopBuilder.CreateAlignedLoad(EltTy, SrcGEP, PartSrcAlign, "element");
  Value *DstGEP = FwdLoopBuilder.CreateInBoundsGEP(EltTy, DstAddr, FwdCopyPhi);
  FwdLoopBuilder.CreateAlignedStore(FwdElement, DstGEP, PartDstAlign);
  Value *FwdIndexPtr = FwdLoopBuilder.CreateAdd(
      FwdCopyPhi, ConstantInt::get(TypeOfCopyLen, 1), "index_increment");
  FwdLoopBuilder.CreateCondBr(FwdLoopBuilder.CreateICmpEQ(FwdIndexPtr, CopyLen),
                              ExitBB, FwdLoopBB);
  FwdCopyPhi->addIncoming(FwdIndexPtr, FwdLoopBB);
  FwdCopyPhi->addIncoming(ConstantInt::get(TypeOfCopyLen, 0), CopyForwardBB);

  BranchInst::Create(ExitBB, FwdLoopBB, CompareN, ElseTerm);
  ElseTerm->eraseFromParent();
}

static void createMemSetLoop(Instruction *InsertBefore, Value *DstAddr,
                             Value *CopyLen, Value *SetValue, Align DstAlign,
                             bool IsVolatile) {
  Module *M = InsertBefore->getModule();
  IRBuilder<> Builder(InsertBefore);
  Value *SetValueI256 = [SetValue, &Builder] {
    std::vector<unsigned> ShiftAmounts = {8, 16, 32, 64, 128};
    if (auto *CVal = dyn_cast<ConstantInt>(SetValue)) {
      APInt ValInt = APInt(256, CVal->getValue().getZExtValue(), false);
      for (unsigned ShiftAmount : ShiftAmounts)
        ValInt = ValInt.shl(ShiftAmount) | ValInt;
      return static_cast<Value *>(Builder.getInt(ValInt));
    }
    Value *Result = Builder.CreateZExt(SetValue, Builder.getInt256Ty());
    Value *Prev = Result;
    for (unsigned ShiftAmount : ShiftAmounts) {
      Result = Builder.CreateShl(Result, ShiftAmount);
      Prev = Result = Builder.CreateOr(Result, Prev);
    }
    return Result;
  }();
  unsigned AS = cast<PointerType>(DstAddr->getType())->getAddressSpace();
  Value *CopyLenI256 = Builder.CreateZExt(CopyLen, Builder.getInt256Ty());
  assert(AS <= SyncVMAS::AS_HEAP &&
         "Memset is unsupported for calldata and returndata");
  if (AS == SyncVMAS::AS_HEAP) {
    Value *AddrP256 = Builder.CreateBitCast(
        DstAddr, Builder.getInt256Ty()->getPointerTo(SyncVMAS::AS_HEAP));
    Function *Memset = M->getFunction("__memset_uma_as1");
    Builder.CreateCall(Memset, {AddrP256, SetValueI256, CopyLenI256});
  } else if (AS == SyncVMAS::AS_STACK) {
    Function *Memset = M->getFunction("__memset_as0");
    if (DstAlign.value() % 32) {
      Value *AddrI256 = Builder.CreatePtrToInt(DstAddr, Builder.getInt256Ty());
      Value *Peel = Builder.CreateURem(AddrI256, Builder.getIntN(256, 32));
      Value *Lt = Builder.CreateICmpULT(Peel, CopyLenI256);
      Peel = Builder.CreateSelect(Lt, Peel, CopyLenI256);
      Instruction *Cmp = cast<Instruction>(
          Builder.CreateICmpEQ(Peel, Builder.getIntN(256, 0)));
      BasicBlock *OrigBB = InsertBefore->getParent();
      BasicBlock *ContBB =
          InsertBefore->getParent()->splitBasicBlock(InsertBefore, "memset-split");
      BasicBlock *SmallStoreBB = BasicBlock::Create(
          M->getContext(), "smallstore", InsertBefore->getFunction(), ContBB);
      std::prev(OrigBB->end())->eraseFromParent();
      Builder.SetInsertPoint(OrigBB, OrigBB->end());
      Builder.CreateCondBr(Cmp, ContBB, SmallStoreBB);

      IRBuilder<> SmallStoreBuilder(SmallStoreBB);
      Function *SmallStore = M->getFunction("__small_store_as0");
      Value *PeelBits = SmallStoreBuilder.CreateMul(
          Peel, SmallStoreBuilder.getIntN(256, 8), "peel-bits", true, true);
      Value *ShiftPeelVal = SmallStoreBuilder.CreateSub(
          SmallStoreBuilder.getIntN(256, 256), PeelBits, "shift-peel", true, true);
      Value *PeelVal = SmallStoreBuilder.CreateLShr(SetValueI256, ShiftPeelVal);
      SmallStoreBuilder.CreateCall(SmallStore, {AddrI256, PeelVal, PeelBits});
      SmallStoreBuilder.CreateBr(ContBB);

      Builder.SetInsertPoint(ContBB, ContBB->begin());
      Value *CopyLenRem =
          Builder.CreateSub(CopyLenI256, Peel, "rembytes", true, true);
      AddrI256 = Builder.CreateAdd(AddrI256, Peel, "memset-addr", true, true);
      Value *AddrP256 = Builder.CreateIntToPtr(
          AddrI256, Builder.getInt256Ty()->getPointerTo(SyncVMAS::AS_STACK));
      Builder.CreateCall(Memset, {AddrP256, SetValueI256, CopyLenRem});
    } else {
      Value *AddrP256 = Builder.CreateBitCast(
          DstAddr, Builder.getInt256Ty()->getPointerTo(SyncVMAS::AS_STACK));
      Builder.CreateCall(Memset, {AddrP256, SetValueI256, CopyLenI256});
    }
  }
}

void expandMemCpyAsLoop(MemCpyInst *Memcpy, const TargetTransformInfo &TTI) {
  if (ConstantInt *CI = dyn_cast<ConstantInt>(Memcpy->getLength())) {
    createMemCpyLoopKnownSize(
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
    createMemCpyLoopUnknownSize(
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

void expandMemMoveAsLoop(MemMoveInst *Memmove) {
  assert(false);
  createMemMoveLoop(/* InsertBefore */ Memmove,
                    /* SrcAddr */ Memmove->getRawSource(),
                    /* DstAddr */ Memmove->getRawDest(),
                    /* CopyLen */ Memmove->getLength(),
                    /* SrcAlign */ Memmove->getSourceAlign().valueOrOne(),
                    /* DestAlign */ Memmove->getDestAlign().valueOrOne(),
                    /* SrcIsVolatile */ Memmove->isVolatile(),
                    /* DstIsVolatile */ Memmove->isVolatile());
}

void expandMemSetAsLoop(MemSetInst *Memset) {
  createMemSetLoop(/* InsertBefore */ Memset,
                   /* DstAddr */ Memset->getRawDest(),
                   /* CopyLen */ Memset->getLength(),
                   /* SetValue */ Memset->getValue(),
                   /* Alignment */ Memset->getDestAlign().valueOrOne(),
                   Memset->isVolatile());
}

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
      expandMemCpyAsLoop(Memcpy, TTI);
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
