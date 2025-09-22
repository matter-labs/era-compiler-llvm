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
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsEVM.h"
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

EVMAAResult::EVMAAResult(const DataLayout &DL)
    : VMAAResult(DL, {EVMAS::AS_STORAGE, EVMAS::AS_TSTORAGE}, {EVMAS::AS_HEAP},
                 EVMAS::MAX_ADDRESS) {}

ModRefInfo EVMAAResult::getArgModRefInfo(const CallBase *Call,
                                         unsigned ArgIdx) {
  if (Call->doesNotAccessMemory(ArgIdx))
    return ModRefInfo::NoModRef;

  if (Call->onlyWritesMemory(ArgIdx))
    return ModRefInfo::Mod;

  if (Call->onlyReadsMemory(ArgIdx))
    return ModRefInfo::Ref;

  return ModRefInfo::ModRef;
}

static MemoryLocation getMemLocForArgument(const CallBase *Call,
                                           unsigned ArgIdx) {
  AAMDNodes AATags = Call->getAAMetadata();
  const Value *Arg = Call->getArgOperand(ArgIdx);
  const auto *II = cast<IntrinsicInst>(Call);

  auto GetMemLocation = [Call, Arg, &AATags](unsigned MemSizeArgIdx) {
    const auto *LenCI =
        dyn_cast<ConstantInt>(Call->getArgOperand(MemSizeArgIdx));
    if (LenCI && LenCI->getValue().getActiveBits() <= 64)
      return MemoryLocation(Arg, LocationSize::precise(LenCI->getZExtValue()),
                            AATags);
    return MemoryLocation::getAfter(Arg, AATags);
  };

  switch (II->getIntrinsicID()) {
  case Intrinsic::evm_return: {
    assert((ArgIdx == 0) && "Invalid argument index for return");
    return GetMemLocation(ArgIdx + 1);
  }
  case Intrinsic::evm_create:
  case Intrinsic::evm_create2: {
    assert((ArgIdx == 1) && "Invalid argument index for create/create2");
    return GetMemLocation(ArgIdx + 1);
  }
  case Intrinsic::evm_call:
  case Intrinsic::evm_callcode: {
    assert((ArgIdx == 3 || ArgIdx == 5) &&
           "Invalid argument index for call/callcode");
    return GetMemLocation(ArgIdx + 1);
  }
  case Intrinsic::evm_delegatecall:
  case Intrinsic::evm_staticcall: {
    assert((ArgIdx == 2 || ArgIdx == 4) &&
           "Invalid argument index for delegatecall/staticcall");
    return GetMemLocation(ArgIdx + 1);
  }
  default:
    llvm_unreachable("Unexpected intrinsic for EVM/EVM target");
    break;
  }
}

ModRefInfo EVMAAResult::getModRefInfo(const CallBase *Call,
                                      const MemoryLocation &Loc,
                                      AAQueryInfo &AAQI) {
  const auto *II = dyn_cast<IntrinsicInst>(Call);
  if (!II)
    return AAResultBase::getModRefInfo(Call, Loc, AAQI);

  unsigned AS = Loc.Ptr->getType()->getPointerAddressSpace();
  switch (II->getIntrinsicID()) {
  case Intrinsic::evm_return:
  case Intrinsic::evm_staticcall:
    if (AS == EVMAS::AS_STORAGE || AS == EVMAS::AS_TSTORAGE)
      return ModRefInfo::Ref;
    break;
  case Intrinsic::evm_create:
  case Intrinsic::evm_create2:
  case Intrinsic::evm_call:
  case Intrinsic::evm_callcode:
  case Intrinsic::evm_delegatecall:
    if (AS == EVMAS::AS_STORAGE || AS == EVMAS::AS_TSTORAGE)
      return ModRefInfo::ModRef;
    break;
  default:
    return AAResultBase::getModRefInfo(Call, Loc, AAQI);
  }

  ModRefInfo Result = ModRefInfo::NoModRef;
  for (const auto &I : llvm::enumerate(Call->args())) {
    const Value *Arg = I.value();
    if (!Arg->getType()->isPointerTy())
      continue;
    unsigned ArgIdx = I.index();
    MemoryLocation ArgLoc = getMemLocForArgument(Call, ArgIdx);
    AliasResult ArgAlias = VMAAResult::alias(ArgLoc, Loc, AAQI, Call);
    if (ArgAlias != AliasResult::NoAlias)
      Result |= getArgModRefInfo(Call, ArgIdx);
  }

  return Result;
}

EVMAAWrapperPass::EVMAAWrapperPass() : ImmutablePass(ID) {
  initializeEVMAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

void EVMAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool EVMAAWrapperPass::doInitialization(Module &M) {
  Result = std::make_unique<EVMAAResult>(M.getDataLayout());
  return false;
}

EVMAAResult EVMAA::run(Function &F, AnalysisManager<Function> &AM) {
  return EVMAAResult(F.getParent()->getDataLayout());
}
