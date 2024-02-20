//===-- EraVMLowerIntrinsics.cpp - Lower intrinsics -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass lowers memcpy, memmove and memset intrinsics.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

#include "EraVMSubtarget.h"

#define DEBUG_TYPE "eravm-lower-intrinsics"

using namespace llvm;

namespace {

class EraVMLowerIntrinsics : public ModulePass {
public:
  static char ID;

  EraVMLowerIntrinsics() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  StringRef getPassName() const override { return "EraVM Lower Intrinsics"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }
};

} // namespace

char EraVMLowerIntrinsics::ID = 0;

INITIALIZE_PASS(EraVMLowerIntrinsics, DEBUG_TYPE, "Lower intrinsics", false,
                false)

// Lower memmove to IR. memmove is required to correctly copy overlapping memory
// regions; therefore, it has to check the relative positions of the source and
// destination pointers and choose the copy direction accordingly.
static void createEraVMMemMoveLoop(Instruction *InsertBefore, Value *SrcAddr,
                                   Value *DstAddr, Value *CopyLen,
                                   Align SrcAlign, Align DstAlign,
                                   bool SrcIsVolatile, bool DstIsVolatile,
                                   const TargetTransformInfo &TTI) {
  // No need to expand zero length moves.
  if (auto *CI = dyn_cast<ConstantInt>(CopyLen))
    if (CI->isZero())
      return;

  BasicBlock *OrigBB = InsertBefore->getParent();
  Function *F = OrigBB->getParent();
  const DataLayout &DL = F->getParent()->getDataLayout();
  LLVMContext &Ctx = F->getContext();
  unsigned SrcAS = cast<PointerType>(SrcAddr->getType())->getAddressSpace();
  unsigned DstAS = cast<PointerType>(DstAddr->getType())->getAddressSpace();
  Type *Int8Type = Type::getInt8Ty(Ctx);

  Type *LoopOpType = TTI.getMemcpyLoopLoweringType(
      Ctx, CopyLen, SrcAS, DstAS, SrcAlign.value(), DstAlign.value());
  int LoopOpSize = DL.getTypeStoreSize(LoopOpType);

  IRBuilder<> PLBuilder(InsertBefore);

  PointerType *SrcOpType = PointerType::get(LoopOpType, SrcAS);
  PointerType *DstOpType = PointerType::get(LoopOpType, DstAS);
  if (SrcAddr->getType() != SrcOpType)
    SrcAddr = PLBuilder.CreateBitCast(SrcAddr, SrcOpType, "src-addr-casted");
  if (DstAddr->getType() != DstOpType)
    DstAddr = PLBuilder.CreateBitCast(DstAddr, DstOpType, "dst-addr-casted");

  if (CopyLen->getType() != LoopOpType)
    CopyLen = PLBuilder.CreateZExt(CopyLen, LoopOpType, "copy-len-zext");

  // Calculate the loop byte count, and remaining bytes to copy after the loops.
  Value *LoopCount = PLBuilder.CreateAnd(
      CopyLen, ConstantInt::get(LoopOpType, -LoopOpSize, true),
      "loop-bytes-count");
  Value *ResidualBytes = PLBuilder.CreateAnd(
      CopyLen, ConstantInt::get(LoopOpType, LoopOpSize - 1), "residual-bytes");

  // Create the a comparison of src and dst, based on which we jump to either
  // the forward-copy part of the function (if src >= dst) or the backwards-copy
  // part (if src < dst).
  auto *PtrCompare = new ICmpInst(InsertBefore, ICmpInst::ICMP_ULT, SrcAddr,
                                  DstAddr, "compare-src-dst");

  // Initial comparison of loop-bytes-count == 0 that lets us skip the loops
  // altogether. Shared between both backwards and forward copy clauses.
  auto *CompareLoopBytesCount =
      new ICmpInst(InsertBefore, ICmpInst::ICMP_EQ, LoopCount,
                   ConstantInt::get(LoopOpType, 0), "compare-lcb-to-0");

  // Initial comparison of residual-bytes == 0 that lets us skip the
  // residual BB. Shared between both backwards and forward copy clauses.
  auto *CompareResidualBytes =
      new ICmpInst(InsertBefore, ICmpInst::ICMP_EQ, ResidualBytes,
                   ConstantInt::get(LoopOpType, 0), "compare-rb-to-0");

  auto *ExitBB =
      OrigBB->splitBasicBlock(InsertBefore->getIterator(), "memmove-done");
  auto *CopyBwdBB = BasicBlock::Create(Ctx, "copy-backwards", F, ExitBB);
  auto *CopyBwdResidualCondBB =
      BasicBlock::Create(Ctx, "copy-backwards-residual-cond", F, ExitBB);
  auto *CopyBwdLoopPreheaderBB =
      BasicBlock::Create(Ctx, "copy-backwards-loop-preheader", F, ExitBB);
  auto *CopyBwdLoopBB =
      BasicBlock::Create(Ctx, "copy-backwards-loop", F, ExitBB);
  auto *CopyFwdBB = BasicBlock::Create(Ctx, "copy-forward", F, ExitBB);
  auto *CopyFwdResidualCondBB =
      BasicBlock::Create(Ctx, "copy-forward-residual-cond", F, ExitBB);
  // Create an empty preheader BB for copy-forward-loop since in this loop
  // we can use indexed instructions and EraVMIndexedMemOpsPrepare
  // requires loops to be in a simplify form.
  auto *CopyFwdLoopPreheaderBB =
      BasicBlock::Create(Ctx, "copy-forward-loop-preheader", F, ExitBB);
  auto *CopyFwdLoopBB = BasicBlock::Create(Ctx, "copy-forward-loop", F, ExitBB);
  // Create an empty exit BB for copy-forward-loop since in this loop
  // we can use indexed instructions and EraVMIndexedMemOpsPrepare
  // requires loops to be in a simplify form.
  auto *CopyFwdLoopExitBB =
      BasicBlock::Create(Ctx, "copy-forward-loop-exit", F, ExitBB);
  auto *CopyFwdResidualBB =
      BasicBlock::Create(Ctx, "copy-forward-residual", F, ExitBB);
  auto *ResBB = BasicBlock::Create(Ctx, "memmove-residual", F, ExitBB);

  // Remove terminator after splitBasicBlock.
  OrigBB->getTerminator()->eraseFromParent();

  // Create necessary branches in the following BBs.
  BranchInst::Create(CopyBwdBB, CopyFwdBB, PtrCompare, OrigBB);
  BranchInst::Create(CopyBwdResidualCondBB, CopyBwdLoopPreheaderBB,
                     CompareLoopBytesCount, CopyBwdBB);
  BranchInst::Create(CopyFwdResidualCondBB, CopyFwdLoopPreheaderBB,
                     CompareLoopBytesCount, CopyFwdBB);
  BranchInst::Create(ExitBB, ResBB, CompareResidualBytes,
                     CopyBwdResidualCondBB);
  BranchInst::Create(ExitBB, CopyFwdResidualBB, CompareResidualBytes,
                     CopyFwdResidualCondBB);
  BranchInst::Create(CopyFwdLoopBB, CopyFwdLoopPreheaderBB);
  BranchInst::Create(CopyFwdResidualCondBB, CopyFwdLoopExitBB);

  Align PartSrcAlign(commonAlignment(SrcAlign, LoopOpSize));
  Align PartDstAlign(commonAlignment(DstAlign, LoopOpSize));

  // Copying backwards preheader.
  IRBuilder<> BwdLoopPreheaderBuilder(CopyBwdLoopPreheaderBB);
  Value *SrcBwdAddr = BwdLoopPreheaderBuilder.CreateInBoundsGEP(
      Int8Type, SrcAddr, ResidualBytes, "src-bwd-start");
  Value *DstBwdAddr = BwdLoopPreheaderBuilder.CreateInBoundsGEP(
      Int8Type, DstAddr, ResidualBytes, "dst-bwd-start");
  BwdLoopPreheaderBuilder.CreateBr(CopyBwdLoopBB);

  // Copying backwards.
  IRBuilder<> BwdLoopBuilder(CopyBwdLoopBB);
  PHINode *BwdLoopPhi = BwdLoopBuilder.CreatePHI(LoopOpType, 2, "bytes-count");
  Value *BytesDecrement = BwdLoopBuilder.CreateAdd(
      BwdLoopPhi, ConstantInt::get(LoopOpType, -LoopOpSize, true),
      "decrement-bytes");
  Value *BwdElement = BwdLoopBuilder.CreateAlignedLoad(
      LoopOpType,
      BwdLoopBuilder.CreateInBoundsGEP(Int8Type, SrcBwdAddr, BytesDecrement,
                                       "load-addr"),
      PartSrcAlign, SrcIsVolatile, "element");
  BwdLoopBuilder.CreateAlignedStore(
      BwdElement,
      BwdLoopBuilder.CreateInBoundsGEP(Int8Type, DstBwdAddr, BytesDecrement,
                                       "store-addr"),
      PartDstAlign, DstIsVolatile);
  BwdLoopBuilder.CreateCondBr(
      BwdLoopBuilder.CreateICmpEQ(
          BytesDecrement, ConstantInt::get(LoopOpType, 0), "compare-bytes"),
      CopyBwdResidualCondBB, CopyBwdLoopBB);
  BwdLoopPhi->addIncoming(BytesDecrement, CopyBwdLoopBB);
  BwdLoopPhi->addIncoming(LoopCount, CopyBwdLoopPreheaderBB);

  // Copying forward.
  IRBuilder<> FwdLoopBuilder(CopyFwdLoopBB);
  PHINode *FwdCopyPhi = FwdLoopBuilder.CreatePHI(LoopOpType, 2, "bytes-count");
  Value *FwdElement = FwdLoopBuilder.CreateAlignedLoad(
      LoopOpType,
      FwdLoopBuilder.CreateInBoundsGEP(Int8Type, SrcAddr, FwdCopyPhi,
                                       "load-addr"),
      PartSrcAlign, SrcIsVolatile, "element");
  FwdLoopBuilder.CreateAlignedStore(
      FwdElement,
      FwdLoopBuilder.CreateInBoundsGEP(Int8Type, DstAddr, FwdCopyPhi,
                                       "store-addr"),
      PartDstAlign, DstIsVolatile);
  Value *BytesIncrement = FwdLoopBuilder.CreateAdd(
      FwdCopyPhi, ConstantInt::get(LoopOpType, LoopOpSize), "increment-bytes");
  FwdLoopBuilder.CreateCondBr(
      FwdLoopBuilder.CreateICmpEQ(BytesIncrement, LoopCount, "compare-bytes"),
      CopyFwdLoopExitBB, CopyFwdLoopBB);
  FwdCopyPhi->addIncoming(BytesIncrement, CopyFwdLoopBB);
  FwdCopyPhi->addIncoming(ConstantInt::get(LoopOpType, 0),
                          CopyFwdLoopPreheaderBB);

  // Residual forward.
  IRBuilder<> FwdResBuilder(CopyFwdResidualBB);
  Value *SrcFwdResAddr = FwdResBuilder.CreateInBoundsGEP(
      Int8Type, SrcAddr, LoopCount, "src-fwd-res-addr");
  Value *DstFwdResAddr = FwdResBuilder.CreateInBoundsGEP(
      Int8Type, DstAddr, LoopCount, "dst-fwd-res-adr");
  FwdResBuilder.CreateBr(ResBB);

  // Residual.
  IRBuilder<> ResBuilder(ResBB);
  PHINode *SrcResPhi = ResBuilder.CreatePHI(SrcOpType, 2, "src-res-addr");
  SrcResPhi->addIncoming(SrcFwdResAddr, CopyFwdResidualBB);
  SrcResPhi->addIncoming(SrcAddr, CopyBwdResidualCondBB);

  PHINode *DstResPhi = ResBuilder.CreatePHI(DstOpType, 2, "dst-res-addr");
  DstResPhi->addIncoming(DstFwdResAddr, CopyFwdResidualBB);
  DstResPhi->addIncoming(DstAddr, CopyBwdResidualCondBB);

  Value *SrcLoad = ResBuilder.CreateAlignedLoad(
      LoopOpType, SrcResPhi, PartSrcAlign, SrcIsVolatile, "src-load");
  Value *ResidualBits = ResBuilder.CreateMul(ConstantInt::get(LoopOpType, 8),
                                             ResidualBytes, "res-bits");
  Value *UpperBits = ResBuilder.CreateSub(
      ConstantInt::get(LoopOpType, LoopOpSize * 8), ResidualBits, "upper-bits");
  Value *SrcMask = ResBuilder.CreateShl(ConstantInt::get(LoopOpType, -1, true),
                                        UpperBits, "src-mask");
  Value *SrcMasked = ResBuilder.CreateAnd(SrcLoad, SrcMask, "src-masked");

  Value *DstLoad = ResBuilder.CreateAlignedLoad(
      LoopOpType, DstResPhi, PartDstAlign, DstIsVolatile, "dst-load");
  Value *DstMask = ResBuilder.CreateLShr(ConstantInt::get(LoopOpType, -1, true),
                                         ResidualBits, "dst-mask");
  Value *DstMasked = ResBuilder.CreateAnd(DstLoad, DstMask, "dst-masked");
  Value *StoreElement =
      ResBuilder.CreateOr(SrcMasked, DstMasked, "store-element");
  ResBuilder.CreateAlignedStore(StoreElement, DstResPhi, PartDstAlign,
                                DstIsVolatile);
  ResBuilder.CreateBr(ExitBB);
}

