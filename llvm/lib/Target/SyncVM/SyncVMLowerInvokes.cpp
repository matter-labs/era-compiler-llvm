//=== SyncVMLowerInvokes.cpp - Lower Invokes to calls and checks =//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file is copied and modified from WebAssembly/WebAssemblyLowerEmscriptenEHSjLj.cpp
/// The pass is tailored because we don't need:
/// * long jumps and set jumps because SyncVM does not have such capability.
/// The file descriptions are kept.
/// 
/// This file lowers exception-related instructions (`InvokeInst`'s) to SyncVM-compatible
/// handling mechanism.
///
/// First, we introduce a `__THREW__` global variable which serves as an indicator
/// of exception status. Should the function call `__cxa_throw` to throw a runtime
/// exception, instead the program sets `__THREW__` to be 1 and return via normal path.
///
/// Second, an `InvokeInst` is lowered to `CallInst` followed by a conditional branch. 
/// The predicate in the branch checks if the `__THREW__` value has been changed. If so
/// we will redirect our code to unwind BB label.
///
///===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "SyncVMTargetMachine.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "syncvm-lower-invokes"

namespace {
class SyncVMLowerInvokes final : public ModulePass {
  GlobalVariable *ThrewGV = nullptr;

  StringRef getPassName() const override {
    return "SyncVM Lower Invokes";
  }

  bool runEHOnFunction(Function &F);

public:
  static char ID;

  SyncVMLowerInvokes()
      : ModulePass(ID) {
    
  }
  bool runOnModule(Module &M) override;
};
} // End anonymous namespace

char SyncVMLowerInvokes::ID = 0;
INITIALIZE_PASS(SyncVMLowerInvokes, DEBUG_TYPE,
                "SyncVM Lower invokes",
                false, false)

ModulePass *llvm::createSyncVMLowerInvokesPass() {
  return new SyncVMLowerInvokes();
}

// Get a global variable with the given name. If it doesn't exist declare it,
// which will generate an import and assume that it will exist at link time.
static GlobalVariable *getGlobalVariable(Module &M, Type *Ty,
                                         SyncVMTargetMachine &TM,
                                         const char *Name) {
  Constant* GVConstant = M.getOrInsertGlobal(Name, Ty);
  GlobalVariable *GV = dyn_cast<GlobalVariable>(GVConstant);
  if (!GV)
    report_fatal_error(Twine("unable to create global: ") + Name);

  // we are not going to be multi-threaded so not going to use atomic
  /*
  // If the target supports TLS, make this variable thread-local. We can't just
  // unconditionally make it thread-local and depend on
  // CoalesceFeaturesAndStripAtomics to downgrade it, because stripping TLS has
  // the side effect of disallowing the object from being linked into a
  // shared-memory module, which we don't want to be responsible for.
  auto *Subtarget = TM.getSubtargetImpl();
  auto TLS = Subtarget->hasAtomics() && Subtarget->hasBulkMemory()
                 ? GlobalValue::LocalExecTLSModel
                 : GlobalValue::NotThreadLocal;
  GV->setThreadLocalMode(TLS);
  */
  return GV;
}

static Type *getAddrIntType(Module *M) {
  IRBuilder<> IRB(M->getContext());
  return IRB.getIntNTy(M->getDataLayout().getPointerSizeInBits());
}

static Value *getAddrSizeInt(Module *M, uint64_t C) {
  IRBuilder<> IRB(M->getContext());
  return IRB.getIntN(M->getDataLayout().getPointerSizeInBits(), C);
}

bool SyncVMLowerInvokes::runOnModule(Module &M) {
  LLVM_DEBUG(dbgs() << "********** Lower Emscripten EH & SjLj **********\n");

  LLVMContext &C = M.getContext();
  IRBuilder<> IRB(C);


  auto *TPC = getAnalysisIfAvailable<TargetPassConfig>();
  assert(TPC && "Expected a TargetPassConfig");
  auto &TM = TPC->getTM<SyncVMTargetMachine>();

  // Declare (or get) global variables __THREW__
  // which are used in common for both
  // exception handling and setjmp/longjmp handling
  ThrewGV = getGlobalVariable(M, getAddrIntType(&M), TM, "__THREW__");

  Constant* zero = IRB.getInt(APInt(256, 0));
  ThrewGV->setInitializer(zero);

  bool Changed = false;

  for (Function & F : M) {
    if (F.isDeclaration()) continue;
    Changed |= runEHOnFunction(F);
  }

  return true;
}

