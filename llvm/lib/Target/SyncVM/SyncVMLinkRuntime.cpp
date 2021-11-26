//===--- SyncVMLinkRuntime.cpp - inject runtime library into the module ---===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/Scalar.h"

#include <memory>

#include "SyncVM.h"

using namespace llvm;

namespace llvm {
ModulePass *createSyncVMLinkRuntimePass();
void initializeSyncVMLinkRuntimePass(PassRegistry &);
} // namespace llvm

static ExitOnError ExitOnErr;

namespace {
/// Link the runtime library into the module.
/// At the moment front ends work only with single source programs.
struct SyncVMLinkRuntime : public ModulePass {
public:
  static char ID;
  SyncVMLinkRuntime() : ModulePass(ID) {
    initializeSyncVMIndirectExternalCallPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Link runtime library into the module";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
};
} // namespace

char SyncVMLinkRuntime::ID = 0;

INITIALIZE_PASS(SyncVMLinkRuntime, "syncvm-link-runtime",
                "Link runtime library into the module", false, false)

const char *LL_DATA =
#include "SyncVMRT.inc"
    ;

bool SyncVMLinkRuntime::runOnModule(Module &M) {
  Linker L(M);
  LLVMContext &C = M.getContext();
  unsigned Flags = Linker::Flags::None;

  std::unique_ptr<MemoryBuffer> Buffer = MemoryBuffer::getMemBuffer(LL_DATA);
  SMDiagnostic Err;
  std::unique_ptr<Module> RTM = parseIR(*Buffer, Err, C);
  if (!RTM) {
    Err.print("syncvm-runtime.ll", errs());
    exit(1);
  }
  bool LinkErr = false;
  LinkErr = L.linkInModule(
      std::move(RTM), Flags, [](Module &M, const StringSet<> &GVS) {
        internalizeModule(M, [&GVS](const GlobalValue &GV) {
          return !GV.hasName() || (GVS.count(GV.getName()) == 0);
        });
      });
  if (LinkErr) {
    errs() << "Can't link SyncVM runtime \n";
    exit(1);
  }
  return true;
}

ModulePass *llvm::createSyncVMLinkRuntimePass() {
  return new SyncVMLinkRuntime();
}
