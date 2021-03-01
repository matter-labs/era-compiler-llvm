//===-- SyncVMTargetInfo.h - SyncVM Target Implementation -------*- C++ -*-===//
//
// 
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_TARGETINFO_SYNCVMTARGETINFO_H
#define LLVM_LIB_TARGET_SYNCVM_TARGETINFO_SYNCVMTARGETINFO_H

namespace llvm {

class Target;

Target &getTheSyncVMTarget();

} // namespace llvm

#endif // LLVM_LIB_TARGET_SYNCVM_TARGETINFO_SYNCVMTARGETINFO_H
