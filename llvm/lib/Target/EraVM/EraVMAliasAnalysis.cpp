//===-- EraVMAliasAnalysis.cpp - EraVM alias analysis -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EraVM address space based alias analysis pass.
//
//===----------------------------------------------------------------------===//

#include "EraVMAliasAnalysis.h"
#include "EraVM.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Instructions.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "eravm-aa"

AnalysisKey EraVMAA::Key;

// Register this pass...
char EraVMAAWrapperPass::ID = 0;
char EraVMExternalAAWrapper::ID = 0;

INITIALIZE_PASS(EraVMAAWrapperPass, "eravm-aa",
                "EraVM Address space based Alias Analysis", false, true)

INITIALIZE_PASS(EraVMExternalAAWrapper, "eravm-aa-wrapper",
                "EraVM Address space based Alias Analysis Wrapper", false, true)

ImmutablePass *llvm::createEraVMAAWrapperPass() {
  return new EraVMAAWrapperPass();
}

ImmutablePass *llvm::createEraVMExternalAAWrapperPass() {
  return new EraVMExternalAAWrapper();
}

EraVMAAWrapperPass::EraVMAAWrapperPass() : ImmutablePass(ID) {
  initializeEraVMAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

void EraVMAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

// Get the base pointer and the offset by looking through the
// ptrtoint+arithmetic+inttoptr sequence.
static std::pair<const Value *, APInt>
getBaseWithOffset(const Value *V, unsigned BitWidth, unsigned MaxLookup = 6) {
  auto Offset = APInt::getZero(BitWidth);

  // Bail out if this is not an inttoptr instruction.
  if (!isa<IntToPtrInst>(V))
    return {nullptr, Offset};

  V = cast<IntToPtrInst>(V)->getOperand(0);

  for (unsigned I = 0; I < MaxLookup; ++I) {
    // If this is a ptrtoint, get the operand and stop the lookup.
    if (const auto *PtrToInt = dyn_cast<PtrToIntInst>(V)) {
      V = PtrToInt->getOperand(0);
      break;
    }

    // We only handle binary operations.
    const auto *BOp = dyn_cast<BinaryOperator>(V);
    if (!BOp)
      break;

    // With constant operand.
    const auto *CI = dyn_cast<ConstantInt>(BOp->getOperand(1));
    if (!CI)
      break;

    auto Val = CI->getValue();

    // If the value is larger than the current bitwidth, extend the offset
    // and remember the new bitwidth.
    if (Val.getBitWidth() > BitWidth) {
      BitWidth = Val.getBitWidth();
      Offset = Offset.sext(BitWidth);
    } else {
      // Otherwise, extend the value to the current bitwidth.
      Val = Val.sext(BitWidth);
    }

    // TODO: CPR-1652 Support more instructions.
    if (BOp->getOpcode() == Instruction::Add)
      Offset += Val;
    else if (BOp->getOpcode() == Instruction::Sub)
      Offset -= Val;
    else
      break;

    V = BOp->getOperand(0);
  }
  return {V, Offset};
}

static std::optional<APInt> getConstStartLoc(const MemoryLocation &Loc,
                                             unsigned BitWidth) {
  if (const auto *CPN = dyn_cast<ConstantPointerNull>(Loc.Ptr))
    return APInt::getZero(BitWidth);

  if (const auto *CE = dyn_cast<ConstantExpr>(Loc.Ptr)) {
    if (CE->getOpcode() == Instruction::IntToPtr) {
      if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(0)))
        return CI->getValue();
    }
  }

  if (const auto *IntToPtr = dyn_cast<IntToPtrInst>(Loc.Ptr)) {
    if (auto *CI = dyn_cast<ConstantInt>(IntToPtr->getOperand(0)))
      return CI->getValue();
  }

  return std::nullopt;
}

