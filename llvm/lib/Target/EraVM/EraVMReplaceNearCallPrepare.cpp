//===-- EraVMReplaceNearCallPrepare.cpp - Call by Jump ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass will check functions in current unit to see whether:
//   * a function can use jump to return
//   * a function has a near_call which can be replaced with jump
//
// If a function is called via INVOKE then it can't use jump to return.
// If a callee of a near_call can't use jump to return, then current near_call
// can't be replaced with jump.
//
// The final results will be added to function as attributes for being used in
// subsequent passes:
//   * UseJumpReturn: current function hasn't been called via INVOKE at all in
//                    current unit and thus can return via jump.
//   * HasJumpCall: current function has at least one near_call which can be
//                  replaced with jump, the later frameLowering should reserve
//                  the top of stack for storing return address.
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMMachineFunctionInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "eravm-replace-near-call-prepare"

using namespace llvm;

#define ERAVM_REPLACE_NEAR_CALL_PREPARE_NAME                                   \
  "EraVM replace near_call prepare pass"

namespace {

class EraVMReplaceNearCallPrepare final : public ModulePass {
public:
  static char ID; // Pass ID
  EraVMReplaceNearCallPrepare() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return ERAVM_REPLACE_NEAR_CALL_PREPARE_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

static bool runImpl(Module &M, MachineModuleInfo *MMI) {
  bool Changed = false;

  // Check whether current function is called via INVOKE, if none, then
  // it can use jump to return.
  for (auto &F : M) {
    if (F.empty() || F.isDeclaration() || F.getName() == "__entry" ||
        F.getName() == "__cxa_throw")
      continue;

    if (any_of(F.uses(),
               [](const Use &U) { return isa<InvokeInst>(U.getUser()); }))
      continue;

    if (any_of(F.uses(), [MMI](const Use &U) {
          auto &CallerFunc = *cast<Instruction>(U.getUser())->getFunction();
          auto *CallerMF = MMI->getMachineFunction(CallerFunc);
          return !CallerMF->getFrameInfo().getNumObjects();
        }))
      continue;

    F.addFnAttr("UseJumpReturn");
    Changed = true;
  }

  // Check all the CallInst, if the callee can use jump to return,
  // then the function including CallInst has a near_call which
  // can be replaced with jump, add HasJumpCall attribute for being
  // used by later frameLowering pass to reserve the top of the stack
  // for storing the return address.
  for (auto &F : M) {
    if (!F.hasFnAttribute("UseJumpReturn"))
      continue;

    for (auto I = F.user_begin(), E = F.user_end(); I != E; I++) {
      if (auto *II = dyn_cast<CallInst>(*I)) {
        Function *Caller = II->getParent()->getParent();
        Caller->addFnAttr("HasJumpCall");
        Changed = true;
      }
    }
  }

  return Changed;
}

bool EraVMReplaceNearCallPrepare::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  MachineModuleInfo *MMI =
      &getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

  return runImpl(M, MMI);
}

char EraVMReplaceNearCallPrepare::ID = 0;

INITIALIZE_PASS(EraVMReplaceNearCallPrepare, "eravm-replace-near-call-prepare",
                ERAVM_REPLACE_NEAR_CALL_PREPARE_NAME, false,
                false)

ModulePass *llvm::createEraVMReplaceNearCallPreparePass() {
  return new EraVMReplaceNearCallPrepare;
}
