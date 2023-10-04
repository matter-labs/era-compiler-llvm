//===--- EraVMLinkRuntime.cpp - inject runtime library into the module ---===//
//
/// \file
/// Implement pass which links runtime (eravm-runtime.ll), stdlib
/// (eravm-stdlib.ll) and internalize their contents. EraVM doesn't have a
/// proper linker and all programs consist of a single module. The pass links
/// the the necessary modules into the program module.
/// It supposed to be called twice:
/// * First, in the beginning of optimization pipeline, the pass links
///   the context of eravm-stdlib.ll and internalize its content, after that
///   global DCE is expected to be run to remove all unused functions.
/// * Second, it runs before code generation, and it links and internalize
///   eravm-runtime.ll
//
//============================================================================//

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsEraVM.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/Scalar.h"

#include <memory>

#include "EraVM.h"

using namespace llvm;

namespace llvm {
ModulePass *createEraVMLinkRuntimePass();
void initializeEraVMLinkRuntimePass(PassRegistry & /*Registry*/);
} // namespace llvm

static ExitOnError ExitOnErr;

namespace {
/// Link std and runtime libraries into the module.
/// At the moment front ends work only with single source programs.
struct EraVMLinkRuntime : public ModulePass {
public:
  static char ID;
  EraVMLinkRuntime(bool IsRuntimeLinkage = false)
      : ModulePass(ID), IsRuntimeLinkage(IsRuntimeLinkage) {}
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

char EraVMLinkRuntime::ID = 0;

INITIALIZE_PASS(EraVMLinkRuntime, "eravm-link-runtime",
                "Link standard and runtime library into the module", false,
                false)

const char *RUNTIME_DATA =
#include "EraVMRT.inc"
    ;
const char *STDLIB_DATA =
#include "EraVMStdLib.inc"
    ;

namespace {
bool EraVMLinkRuntimeImpl(Module &M, const char *ModuleToLink,
                          FunctionAnalysisManager *FAM = nullptr,
                          bool OptimizeForSize = false) {
  Linker L(M);
  LLVMContext &C = M.getContext();
  unsigned Flags = Linker::Flags::None;

  std::unique_ptr<MemoryBuffer> Buffer =
      MemoryBuffer::getMemBuffer(ModuleToLink);
  SMDiagnostic Err;
  std::unique_ptr<Module> RTM = parseIR(*Buffer, Err, C);
  if (!RTM) {
    Err.print("Unable to parse eravm-runtime.ll, eravm-stdlib.ll", errs());
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
    errs() << "Can't link EraVM runtime or stdlib \n";
    exit(1);
  }

  if (OptimizeForSize) {
    // 1. Add 'noinline' attribute to the call sites of the stdlib functions
    // that have 'noinline-oz' attribute. This attribute was manually
    // added to some stdlib functions based on test results.
    for (auto &F : M) {
      for (auto &I : instructions(F)) {
        CallInst *Call = dyn_cast<CallInst>(&I);
        if (!Call)
          continue;

        Function *Callee = Call->getCalledFunction();
        if (!Callee)
          continue;

        // If arguments are constants, following functions are simplified to
        // one intrinsic call, so it's worth to enable their inlining.
        LibFunc Func = NotLibFunc;
        const TargetLibraryInfo &TLI = FAM->getResult<TargetLibraryAnalysis>(F);
        const StringRef Name = Callee->getName();
        if (TLI.getLibFunc(Name, Func) && TLI.has(Func) &&
            (Func == LibFunc_xvm_revert || Func == LibFunc_xvm_return)) {
          if (Call->arg_empty() ||
              std::all_of(Call->arg_begin(), Call->arg_end(),
                          [](const auto &Arg) { return isa<Constant>(Arg); }))
            continue;
        }
        if (Callee->hasFnAttribute("noinline-oz"))
          Call->addFnAttr(Attribute::NoInline);
      }
    }
    // 2. Add 'minsize', 'optsize' attributes to all the function definitions.
    for (auto &F : M.functions()) {
      if (!F.isDeclaration()) {
        F.addFnAttr(Attribute::MinSize);
        F.addFnAttr(Attribute::OptimizeForSize);
      }
    }
  }

  return true;
}

} // end of anonymous namespace

bool EraVMLinkRuntime::runOnModule(Module &M) {
  bool Changed =
      EraVMLinkRuntimeImpl(M, IsRuntimeLinkage ? RUNTIME_DATA : STDLIB_DATA);
  return Changed;
}

ModulePass *llvm::createEraVMLinkRuntimePass(bool IsRuntimeLinkage) {
  return new EraVMLinkRuntime(IsRuntimeLinkage);
}

PreservedAnalyses EraVMLinkRuntimePass::run(Module &M,
                                            ModuleAnalysisManager &AM) {
  FunctionAnalysisManager *FAM =
      &AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  EraVMLinkRuntimeImpl(M, STDLIB_DATA, FAM, Level == OptimizationLevel::Oz);
  return PreservedAnalyses::all();
}
