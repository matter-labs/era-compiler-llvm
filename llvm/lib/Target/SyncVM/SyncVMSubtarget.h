//===-- SyncVMSubtarget.h - Define Subtarget for the SyncVM ----*- C++ -*--===//
//
// This file declares the SyncVM specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMSUBTARGET_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMSUBTARGET_H

#include "SyncVMFrameLowering.h"
#include "SyncVMISelLowering.h"
#include "SyncVMInstrInfo.h"
#include "SyncVMRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "SyncVMGenSubtargetInfo.inc"

namespace llvm {
class StringRef;

class SyncVMSubtarget : public SyncVMGenSubtargetInfo {
private:
  virtual void anchor();

  SyncVMFrameLowering FrameLowering;
  SyncVMInstrInfo InstrInfo;
  SyncVMTargetLowering TLInfo;
  SelectionDAGTargetInfo TSInfo;

public:
  /// This constructor initializes the data members to match that
  /// of the specified triple.
  ///
  SyncVMSubtarget(const Triple &TT, const std::string &CPU,
                  const std::string &FS, const TargetMachine &TM);

  /// ParseSubtargetFeatures - Parses features string setting specified
  /// subtarget options.  Definition of function is auto generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const SyncVMInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const TargetRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const SyncVMTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
  Align getStackAlignment() const { return Align(32); }
};
} // namespace llvm

#endif // LLVM_TARGET_SYNCVM_SUBTARGET_H
