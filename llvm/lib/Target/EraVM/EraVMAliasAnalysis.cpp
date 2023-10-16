//===-- EraVMAliasAnalysis.cpp - EraVM alias analysis ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This is the EraVM address space based alias analysis pass.
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMAliasAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Instructions.h"

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

static Optional<APInt> getConstStartLoc(const MemoryLocation &Loc,
                                        const DataLayout &DL) {
  if (auto *CPN = dyn_cast<ConstantPointerNull>(Loc.Ptr))
    return APInt::getZero(DL.getPointerTypeSizeInBits(CPN->getType()));

  if (auto *CE = dyn_cast<ConstantExpr>(Loc.Ptr)) {
    if (CE->getOpcode() == Instruction::IntToPtr) {
      if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(0)))
        return CI->getValue();
    }
  }

  if (auto *IntToPtr = dyn_cast<IntToPtrInst>(Loc.Ptr)) {
    if (auto *CI = dyn_cast<ConstantInt>(IntToPtr->getOperand(0)))
      return CI->getValue();
  }

  return None;
}

AliasResult EraVMAAResult::alias(const MemoryLocation &LocA,
                                 const MemoryLocation &LocB,
                                 AAQueryInfo &AAQI) {
  const unsigned ASA = LocA.Ptr->getType()->getPointerAddressSpace();
  const unsigned ASB = LocB.Ptr->getType()->getPointerAddressSpace();

  // If we don't know what this is, bail out.
  if (ASA > EraVMAS::MAX_ADDRESS || ASB > EraVMAS::MAX_ADDRESS)
    return AAResultBase::alias(LocA, LocB, AAQI);

  // Pointers can't alias if they are not in the same address space.
  if (ASA != ASB)
    return AliasResult::NoAlias;

  // Since pointers are in the same address space, handle only cases that are
  // interesting to us.
  if (ASA != EraVMAS::AS_HEAP && ASA != EraVMAS::AS_HEAP_AUX &&
      ASA != EraVMAS::AS_STORAGE)
    return AAResultBase::alias(LocA, LocB, AAQI);

  // Don't check unknown memory locations.
  if (!LocA.Size.isPrecise() || !LocB.Size.isPrecise())
    return AAResultBase::alias(LocA, LocB, AAQI);

  // Only 256-bit keys are valid for storage.
  if (ASA == EraVMAS::AS_STORAGE) {
    constexpr unsigned KeyByteWidth = 32;
    if (LocA.Size != KeyByteWidth || LocB.Size != KeyByteWidth)
      return AAResultBase::alias(LocA, LocB, AAQI);
  }

  auto StartA = getConstStartLoc(LocA, DL);
  auto StartB = getConstStartLoc(LocB, DL);

  // Forward the query to the next alias analysis, if we don't have constant
  // start locations.
  if (!StartA || !StartB)
    return AAResultBase::alias(LocA, LocB, AAQI);

  // Extend start locations to the same bitwidth.
  const unsigned MaxBitWidth =
      std::max(StartA->getBitWidth(), StartB->getBitWidth());
  const APInt StartAVal = StartA->zext(MaxBitWidth);
  const APInt StartBVal = StartB->zext(MaxBitWidth);

  // Keys in storage can't overlap.
  if (ASA == EraVMAS::AS_STORAGE) {
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
    return Y.uge(X) && Y.ult(XEnd);
  };

  // For heap accesses, if locations don't overlap, they are not aliasing.
  if (!DoesOverlap(StartAVal, StartAVal + LocA.Size.getValue(), StartBVal) &&
      !DoesOverlap(StartBVal, StartBVal + LocB.Size.getValue(), StartAVal))
    return AliasResult::NoAlias;
  return AliasResult::PartialAlias;
}
