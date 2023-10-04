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

static Optional<APInt> getConstStartLoc(const MemoryLocation &Loc) {
  // TODO: CPR-1337 Enable all pointer sizes.
  constexpr unsigned ByteWidth = 32;
  if (Loc.Size != ByteWidth)
    return None;

  if (const auto *CPN = dyn_cast<ConstantPointerNull>(Loc.Ptr))
    return APInt::getZero(ByteWidth * 8);

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

  auto StartA = getConstStartLoc(LocA);
  auto StartB = getConstStartLoc(LocB);

  // Forward the query to the next alias analysis, if we don't have constant
  // start locations.
  if (!StartA || !StartB)
    return AAResultBase::alias(LocA, LocB, AAQI);

  // TODO: CPR-1337 Take into account pointer size.
  // If locations are the same, they must alias.
  if (*StartA == *StartB)
    return AliasResult::MustAlias;

  // For storage accesses, we know that locations are pointing to the different
  // keys, so they are not aliasing.
  if (ASA == EraVMAS::AS_STORAGE)
    return AliasResult::NoAlias;

  auto DoesOverlap = [](const APInt &X, const APInt &XEnd, const APInt &Y) {
    return Y.uge(X) && Y.ult(XEnd);
  };

  // For heap accesses, if locations don't overlap, they are not aliasing.
  if (!DoesOverlap(*StartA, *StartA + LocA.Size.getValue(), *StartB) &&
      !DoesOverlap(*StartB, *StartB + LocB.Size.getValue(), *StartA))
    return AliasResult::NoAlias;
  return AliasResult::PartialAlias;
}
