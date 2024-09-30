//=------ EVMMachineFunctionInfo.h - EVM machine function info ----*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares EVM-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H

#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

namespace llvm {

class EVMMachineFunctionInfo;

namespace yaml {

struct EVMMachineFunctionInfo final : public yaml::MachineFunctionInfo {
  bool IsStackified = false;

  EVMMachineFunctionInfo() = default;
  explicit EVMMachineFunctionInfo(const llvm::EVMMachineFunctionInfo &MFI);
  ~EVMMachineFunctionInfo() override;

  void mappingImpl(yaml::IO &YamlIO) override;
};

template <> struct MappingTraits<EVMMachineFunctionInfo> {
  static void mapping(IO &YamlIO, EVMMachineFunctionInfo &MFI) {
    YamlIO.mapOptional("isStackified", MFI.IsStackified, false);
  }
};
} // end namespace yaml

/// This class is derived from MachineFunctionInfo and contains private
/// EVM-specific information for each MachineFunction.
class EVMMachineFunctionInfo final : public MachineFunctionInfo {
  /// A mapping from CodeGen vreg index to a boolean value indicating whether
  /// the given register is considered to be "stackified", meaning it has been
  /// determined or made to meet the stack requirements:
  ///   - single use (per path)
  ///   - single def (per path)
  ///   - defined and used in LIFO order with other stack registers
  BitVector VRegStackified;

  /// Number of parameters. Their type doesn't matter as it always is i256.
  unsigned NumberOfParameters = 0;

  /// If the MF's instructions are in 'stack' form.
  bool IsStackified = false;

public:
  explicit EVMMachineFunctionInfo(MachineFunction &MF) {}
  EVMMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}
  ~EVMMachineFunctionInfo() override;

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  void initializeBaseYamlFields(const yaml::EVMMachineFunctionInfo &YamlMFI);

  void stackifyVReg(MachineRegisterInfo &MRI, unsigned VReg) {
    assert(MRI.getUniqueVRegDef(VReg));
    auto I = Register::virtReg2Index(VReg);
    if (I >= VRegStackified.size())
      VRegStackified.resize(I + 1);
    VRegStackified.set(I);
  }

  void unstackifyVReg(unsigned VReg) {
    auto I = Register::virtReg2Index(VReg);
    if (I < VRegStackified.size())
      VRegStackified.reset(I);
  }

  bool isVRegStackified(unsigned VReg) const {
    auto I = Register::virtReg2Index(VReg);
    if (I >= VRegStackified.size())
      return false;
    return VRegStackified.test(I);
  }

  void addParam() {
    ++NumberOfParameters;
  }

  unsigned getNumParams() const {
    return NumberOfParameters;
  }

  void setIsStackified(bool Val = true) { IsStackified = Val; }

  bool getIsStackified() const { return IsStackified; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H
