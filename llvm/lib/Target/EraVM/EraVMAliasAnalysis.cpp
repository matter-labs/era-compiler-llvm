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

bool EraVMAAWrapperPass::doInitialization(Module &M) {
  SmallDenseSet<unsigned> StorageAS = {EraVMAS::AS_STORAGE,
                                       EraVMAS::AS_TRANSIENT};
  SmallDenseSet<unsigned> HeapAS = {EraVMAS::AS_HEAP, EraVMAS::AS_HEAP_AUX};
  Result = std::make_unique<VMAAResult>(M.getDataLayout(), StorageAS, HeapAS,
                                        EraVMAS::MAX_ADDRESS);
  return false;
}

VMAAResult EraVMAA::run(Function &F, AnalysisManager<Function> &AM) {
  SmallDenseSet<unsigned> StorageAS = {EraVMAS::AS_STORAGE,
                                       EraVMAS::AS_TRANSIENT};
  SmallDenseSet<unsigned> HeapAS = {EraVMAS::AS_HEAP, EraVMAS::AS_HEAP_AUX};
  return VMAAResult(F.getParent()->getDataLayout(), StorageAS, HeapAS,
                    EraVMAS::MAX_ADDRESS);
}
