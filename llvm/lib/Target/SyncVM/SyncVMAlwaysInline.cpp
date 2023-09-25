//===- SyncVMAlwaysInline.cpp - Add alwaysinline attribute ----------------===//
//===----------------------------------------------------------------------===//
//
// Add always inline attribute to function with one call site.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/OptimizationLevel.h"

#define DEBUG_TYPE "syncvm-always-inline"

using namespace llvm;

namespace {

class SyncVMAlwaysInline final : public ModulePass {
public:
  static char ID; // Pass ID
  SyncVMAlwaysInline(OptimizationLevel Level = OptimizationLevel::O0)
      : ModulePass(ID), Level(Level) {}

  StringRef getPassName() const override { return "SyncVM call attributes"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
  }

  bool runOnModule(Module &M) override;

private:
  OptimizationLevel Level;
};

} // end anonymous namespace

static bool
runImpl(Module &M, function_ref<const TargetLibraryInfo &(Function &)> GetTLI,
        function_ref<const TargetTransformInfo &(Function &)> GetTTI,
        const OptimizationLevel Level) {
  if (Level == OptimizationLevel::O0)
    return false;

  DenseMap<Function *, unsigned int> FuncRefs;
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;

    const TargetTransformInfo &TTI = GetTTI(F);
    const TargetLibraryInfo &TLI = GetTLI(F);
    for (auto &I : instructions(F)) {
      CallInst *Call = dyn_cast<CallInst>(&I);
      if (!Call)
        continue;

      Function *Callee = Call->getCalledFunction();
      if (!Callee)
        continue;

      if (Callee->isDeclaration() || !TTI.isLoweredToCall(Callee))
        continue;

      LibFunc Func = NotLibFunc;
      const StringRef Name = Callee->getName();
      if (TLI.getLibFunc(Name, Func))
        continue;

      if (Callee->hasFnAttribute(Attribute::NoInline) ||
          Callee->hasFnAttribute(Attribute::OptimizeNone))
        continue;

      FuncRefs[Callee]++;
    }
  }

  bool Changed = false;
  for (auto [Func, Refs] : FuncRefs) {
    if (Refs != 1)
      continue;

    Func->addFnAttr(Attribute::AlwaysInline);
    Changed = true;
  }
  return Changed;
}

bool SyncVMAlwaysInline::runOnModule(Module &M) {
  if (skipModule(M))
    return false;

  auto GetTLI = [this](Function &F) -> const TargetLibraryInfo & {
    return this->getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  };
  auto GetTTI = [this](Function &F) -> const TargetTransformInfo & {
    return this->getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
  };
  return runImpl(M, GetTLI, GetTTI, Level);
}

char SyncVMAlwaysInline::ID = 0;

INITIALIZE_PASS_BEGIN(
    SyncVMAlwaysInline, "syncvm-always-inline",
    "Add always inline attribute to functions with one call site", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_END(
    SyncVMAlwaysInline, "syncvm-always-inline",
    "Add always inline attribute to functions with one call site", false, false)

ModulePass *llvm::createSyncVMAlwaysInlinePass(OptimizationLevel Level) {
  return new SyncVMAlwaysInline(Level);
}

PreservedAnalyses SyncVMAlwaysInlinePass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  FunctionAnalysisManager &FAM =
      AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  auto GetTLI = [&FAM](Function &F) -> const TargetLibraryInfo & {
    return FAM.getResult<TargetLibraryAnalysis>(F);
  };
  auto GetTTI = [&FAM](Function &F) -> const TargetTransformInfo & {
    return FAM.getResult<TargetIRAnalysis>(F);
  };
  runImpl(M, GetTLI, GetTTI, Level);
  return PreservedAnalyses::all();
}
