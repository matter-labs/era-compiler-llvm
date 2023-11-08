//=---------- EVMInstrInfo.h - EVM Instruction Information -*- C++ -*--------=//
//
// This file contains the EVM implementation of the
// TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H
#define LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H

#include "EVMRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "EVMGenInstrInfo.inc"

namespace llvm {

class EVMInstrInfo final : public EVMGenInstrInfo {
  const EVMRegisterInfo RI;
  virtual void anchor();

public:
  explicit EVMInstrInfo();

  const EVMRegisterInfo &getRegisterInfo() const { return RI; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMINSTRINFO_H
