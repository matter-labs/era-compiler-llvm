//===- SyncVMAlwaysInline.cpp - Add alwaysinline attribute ----------------===//
//===----------------------------------------------------------------------===//
//
// Add alwaysinline attribute to functions with one call site.
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "llvm/IR/Instructions.h"

#define DEBUG_TYPE "syncvm-always-inline"

using namespace llvm;

namespace {

class SyncVMAlwaysInline final : public ModulePass {
public:
  static char ID; // Pass ID
  SyncVMAlwaysInline() : ModulePass(ID) {}

  StringRef getPassName() const override { return "SyncVM always inline"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;
};

} // end anonymous namespace

static bool runImpl(Module &M) {
  bool Changed = false;
  for (auto &F : M) {
    if (F.isDeclaration() || F.hasOptNone() ||
        F.hasFnAttribute(Attribute::NoInline) || !F.hasOneUse())
      continue;

    CallInst *Call = dyn_cast<CallInst>(*F.user_begin());

    // Skip non call instructions, recursive calls, or calls with noinline
    // attribute.
    if (!Call || Call->getFunction() == &F || Call->isNoInline())
      continue;

    F.addFnAttr(Attribute::AlwaysInline);
    Changed = true;
  }

  return Changed;
}

bool SyncVMAlwaysInline::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return runImpl(M);
}

char SyncVMAlwaysInline::ID = 0;

INITIALIZE_PASS(SyncVMAlwaysInline, "syncvm-always-inline",
                "Add alwaysinline attribute to functions with one call site",
                false, false)

ModulePass *llvm::createSyncVMAlwaysInlinePass() {
  return new SyncVMAlwaysInline;
}

PreservedAnalyses SyncVMAlwaysInlinePass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  runImpl(M);
  return PreservedAnalyses::all();
}
