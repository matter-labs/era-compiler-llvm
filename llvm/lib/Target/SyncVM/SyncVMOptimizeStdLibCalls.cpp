//===- SyncVMOptimizeStdLibCalls.cpp - Optimize stdlib calls--  -*- C++ -*-===//
//===----------------------------------------------------------------------===//
//
// This pass tries to replace an stdlib call with either a call to the more
// efficient algorithm, or just an optimized sequence of instructions.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#define DEBUG_TYPE "syncvm-optimize-stdlib-calls"

using namespace llvm;

namespace {

class SyncVMOptimizeStdLibCalls final : public FunctionPass {
private:
  StringRef getPassName() const override {
    return "SyncVM optimize stdlib calls";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    FunctionPass::getAnalysisUsage(AU);
  }

  bool runOnFunction(Function &function) override;

public:
  static char ID; // Pass ID

  SyncVMOptimizeStdLibCalls() : FunctionPass(ID) {}
};

} // end anonymous namespace

static bool hasUndefOrPoision(CallBase *Call) {
  for (const auto &Op : Call->operands()) {
    if (isa<PoisonValue>(Op) || isa<UndefValue>(Op))
      return true;
  }
  return false;
}

/// Convert __exp(base, exp) call where 'base' is a power of 2 into
/// the __shl(log2(base) * exp, 1).
static bool tryToOptimizeExpCall(CallBase *Call, const TargetLibraryInfo &TLI) {
  if (hasUndefOrPoision(Call)) {
    Call->replaceAllUsesWith(PoisonValue::get(Call->getType()));
    LLVM_DEBUG(dbgs() << "replacing with : poison" << '\n');
    return true;
  }

  auto *CI = dyn_cast<ConstantInt>(Call->getOperand(0));
  if (!CI)
    return false;

  int LogBase = CI->getValue().exactLogBase2();
  if (LogBase <= 0)
    return false;

  StringRef ShlName = TLI.getName(LibFunc_xvm_shl);
  Function *ShlF = Call->getParent()->getModule()->getFunction(ShlName);
  assert(ShlF && "__shl must be defined in syncvm-stdlib.ll");

  Call->setCalledFunction(ShlF);
  const auto &Exp = Call->getOperand(1);
  IRBuilder<> IRB(Call);
  auto NewExp = IRB.CreateMul(Exp, ConstantInt::get(Call->getType(), LogBase),
                              "new_exp", true, true);
  Call->setOperand(0, NewExp);
  Call->setOperand(1, ConstantInt::get(Call->getType(), 1));

  LLVM_DEBUG(dbgs() << "replacing with : " << *NewExp << '\n'
                    << "                 " << *Call << '\n');
  return true;
}

static bool runSyncVMOptimizeStdLibCalls(Function &F,
                                         const TargetLibraryInfo &TLI) {
  bool Changed = false;
  for (auto &I : instructions(F)) {
    CallInst *Call = dyn_cast<CallInst>(&I);
    if (!Call)
      continue;

    Function *Callee = Call->getCalledFunction();
    if (!Callee)
      continue;

    LibFunc Func = NotLibFunc;
    StringRef Name = Callee->getName();
    if (!TLI.getLibFunc(Name, Func) || !TLI.has(Func))
      continue;

    LLVM_DEBUG(dbgs() << "Trying to optimize : " << *Call << '\n');

    switch (Func) {
    case LibFunc_xvm_exp:
      Changed |= tryToOptimizeExpCall(Call, TLI);
      break;
    default:
      break;
    }
  }

  return Changed;
}

bool SyncVMOptimizeStdLibCalls::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  const TargetLibraryInfo &TLI =
      getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);

  return runSyncVMOptimizeStdLibCalls(F, TLI);
}

char SyncVMOptimizeStdLibCalls::ID = 0;

INITIALIZE_PASS(
    SyncVMOptimizeStdLibCalls, "syncvm-optimize-stdlib-calls",
    "Replace stdlib call with a more optimized sequence of instructions", false,
    false)

FunctionPass *llvm::createSyncVMOptimizeStdLibCallsPass() {
  return new SyncVMOptimizeStdLibCalls;
}

PreservedAnalyses
SyncVMOptimizeStdLibCallsPass::run(Function &F, FunctionAnalysisManager &AM) {
  const TargetLibraryInfo &TLI = AM.getResult<TargetLibraryAnalysis>(F);

  return runSyncVMOptimizeStdLibCalls(F, TLI) ? PreservedAnalyses::none()
                                              : PreservedAnalyses::all();
}
