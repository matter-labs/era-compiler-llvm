// EVMMachineFunctionInfo.h-EVM machine function info-*- C++ -*-
//
//
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file declares EVM-specific per-machine-function
/// information.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H

#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCSymbolWasm.h"

namespace llvm {

/// This class is derived from MachineFunctionInfo and contains private
/// EVM-specific information for each MachineFunction.
class EVMFunctionInfo final : public MachineFunctionInfo {
  /// A mapping from CodeGen vreg index to a boolean value indicating whether
  /// the given register is considered to be "stackified", meaning it has been
  /// determined or made to meet the stack requirements:
  ///   - single use (per path)
  ///   - single def (per path)
  ///   - defined and used in LIFO order with other stack registers
  BitVector VRegStackified;

  /// Number of parameters. Their type doesn't matter as it always is i256.
  unsigned NumberOfParameters = 0;

public:
  explicit EVMFunctionInfo(MachineFunction &MF) {}
  EVMFunctionInfo(const EVMFunctionInfo &) = delete;
  EVMFunctionInfo(EVMFunctionInfo &&) = delete;
  EVMFunctionInfo &operator=(const EVMFunctionInfo &) = delete;
  EVMFunctionInfo &operator=(EVMFunctionInfo &&) = delete;
  ~EVMFunctionInfo() override;

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
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMMACHINEFUNCTIONINFO_H
