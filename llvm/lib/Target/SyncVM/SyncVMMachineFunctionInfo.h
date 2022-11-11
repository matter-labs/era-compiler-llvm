//=== SyncVMMachineFunctionInfo.h - SyncVM machine function info -*- C++ -*-==//
//
// This file declares SyncVM-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// SyncVMMachineFunctionInfo - This class is derived from MachineFunction and
/// contains private SyncVM target-specific information for each MachineFunction.
class SyncVMMachineFunctionInfo : public MachineFunctionInfo {
  virtual void anchor();

  /// CalleeSavedFrameSize - Size of the callee-saved register portion of the
  /// stack frame in bytes.
  unsigned CalleeSavedFrameSize = 0;

  /// ReturnAddrIndex - FrameIndex for return slot.
  int ReturnAddrIndex = 0;

  /// VarArgsFrameIndex - FrameIndex for start of varargs area.
  int VarArgsFrameIndex = 0;

  /// SRetReturnReg - Some subtargets require that sret lowering includes
  /// returning the value of the returned struct in a register. This field
  /// holds the virtual register into which the sret argument is passed.
  Register SRetReturnReg;

public:
  SyncVMMachineFunctionInfo() = default;

  explicit SyncVMMachineFunctionInfo(MachineFunction &MF)
    : CalleeSavedFrameSize(0), ReturnAddrIndex(0), SRetReturnReg(0) {}

  unsigned getCalleeSavedFrameSize() const { return CalleeSavedFrameSize; }
  void setCalleeSavedFrameSize(unsigned bytes) { CalleeSavedFrameSize = bytes; }

  Register getSRetReturnReg() const { return SRetReturnReg; }
  void setSRetReturnReg(Register Reg) { SRetReturnReg = Reg; }

  int getRAIndex() const { return ReturnAddrIndex; }
  void setRAIndex(int Index) { ReturnAddrIndex = Index; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex;}
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }
};

} // End llvm namespace

#endif
