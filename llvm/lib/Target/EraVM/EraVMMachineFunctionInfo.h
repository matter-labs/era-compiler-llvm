//===-- EraVMMachineFunctionInfo.h - EraVM machine func info ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares EraVM-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// EraVMMachineFunctionInfo - This class is derived from MachineFunction and
/// contains private EraVM target-specific information for each MachineFunction.
class EraVMMachineFunctionInfo : public MachineFunctionInfo {
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

  // Whether we adjusted stack after machine outline in this function.
  bool StackAdjustedPostOutline = false;

  // Whether this function is outlined.
  bool IsOutlined = false;

  // Whether this function ends with a tail call.
  bool IsTailCall = false;

public:
  EraVMMachineFunctionInfo() = default;

  explicit EraVMMachineFunctionInfo(MachineFunction &MF)
    : CalleeSavedFrameSize(0), ReturnAddrIndex(0), SRetReturnReg(0) {}

  unsigned getCalleeSavedFrameSize() const { return CalleeSavedFrameSize; }
  void setCalleeSavedFrameSize(unsigned bytes) { CalleeSavedFrameSize = bytes; }

  Register getSRetReturnReg() const { return SRetReturnReg; }
  void setSRetReturnReg(Register Reg) { SRetReturnReg = Reg; }

  int getRAIndex() const { return ReturnAddrIndex; }
  void setRAIndex(int Index) { ReturnAddrIndex = Index; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex;}
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  bool isStackAdjustedPostOutline() const { return StackAdjustedPostOutline; }
  void setStackAdjustedPostOutline() { StackAdjustedPostOutline = true; }

  bool isOutlined() const { return IsOutlined; }
  void setIsOutlined() { IsOutlined = true; }

  bool isTailCall() const { return IsTailCall; }
  void setIsTailCall(bool IsTC) { IsTailCall = IsTC; }
};

} // End llvm namespace

#endif
