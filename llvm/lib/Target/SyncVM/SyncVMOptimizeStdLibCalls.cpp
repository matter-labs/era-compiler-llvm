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

/// given an integer A and a prime P, return the X such that A*X = 1 (mod P)
[[maybe_unused]] static APInt computeInversModulus(APInt A, APInt P) {
  APInt T = APInt(256, 0, true);
  APInt NewT = APInt(256, 1, true);
  
  APInt R = P.zext(256);
  APInt NewR = A.zext(256);

  while (!NewR.isZero()) {
    APInt Quotient = R.udiv(NewR);
    APInt Tmp = T - Quotient * NewT;
    T = Tmp;

    Tmp = NewR;
    NewR = R - Quotient * NewR;
    R = Tmp;
  }
  if(R.sgt(1))
    return APInt(256, 0); // A is not invertible, return 0
  if (T.slt(0))
      T = T + P; // making sure T ispositive
  return T;
}

[[maybe_unused]]
static APInt computeBarrettReverse(APInt P, uint64_t PrimeBitWidth) {
  APInt R = APInt(512, 1) << PrimeBitWidth;
  P = P.zext(512);
  APInt s;
  APInt aux;

  for (;;) {
    s = R;
    APInt RR = R * R;
    aux = RR.lshr(PrimeBitWidth);
    aux = (aux * P).lshr(PrimeBitWidth); 
    R = (R << 1) - aux;
    if (R.ule(s))
      break;
  }

  APInt t = (APInt(512, 1, true) << (2 * PrimeBitWidth)) - (P * R);

  while (t.slt(0)) {
    R = R - 1;
    t = t + P;
  }

  // R's effective bit width is less than 256, so we can truncate it.
  return R.trunc(256);
}

static bool tryToOptimizeMulModCall(CallBase *Call, const TargetLibraryInfo &TLI) {
  // This pass is executed after constant folding, where it will try to fold
  // mulmod if all arguments are known. So if we happen to see the 3rd argument
  // is a constant, we know it's not folded.
  
  if(Call->arg_size() != 3)
    return false;
  auto *PrimeConstant = dyn_cast<ConstantInt>(Call->getArgOperand(2));
  if (!PrimeConstant)
    return false;
  APInt Prime = PrimeConstant->getValue();
  
  // Cannot handle negative primes
  if (Prime.sle(0))
    return false;

  // TODO: tune this, because 250 is random.
  uint32_t EffectiveBits = Prime.getBitWidth() - Prime.countLeadingZeros();
  if (EffectiveBits > 250)
    return false;

  LLVM_DEBUG(dbgs() << "Found foldable call to `__mulmod`: "; Call->dump(););

  llvm::errs() << Call->getFunction()->getName() << "\n";

  //assert(false && "found call candidate: ");

  StringRef BarrettName = TLI.getName(LibFunc_xvm_mulmod_barrett);
  Function *BarrettFunc = Call->getParent()->getModule()->getFunction(BarrettName);
  assert(BarrettFunc && "__mulmod_barrett must be defined in syncvm-stdlib.ll");

  // prepare extra parameters
  uint64_t PrimeBitWidthInt = Prime.getBitWidth() - Prime.countLeadingZeros();
  APInt Estimate = computeBarrettReverse(Prime, PrimeBitWidthInt);
  dbgs() << "Calculated Estimate: "; Estimate.dump();
  APInt PrimeBitWidth(256, PrimeBitWidthInt);
  std::vector<llvm::Value *> Args{Call->getOperand(0), Call->getOperand(1),
                                  ConstantInt::get(PrimeConstant->getType(), PrimeBitWidth),
                                  ConstantInt::get(PrimeConstant->getType(), Estimate),
                                  PrimeConstant};
  auto IRBuilder = llvm::IRBuilder<>(Call);
  auto NewCall = IRBuilder.CreateCall(BarrettFunc, Args, "replaced_mulmod_call");
  Call->replaceAllUsesWith(NewCall);
  return true;
}

static bool runSyncVMOptimizeStdLibCalls(Function &F,
                                         const TargetLibraryInfo &TLI) {
  std::vector<CallInst *> CallsToErase;
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
    case LibFunc_xvm_mulmod: {
      Changed |= tryToOptimizeMulModCall(Call, TLI);
      if (Changed)
        CallsToErase.push_back(Call);
      break;
    }
    default:
      break;
    }
  }
  for (auto *Call : CallsToErase)
    Call->eraseFromParent();
  return CallsToErase.size() > 0;
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
