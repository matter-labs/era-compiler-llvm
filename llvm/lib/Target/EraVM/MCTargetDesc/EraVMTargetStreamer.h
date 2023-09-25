//===--- EraVMTargetStreamer.h - EraVMTargetStreamer class --*- C++ -*---===//
//
// This file declares the EraVMTargetStreamer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGET_STREAMER_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGET_STREAMER_H
#include "llvm/MC/MCStreamer.h"

namespace llvm {

class EraVMTargetStreamer : public MCTargetStreamer {
public:
  EraVMTargetStreamer(MCStreamer &S);
  ~EraVMTargetStreamer() override;
  virtual void emitGlobalConst(APInt Value);

private:
  std::unique_ptr<AssemblerConstantPools> ConstantPools;
};
} // namespace llvm

#endif
