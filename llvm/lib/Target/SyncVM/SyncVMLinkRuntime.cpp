//===--- SyncVMLinkRuntime.cpp - inject runtime library into the module ---===//
//
/// \file
/// Implement pass which links runtime (syncvm-runtime.ll).
/// SyncVM doesn't have a proper linker and all programs consist of a single
/// module. The pass links the runtime into the program module.
/// It supposed to be called twice:
/// * First, in the beginning of optimization pipeline, the pass links
///   the context of syncvm-runtime.ll and internalize functions that marked
///   with internalize-early attribute.
/// * Second, it runs before code generation, and it internalize the rest of
///   the runtime which is marked with internalize-late. It also wraps invoke
///   sstore intrinsic inro a runtime call.
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
/// The attribute to internalize a runtime function in the beginning of opt
/// pipeline. Functions that are used directly by a frontend are generally
/// marked with it.
static const char INTERNALIZE_EARLY[] = "internalize-early";
/// The attribute to internalize a runtime function in the beginning of llc
/// pipeline. LLVM instructions and intrinsics that are lowered to a runtime
/// call are marked with it.
static const char INTERNALIZE_LATE[] = "internalize-late";

namespace {
/// Link the runtime library into the module.
/// At the moment front ends work only with single source programs.
struct SyncVMLinkRuntime : public ModulePass {
public:
  static char ID;
  SyncVMLinkRuntime(bool OnlyInternalize = false)
      : ModulePass(ID), OnlyInternalize(OnlyInternalize) {
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
  bool OnlyInternalize;
};
} // namespace

char SyncVMLinkRuntime::ID = 0;

INITIALIZE_PASS(SyncVMLinkRuntime, "syncvm-link-runtime",
                "Link runtime library into the module", false, false)

const char *LL_DATA =
#include "SyncVMRT.inc"
    ;

static bool SyncVMLinkRuntimeImpl(Module &M) {
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
          // Keep original symbols as they are
          if (!GV.hasName() || (GVS.count(GV.getName()) == 0))
            return true;
          // Internalize linked-in symbols with "internalize-early" attribute.
          if (auto *F = llvm::dyn_cast<Function>(&GV))
            return !F->hasFnAttribute(INTERNALIZE_EARLY);
          return false;
        });
      });
  if (LinkErr) {
    errs() << "Can't link SyncVM runtime \n";
    exit(1);
  }
  return true;
}

// TODO: CPR-988 Replace with __sstore runtime function.
static Function *createSStoreFunction(Module &M) {
  LLVMContext &C = M.getContext();
  Type *Int256Ty = Type::getInt256Ty(C);
  std::vector<Type *> ArgTy = {Int256Ty, Int256Ty};
  M.getOrInsertFunction(
      "__sstore",
      FunctionType::get(Type::getVoidTy(C), {Int256Ty, Int256Ty}, false));
  Function *Result = M.getFunction("__sstore");
  auto *Entry = BasicBlock::Create(C, "entry", Result);
  auto *SStoreIntFn = Intrinsic::getDeclaration(&M, Intrinsic::syncvm_sstore);

  IRBuilder<> Builder(Entry);
  Builder.CreateCall(SStoreIntFn, {Result->getArg(0), Result->getArg(1)});
  Builder.CreateRetVoid();
  return Result;
}

bool SyncVMLinkRuntime::runOnModule(Module &M) {
  if (!OnlyInternalize)
    return SyncVMLinkRuntimeImpl(M);
  bool Changed = internalizeModule(M, [](const GlobalValue &GV) {
    // Only keep entry public
    if (!GV.hasName())
      return true;
    // Internalize linked-in symbols with "internalize-late" attribute.
    if (auto *F = llvm::dyn_cast<Function>(&GV))
      return !F->hasFnAttribute(INTERNALIZE_LATE);
    return false;
  });
  // sstore can throw, so externalize invoke sstore by wrapping into a function.
  Function *SStoreF = nullptr;
  for (auto &F : M)
    for (auto &BB : F)
      for (auto &I : BB) {
        auto Invoke = dyn_cast<InvokeInst>(&I);
        if (!Invoke)
          continue;
        Intrinsic::ID IntID = Invoke->getIntrinsicID();
        if (IntID != Intrinsic::syncvm_sstore)
          continue;
        if (!SStoreF)
          SStoreF = createSStoreFunction(M);
        Invoke->setCalledFunction(SStoreF);
        Changed = true;
      }
  return Changed;
}

ModulePass *llvm::createSyncVMLinkRuntimePass(bool OnlyInternalize) {
  return new SyncVMLinkRuntime(OnlyInternalize);
}

PreservedAnalyses SyncVMLinkRuntimePass::run(Module &M,
                                             ModuleAnalysisManager &AM) {
  SyncVMLinkRuntimeImpl(M);
  return PreservedAnalyses::all();
}