static bool isThrowFunction(const Function* Callee) {
  return Callee->getName() == "__cxa_throw"; 
}

bool SyncVMLowerInvokes::runEHOnFunction(Function &F) {
  Module &M = *F.getParent();
  LLVMContext &C = F.getContext();
  IRBuilder<> IRB(C);
  bool Changed = false;
  SmallVector<Instruction *, 64> ToErase;
  SmallPtrSet<LandingPadInst *, 32> LandingPads;

  // This loop will replace InvokeInsts and emit:
  // %__THREW__ = 0
  // target(args) ; call target 
  // brcond (__THREW__ == 1) %lpad, %normal
  // ...

  for (BasicBlock &BB : F) {
    auto *II = dyn_cast<InvokeInst>(BB.getTerminator());
    if (!II)
      continue;
    Changed = true;

    ToErase.push_back(II);

    IRB.SetInsertPoint(II);

    // if we are invoking __cxa_throw, we will generate a different pattern:
    // invoke __cxa_throw 
    // turn this function to simply:
    // %__THREW__ = 1
    // br label %catch
    // we know normal path is actually unreachable
    {
      const Function *Callee = II->getCalledFunction();
      assert(Callee && "InvokeInst must have a callee function");
      if (isThrowFunction(Callee)) {
        IRB.CreateStore(getAddrSizeInt(&M, 1), ThrewGV);
        IRB.CreateBr(II->getUnwindDest());
        continue;
      }
    }

    LandingPads.insert(II->getLandingPadInst());

    // change InvokeInst to CallInst
    // TODO: Call should be before Invoke because Invoke is a teminator
    std::vector<Value*> operands(II->arg_begin(), II->arg_end());
    IRB.CreateCall(II->getFunctionType(), II->getCalledOperand(), operands);

    // emit
    Value *Threw =
      IRB.CreateLoad(getAddrIntType(&M), ThrewGV, ThrewGV->getName() + ".val");
    Value *CmpEqOne =
        IRB.CreateICmpEQ(Threw, getAddrSizeInt(&M, 0), "cmp.threw_value");
    IRB.CreateCondBr(CmpEqOne, II->getNormalDest(), II->getUnwindDest());

  }

  // This loop will find all `__cxa_throw` calls and replace with:
  // __THREW__ = 1
  // ret
  for (BasicBlock &BB : F) {
    for (auto & I : BB) {
      auto *CI = dyn_cast<CallInst>(&I);
      if (!CI)
        continue;
      Changed = true;

      if (!isThrowFunction(CI->getCalledFunction())) {
        continue;
      } 

      // remove the subsequent unreachable inst.
      Instruction *nextInst = CI->getNextNode();
      UnreachableInst* unreachable = dyn_cast<UnreachableInst>(nextInst);
      assert(unreachable);
      ToErase.push_back(unreachable);

      IRB.SetInsertPoint(CI);

      IRB.CreateStore(getAddrSizeInt(&M, 1), ThrewGV);

      // find the function's return type
      Type * retty = F.getReturnType();

      // TODO: add more things to return value
      assert(retty == Type::getVoidTy(C));

      IRB.CreateRet(nullptr);

      ToErase.push_back(CI);
    }
  }

  // convert LandingPads into reset `__THREW__` = 0
  for (LandingPadInst * LPI : LandingPads) {
    IRB.SetInsertPoint(LPI);
    IRB.CreateStore(getAddrSizeInt(&M, 0), ThrewGV);
    LPI->eraseFromParent();
  }

  
  // delete landing pads because 

  // Erase everything we no longer need in this function
  for (Instruction *I : ToErase)
    I->eraseFromParent();

  return Changed;
}

