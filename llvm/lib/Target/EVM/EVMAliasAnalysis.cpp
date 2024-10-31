//===-- EVMAliasAnalysis.cpp - EVM alias analysis ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the EVM address space based alias analysis pass.
//
//===----------------------------------------------------------------------===//

#include "EVMAliasAnalysis.h"
#include "EVM.h"
#include "llvm/IR/Module.h"

using namespace llvm;

#define DEBUG_TYPE "evm-aa"

AnalysisKey EVMAA::Key;

// Register this pass...
char EVMAAWrapperPass::ID = 0;
char EVMExternalAAWrapper::ID = 0;

INITIALIZE_PASS(EVMAAWrapperPass, "evm-aa",
                "EVM Address space based Alias Analysis", false, true)

INITIALIZE_PASS(EVMExternalAAWrapper, "evm-aa-wrapper",
                "EVM Address space based Alias Analysis Wrapper", false, true)

ImmutablePass *llvm::createEVMAAWrapperPass() { return new EVMAAWrapperPass(); }

ImmutablePass *llvm::createEVMExternalAAWrapperPass() {
  return new EVMExternalAAWrapper();
}

EVMAAWrapperPass::EVMAAWrapperPass() : ImmutablePass(ID) {
  initializeEVMAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

void EVMAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool EVMAAWrapperPass::doInitialization(Module &M) {
  SmallDenseSet<unsigned> StorageAS = {EVMAS::AS_STORAGE, EVMAS::AS_TSTORAGE};
  SmallDenseSet<unsigned> HeapAS = {EVMAS::AS_HEAP};
  Result = std::make_unique<VMAAResult>(M.getDataLayout(), StorageAS, HeapAS,
                                        EVMAS::MAX_ADDRESS);
  return false;
}

VMAAResult EVMAA::run(Function &F, AnalysisManager<Function> &AM) {
  SmallDenseSet<unsigned> StorageAS = {EVMAS::AS_STORAGE, EVMAS::AS_TSTORAGE};
  SmallDenseSet<unsigned> HeapAS = {EVMAS::AS_HEAP};
  return VMAAResult(F.getParent()->getDataLayout(), StorageAS, HeapAS,
                    EVMAS::MAX_ADDRESS);
}
