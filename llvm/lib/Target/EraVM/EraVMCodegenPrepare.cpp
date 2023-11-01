//===-- EraVMCodegenPrepare.cpp - EraVM CodeGen Prepare ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EraVM specific version of CodeGenPrepare.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IntrinsicsEraVM.h"

#include "EraVM.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-codegen-prepare"

namespace llvm {
FunctionPass *createEraVMCodegenPrepare();
void initializeEraVMCodegenPreparePass(PassRegistry &);
} // namespace llvm

namespace {
struct EraVMCodegenPrepare : public FunctionPass {
public:
  static char ID;
  EraVMCodegenPrepare() : FunctionPass(ID) {
    initializeEraVMCodegenPreparePass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  bool convertPointerArithmetics(Function &F);

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
  bool rearrangeOverflowHandlingBranches(Function &F);

  StringRef getPassName() const override {
    return "Final transformations before code generation";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }
};
} // namespace

char EraVMCodegenPrepare::ID = 0;

INITIALIZE_PASS(EraVMCodegenPrepare, "eravm-codegen-prepare",
                "Final transformations before code generation", false, false)

bool EraVMCodegenPrepare::runOnFunction(Function &F) {
  bool Changed = false;

  std::vector<Instruction *> Replaced;
  for (auto &BB : F)
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      auto &I = *II;
      switch (I.getOpcode()) {
      default:
        break;
      case Instruction::ICmp: {
        auto &Cmp = cast<ICmpInst>(I);
        IRBuilder<> Builder(&I);
        // unsigned cmp is ok
        if (Cmp.isUnsigned())
          break;
        CmpInst::Predicate P = Cmp.getPredicate();
        auto *CmpVal = dyn_cast<ConstantInt>(I.getOperand(1));
        if (CmpVal && (CmpVal->getValue().isNullValue() ||
                       CmpVal->getValue().isOneValue())) {
          unsigned NumBits = CmpVal->getType()->getIntegerBitWidth();
          APInt Val = APInt(NumBits, -1, true).lshr(1);
          if (P == CmpInst::ICMP_SLT)
            P = CmpInst::ICMP_UGT;
          else
            break;

          if (P == CmpInst::ICMP_UGT) {
            auto Val256 =
                Builder.CreateZExt(Builder.getInt(Val), Builder.getIntNTy(256));
            auto Op256 =
                Builder.CreateZExt(I.getOperand(0), Builder.getIntNTy(256));
            auto *NewCmp = Builder.CreateICmp(P, Op256, Val256);
            if (CmpVal->getValue().isOneValue()) {
              auto *Cmp0 =
                  Builder.CreateICmp(CmpInst::ICMP_EQ, Op256,
                                     Builder.getInt(APInt(256, 0, false)));
              NewCmp = Builder.CreateOr(NewCmp, Cmp0);
            }
            I.replaceAllUsesWith(NewCmp);
            Replaced.push_back(&I);
            Changed = true;
          }
        }
        break;
      }
      case Instruction::Call: {
        // TODO: CPR-1353 Move to the constant folding pass.
        auto &Call = cast<CallInst>(I);
        Function *Callee = Call.getCalledFunction();
        if (!Callee && isa<BitCastOperator>(Call.getCalledOperand()))
          Callee = dyn_cast<Function>(
              cast<BitCastOperator>(Call.getCalledOperand())->getOperand(0));
        if (Callee && Callee->hasName()) {
          if (Callee->getName() == "__memset_uma_as1" &&
              isa<ConstantInt>(Call.getOperand(2)) &&
              (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__memset_uma_as2" &&
                     isa<ConstantInt>(Call.getOperand(2)) &&
                     (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__small_store_as1" &&
                     isa<ConstantInt>(Call.getOperand(2)) &&
                     (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__small_store_as2" &&
                     isa<ConstantInt>(Call.getOperand(2)) &&
                     (cast<ConstantInt>(Call.getOperand(2))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          } else if (Callee->getName() == "__small_store_as0" &&
                     isa<ConstantInt>(Call.getOperand(1)) &&
                     (cast<ConstantInt>(Call.getOperand(1))->isZero())) {
            Changed = true;
            Replaced.push_back(&I);
          }
        }
        break;
      }
      }
    }
  for (auto *I : Replaced)
    I->eraseFromParent();

  // convert pointer arithmetics to intrinsics
  Changed |= convertPointerArithmetics(F);

  Changed |= rearrangeOverflowHandlingBranches(F);
  return Changed;
}

static bool isUnsignedArithmeticOverflowInstruction(Instruction &I) {
  auto Call = dyn_cast<CallInst>(&I);
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

bool EraVMCodegenPrepare::rearrangeOverflowHandlingBranches(Function &F) {
  bool Changed = false;
  // iterate over all basic blocks:
  auto BBI = F.begin();
  auto BBE = F.end();
  while (BBI != BBE) {
    auto BB = &*BBI;
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

      auto Call = dyn_cast<CallInst>(&I);
      for (User *U : Call->users()) {

        // check extractvalue: there must be at least one use which is
        // extractvalue and index 1
        ExtractValueInst *ExtractValue = dyn_cast<ExtractValueInst>(U);
        if (!ExtractValue ||
            (!ExtractValue->hasIndices() || ExtractValue->getIndices()[0] != 1))
          continue;

        // check that the extracted value is used by a conditional branch in the
        // same basicblock:
        auto it = std::find_if(
            ExtractValue->user_begin(), ExtractValue->user_end(), [&](User *U) {
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
        BranchInst *Branch = cast<BranchInst>(*it);
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

bool EraVMCodegenPrepare::convertPointerArithmetics(Function &F) {
  const DataLayout *DL = &F.getParent()->getDataLayout();
  auto computeOffset = [&](GetElementPtrInst *GEP, IRBuilder<> &Builder) {
    Value *TmpOffset = Builder.getIntN(256, 0);
    APInt Offset(256, 0);
    if (GEP->accumulateConstantOffset(*DL, Offset)) {
      // We allow arbitrary GEP offset (256-bit)
      TmpOffset = Builder.getInt(Offset.sext(256));
    } else {
      for (gep_type_iterator GTI = gep_type_begin(GEP), E = gep_type_end(GEP);
           GTI != E; ++GTI) {
        Value *Op = GTI.getOperand();
        uint64_t S = DL->getTypeAllocSize(GTI.getIndexedType());

        // rely on folding down the pipeline to fold it.
        Value *Rhs = Builder.CreateMul(Builder.getIntN(256, S), Op);
        TmpOffset = Builder.CreateAdd(TmpOffset, Rhs);
      }
    }
    assert(TmpOffset != nullptr);
    return TmpOffset;
  };

  std::vector<Instruction *> Replaced;

  // look for GEP and convert them to PTR_ADD
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
        // Address Space must be return data address space
        if (GEP->getPointerAddressSpace() != EraVMAS::AS_GENERIC)
          continue;

        Value *Base = GEP->getOperand(0);

        IRBuilder<> Builder(&I);
        Value *Offset = computeOffset(GEP, Builder);
        Base = Builder.CreateBitCast(Base, Builder.getInt8PtrTy(3));

        auto *PtrAdd =
            Intrinsic::getDeclaration(F.getParent(), Intrinsic::eravm_ptr_add);

        Value *NewI = Builder.CreateCall(PtrAdd, {Base, Offset});
        NewI = Builder.CreateBitCast(NewI, GEP->getType());

        LLVM_DEBUG(dbgs() << "    Converted GEP:"; GEP->dump();
                   dbgs() << "    to: "; NewI->dump());
        GEP->replaceAllUsesWith(NewI);
        Replaced.push_back(GEP);
      } else if (auto *PtrToInt = dyn_cast<PtrToIntInst>(&I)) {
        if (PtrToInt->getPointerAddressSpace() != EraVMAS::AS_GENERIC)
          continue;
        // convert it to `PTR_TO_INT` intrinsic internally
        auto *ptr = PtrToInt->getPointerOperand();
        IRBuilder<> Builder(&I);
        Value *NewI = Builder.CreateCall(
            Intrinsic::getDeclaration(F.getParent(), Intrinsic::eravm_ptrtoint),
            {ptr});
        LLVM_DEBUG(dbgs() << "    Converted PtrToInt:"; PtrToInt->dump();
                   dbgs() << "    to: "; NewI->dump());
        PtrToInt->replaceAllUsesWith(NewI);
        Replaced.push_back(PtrToInt);
      }
    }
  }

  for (Instruction *I : Replaced)
    I->eraseFromParent();

  return !Replaced.empty();
}

FunctionPass *llvm::createEraVMCodegenPreparePass() {
  return new EraVMCodegenPrepare();
}
