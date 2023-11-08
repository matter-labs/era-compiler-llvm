//===-------- EVMTargetInfo.h - EVM Target Implementation -------*- C++ -*-===//
//
// This file registers the EVM target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H
#define LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H

namespace llvm {

class Target;

Target &getTheEVMTarget();

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H