// TODO: CPR-1380 Move this function to common code.
static std::optional<APInt> getConstPtr(const Value *Ptr,
                                        const DataLayout &DL) {
  if (const auto *CPN = dyn_cast<ConstantPointerNull>(Ptr))
    return APInt::getZero(DL.getPointerTypeSizeInBits(CPN->getType()));

  if (const auto *CE = dyn_cast<ConstantExpr>(Ptr))
    if (CE->getOpcode() == Instruction::IntToPtr)
      if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(0)))
        return CI->getValue();

  if (const auto *IntToPtr = dyn_cast<IntToPtrInst>(Ptr))
    if (auto *CI = dyn_cast<ConstantInt>(IntToPtr->getOperand(0)))
      return CI->getValue();
  return std::nullopt;
}

// In case Src and Dst are constant, return whether we are doing
// copy forward (if src >= dst).
static bool isCopyForward(Value *Src, Value *Dst, const DataLayout &DL) {
  auto SrcPtr = getConstPtr(Src, DL);
  auto DstPtr = getConstPtr(Dst, DL);
  if (!SrcPtr || !DstPtr || SrcPtr->getBitWidth() != DstPtr->getBitWidth())
    return false;
  return SrcPtr->uge(*DstPtr);
}

static void expandEraVMMemMoveAsLoop(MemMoveInst *Memmove,
                                     const TargetTransformInfo &TTI) {
  Value *SrcAddr = Memmove->getRawSource();
  Value *DstAddr = Memmove->getRawDest();
  Value *CopyLen = Memmove->getLength();
  Align SrcAlign = Memmove->getSourceAlign().valueOrOne();
  Align DstAlign = Memmove->getDestAlign().valueOrOne();
  bool SrcIsVolatile = Memmove->isVolatile();
  bool DstIsVolatile = SrcIsVolatile;
  unsigned SrcAS = SrcAddr->getType()->getPointerAddressSpace();
  unsigned DstAS = DstAddr->getType()->getPointerAddressSpace();
  const DataLayout &DL = Memmove->getModule()->getDataLayout();

  // We can call memcpy if address spaces are not the same (it means
  // they don't alias), or if we are doing copy forward.
  if (SrcAS != DstAS || isCopyForward(SrcAddr, DstAddr, DL)) {
    if (auto *CI = dyn_cast<ConstantInt>(CopyLen))
      createEraVMMemCpyLoopKnownSize(
          /* InsertBefore */ Memmove, SrcAddr, DstAddr, CI, SrcAlign, DstAlign,
          SrcIsVolatile, DstIsVolatile, TTI);
    else
      createEraVMMemCpyLoopUnknownSize(
          /* InsertBefore */ Memmove, SrcAddr, DstAddr, CopyLen, SrcAlign,
          DstAlign, SrcIsVolatile, DstIsVolatile, TTI);
    return;
  }

  createEraVMMemMoveLoop(/* InsertBefore */ Memmove, SrcAddr, DstAddr, CopyLen,
                         SrcAlign, DstAlign, SrcIsVolatile, DstIsVolatile, TTI);
}

