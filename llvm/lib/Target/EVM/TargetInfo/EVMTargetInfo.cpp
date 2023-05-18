//===-------- EVMTargetInfo.cpp - EVM Target Implementation ---------------===//
//
// This file registers the EVM target.
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheEVMTarget() {
  static Target TheEVMTarget;
  return TheEVMTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMTargetInfo() {
  RegisterTarget<Triple::evm, /*HasJIT*/ false> X(
      getTheEVMTarget(), "evm",
      "Ethereum Virtual Machine [experimental] (256-bit big-endian)", "EVM");
}