AliasResult EraVMAAResult::alias(const MemoryLocation &LocA,
                                 const MemoryLocation &LocB, AAQueryInfo &AAQI,
                                 const Instruction *I) {
  const unsigned ASA = LocA.Ptr->getType()->getPointerAddressSpace();
  const unsigned ASB = LocB.Ptr->getType()->getPointerAddressSpace();

  // If we don't know what this is, bail out.
  if (ASA > EraVMAS::MAX_ADDRESS || ASB > EraVMAS::MAX_ADDRESS)
    return AAResultBase::alias(LocA, LocB, AAQI, I);

  // Pointers can't alias if they are not in the same address space.
  if (ASA != ASB)
    return AliasResult::NoAlias;

  // Since pointers are in the same address space, handle only cases that are
  // interesting to us.
  if (ASA != EraVMAS::AS_HEAP && ASA != EraVMAS::AS_HEAP_AUX &&
      ASA != EraVMAS::AS_STORAGE && ASA != EraVMAS::AS_TRANSIENT)
    return AAResultBase::alias(LocA, LocB, AAQI, I);

  // Don't check unknown memory locations.
  if (!LocA.Size.isPrecise() || !LocB.Size.isPrecise())
    return AAResultBase::alias(LocA, LocB, AAQI, I);

  // Only 256-bit keys are valid for storage.
  if (ASA == EraVMAS::AS_STORAGE || ASA == EraVMAS::AS_TRANSIENT) {
    constexpr unsigned KeyByteWidth = 32;
    if (LocA.Size != KeyByteWidth || LocB.Size != KeyByteWidth)
      return AAResultBase::alias(LocA, LocB, AAQI, I);
  }

  unsigned BitWidth = DL.getPointerSizeInBits(ASA);
  auto StartA = getConstStartLoc(LocA, BitWidth);
  auto StartB = getConstStartLoc(LocB, BitWidth);

  // If we don't have constant start locations, try to get the base pointer and
  // the offset. In case we managed to find them and pointers have the same
  // base, we can compare offsets to prove aliasing. Otherwise, forward the
  // query to the next alias analysis.
  if (!StartA || !StartB) {
    auto [BaseA, OffsetA] = getBaseWithOffset(LocA.Ptr, BitWidth);
    auto [BaseB, OffsetB] = getBaseWithOffset(LocB.Ptr, BitWidth);
    if (!BaseA || !BaseB || BaseA != BaseB)
      return AAResultBase::alias(LocA, LocB, AAQI, I);

    StartA = OffsetA;
    StartB = OffsetB;
  }

  // Extend start locations to the same bitwidth and not less than pointer size.
  unsigned MaxBitWidth = std::max(StartA->getBitWidth(), StartB->getBitWidth());
  MaxBitWidth = std::max(MaxBitWidth, BitWidth);
  const APInt StartAVal = StartA->sext(MaxBitWidth);
  const APInt StartBVal = StartB->sext(MaxBitWidth);

  // Keys in storage can't overlap.
  if (ASA == EraVMAS::AS_STORAGE || ASA == EraVMAS::AS_TRANSIENT) {
    if (StartAVal == StartBVal)
      return AliasResult::MustAlias;
    return AliasResult::NoAlias;
  }

  // If heap locations are the same, they either must or partially alias based
  // on the size of locations.
  if (StartAVal == StartBVal) {
    if (LocA.Size == LocB.Size)
      return AliasResult::MustAlias;
    return AliasResult::PartialAlias;
  }

  auto DoesOverlap = [](const APInt &X, const APInt &XEnd, const APInt &Y) {
    return Y.sge(X) && Y.slt(XEnd);
  };

  // For heap accesses, if locations don't overlap, they are not aliasing.
  if (!DoesOverlap(StartAVal, StartAVal + LocA.Size.getValue(), StartBVal) &&
      !DoesOverlap(StartBVal, StartBVal + LocB.Size.getValue(), StartAVal))
    return AliasResult::NoAlias;
  return AliasResult::PartialAlias;
}