static void ExpandMemCpyAsLoop(MemCpyInst *Memcpy,
                               const TargetTransformInfo &TTI) {
  if (auto *CI = dyn_cast<ConstantInt>(Memcpy->getLength())) {
    createEraVMMemCpyLoopKnownSize(
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
    createEraVMMemCpyLoopUnknownSize(
        /* InsertBefore */ Memcpy,
        /* SrcAddr */ Memcpy->getRawSource(),
        /* DstAddr */ Memcpy->getRawDest(),
        /* CopyLen */ Memcpy->getLength(),
        /* SrcAlign */ Memcpy->getSourceAlign().valueOrOne(),
        /* DestAlign */ Memcpy->getDestAlign().valueOrOne(),
        /* SrcIsVolatile */ Memcpy->isVolatile(),
        /* DstIsVolatile */ Memcpy->isVolatile(),
        /* TargetTransformInfo */ TTI);
  }
}

static bool
expandMemIntrinsicUses(Function &F,
                       function_ref<TargetTransformInfo &(Function &)> GetTTI) {
  Intrinsic::ID ID = F.getIntrinsicID();
  bool Changed = false;

  for (auto I = F.user_begin(), E = F.user_end(); I != E;) {
    auto *Inst = cast<Instruction>(*I);
    ++I;

    switch (ID) {
    case Intrinsic::memcpy: {
      auto *Memcpy = cast<MemCpyInst>(Inst);
      Function *ParentFunc = Memcpy->getParent()->getParent();
      const TargetTransformInfo &TTI = GetTTI(*ParentFunc);
      ExpandMemCpyAsLoop(Memcpy, TTI);
      Changed = true;
      Memcpy->eraseFromParent();

      break;
    }
    case Intrinsic::memmove: {
      auto *Memmove = cast<MemMoveInst>(Inst);
      Function *ParentFunc = Memmove->getFunction();
      const TargetTransformInfo &TTI = GetTTI(*ParentFunc);
      expandEraVMMemMoveAsLoop(Memmove, TTI);
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

static bool runImpl(Module &M,
                    function_ref<TargetTransformInfo &(Function &)> GetTTI) {
  bool Changed = false;

  for (Function &F : M) {
    if (!F.isDeclaration())
      continue;

    switch (F.getIntrinsicID()) {
    case Intrinsic::memcpy:
    case Intrinsic::memmove:
    case Intrinsic::memset:
      if (expandMemIntrinsicUses(F, GetTTI))
        Changed = true;
      break;

    default:
      break;
    }
  }

  return Changed;
}

bool EraVMLowerIntrinsics::runOnModule(Module &M) {
  auto GetTTI = [this](Function &F) -> TargetTransformInfo & {
    return this->getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
  };
  return runImpl(M, GetTTI);
}

ModulePass *llvm::createEraVMLowerIntrinsicsPass() {
  return new EraVMLowerIntrinsics();
}

PreservedAnalyses EraVMLowerIntrinsicsPass::run(Module &M,
                                                ModuleAnalysisManager &AM) {
  auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  auto GetTTI = [&FAM](Function &F) -> TargetTransformInfo & {
    return FAM.getResult<TargetIRAnalysis>(F);
  };
  if (runImpl(M, GetTTI))
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
