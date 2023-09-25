//===-- EraVMTargetInfo.cpp - EraVM Target Implementation -----------------===//
//
// 
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/EraVMTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheEraVMTarget() {
  static Target TheEraVMTarget;
  return TheEraVMTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMTargetInfo() {
  RegisterTarget<Triple::eravm, /*HasJIT*/false> X(
    getTheEraVMTarget(),
    "eravm",
    "EraVM [experimental]",
    "EraVM"
  );
}
