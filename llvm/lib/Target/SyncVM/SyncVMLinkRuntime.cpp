//===--- SyncVMLinkRuntime.cpp - inject runtime library into the module ---===//
//
/// \file
/// Implement pass which links runtime (syncvm-runtime.ll), stdlib
/// (syncvm-stdlib.ll) and internalize their contents. SyncVM doesn't have a
/// proper linker and all programs consist of a single module. The pass links
/// the the necessary modules into the program module.
/// It supposed to be called twice:
/// * First, in the beginning of optimization pipeline, the pass links
///   the context of syncvm-stdlib.ll and internalize its content, after that
///   global DCE is expected to be run to remove all unused functions.
/// * Second, it runs before code generation, and it links and internalize
///   syncvm-runtime.ll
//
//============================================================================//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsSyncVM.h"
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
/// Link std and runtime libraries into the module.
/// At the moment front ends work only with single source programs.
struct SyncVMLinkRuntime : public ModulePass {
public:
  static char ID;
  SyncVMLinkRuntime(bool IsRuntimeLinkage = false)
      : ModulePass(ID), IsRuntimeLinkage(IsRuntimeLinkage) { }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "Link runtime library into the module";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    ModulePass::getAnalysisUsage(AU);
  }

private:
  bool IsRuntimeLinkage;
};
} // namespace

char SyncVMLinkRuntime::ID = 0;

INITIALIZE_PASS(SyncVMLinkRuntime, "syncvm-link-runtime",
                "Link standard and runtime library into the module", false,
                false)

const char *RUNTIME_DATA =
#include "SyncVMRT.inc"
    ;
const char *STDLIB_DATA =
#include "SyncVMStdLib.inc"
    ;

static bool SyncVMLinkRuntimeImpl(Module &M, const char *ModuleToLink) {
  Linker L(M);
  LLVMContext &C = M.getContext();
  unsigned Flags = Linker::Flags::None;

  std::unique_ptr<MemoryBuffer> Buffer =
      MemoryBuffer::getMemBuffer(ModuleToLink);
  SMDiagnostic Err;
  std::unique_ptr<Module> RTM = parseIR(*Buffer, Err, C);
  if (!RTM) {
    Err.print("syncvm-runtime.ll, syncvm-stdlib.ll", errs());
    exit(1);
  }
  bool LinkErr = false;
  LinkErr = L.linkInModule(
      std::move(RTM), Flags, [](Module &M, const StringSet<> &GVS) {
        internalizeModule(M, [&GVS](const GlobalValue &GV) {
          // Keep original symbols as they are
          return !GV.hasName() || (GVS.count(GV.getName()) == 0);
        });
      });
  if (LinkErr) {
    errs() << "Can't link SyncVM runtime or stdlib \n";
    exit(1);
  }
  return true;
}

bool SyncVMLinkRuntime::runOnModule(Module &M) {
  bool Changed =
      SyncVMLinkRuntimeImpl(M, IsRuntimeLinkage ? RUNTIME_DATA : STDLIB_DATA);
  if (!IsRuntimeLinkage)
    return Changed;
  // sstore can throw, so externalize invoke sstore by wrapping into a function.
  Function *SStoreF = M.getFunction("__sstore");
  assert(SStoreF && "__sstore must be defined in syncvm-runtime.ll");
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        auto Invoke = dyn_cast<InvokeInst>(&I);
        if (!Invoke)
          continue;
        Intrinsic::ID IntID = Invoke->getIntrinsicID();
        if (IntID != Intrinsic::syncvm_sstore)
          continue;
        Invoke->setCalledFunction(SStoreF);
        Changed = true;
      }
  return Changed;
}

ModulePass *llvm::createSyncVMLinkRuntimePass(bool IsRuntimeLinkage) {
  return new SyncVMLinkRuntime(IsRuntimeLinkage);
}

PreservedAnalyses SyncVMLinkRuntimePass::run(Module &M,
                                             ModuleAnalysisManager &AM) {
  SyncVMLinkRuntimeImpl(M, STDLIB_DATA);
  return PreservedAnalyses::all();
}
