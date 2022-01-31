//===--- SyncVMTargetStreamer.h - SyncVMTargetStreamer class --*- C++ -*---===//
//
// This file declares the SyncVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_MCTARGET_STREAMER_H
#define LLVM_LIB_TARGET_SYNCVM_MCTARGET_STREAMER_H
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class SyncVMTargetStreamer : public MCTargetStreamer {
public:
  SyncVMTargetStreamer(MCStreamer &S);
  ~SyncVMTargetStreamer() override;
  virtual void emitGlobalConst(APInt Value);

private:
  std::unique_ptr<AssemblerConstantPools> ConstantPools;
};
} // namespace llvm

#endif
