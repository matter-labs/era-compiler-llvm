//===-- SyncVMTargetInfo.cpp - SyncVM Target Implementation ---------------===//
//
// 
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/SyncVMTargetInfo.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheSyncVMTarget() {
  static Target TheSyncVMTarget;
  return TheSyncVMTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMTargetInfo() {
  RegisterTarget<Triple::UnknownArch> X(
    getTheSyncVMTarget(),
    "syncvm",
    "SyncVM [experimental]",
    "SyncVM"
  );
}
