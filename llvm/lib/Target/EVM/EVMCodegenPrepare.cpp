//===------ EVMCodegenPrepare.cpp - EVM CodeGen Prepare ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass replaces general memory transfer intrinsics with
// EVM specific ones, which are custom lowered on ISel. This is required to
// overcome limitations of SelectionDAG::getMemcpy()/getMemmove() that breaks
// an assertion when a memory length is an immediate valued whose bit size is
// more than 64 bits.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsEVM.h"
#include "llvm/Pass.h"

#include "EVM.h"

using namespace llvm;

#define DEBUG_TYPE "evm-codegen-prepare"

namespace llvm {
FunctionPass *createEVMCodegenPrepare();

} // namespace llvm

namespace {
struct EVMCodegenPrepare : public FunctionPass {
public:
  static char ID;
  EVMCodegenPrepare() : FunctionPass(ID) {
    initializeEVMCodegenPreparePass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return "Final transformations before code generation";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
  }

  void processMemTransfer(MemTransferInst *M);
};
} // namespace

char EVMCodegenPrepare::ID = 0;

INITIALIZE_PASS(EVMCodegenPrepare, "evm-codegen-prepare",
                "Final transformations before code generation", false, false)

void EVMCodegenPrepare::processMemTransfer(MemTransferInst *M) {
  // See if the source could be modified by this memmove potentially.
  LLVM_DEBUG(dbgs() << "EVM codegenprepare: Replace:" << *M
                    << " with the target instinsic\n");
  unsigned SrcAS = M->getSourceAddressSpace();
  assert(M->getDestAddressSpace() == EVMAS::AS_HEAP);

  // If the length type is not i256, zext it.
  Value *Len = M->getLength();
  Type *LenTy = Len->getType();
  Type *Int256Ty = Type::getInt256Ty(M->getContext());
  assert(LenTy->getIntegerBitWidth() <= Int256Ty->getIntegerBitWidth());
  if (LenTy != Int256Ty) {
    if (LenTy->getIntegerBitWidth() < Int256Ty->getIntegerBitWidth()) {
      IRBuilder<> Builder(M);
      // We cannot use here M->setLength(), as it checks that new type of
      // 'Length' is the same, so we use a base functionality of Intrinsics.
      // It may look a bit hacky, but should be OK.
      M->setArgOperand(2, Builder.CreateZExt(Len, Int256Ty));
    }
  }

  Intrinsic::ID IntrID = Intrinsic::not_intrinsic;
  switch (SrcAS) {
  default:
    llvm_unreachable("Unexpected source address space of memcpy/memset");
    break;
  case EVMAS::AS_HEAP:
    IntrID = Intrinsic::evm_memmoveas1as1;
    break;
  case EVMAS::AS_CALL_DATA:
    IntrID = Intrinsic::evm_memcpyas1as2;
    break;
  case EVMAS::AS_RETURN_DATA:
    IntrID = Intrinsic::evm_memcpyas1as3;
    break;
  case EVMAS::AS_CODE:
    IntrID = Intrinsic::evm_memcpyas1as4;
    break;
  }
  M->setCalledFunction(Intrinsic::getDeclaration(M->getModule(), IntrID));
}

bool EVMCodegenPrepare::runOnFunction(Function &F) {
  bool Changed = false;
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *M = dyn_cast<MemTransferInst>(&I)) {
        processMemTransfer(M);
        Changed = true;
      }
    }
  }

  return Changed;
}

FunctionPass *llvm::createEVMCodegenPreparePass() {
  return new EVMCodegenPrepare();
}
