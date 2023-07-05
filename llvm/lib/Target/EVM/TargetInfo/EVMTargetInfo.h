//===-------- EVMTargetInfo.h - EVM Target Implementation -------*- C++ -*-===//
//
// This file registers the EVM target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H
#define LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H

namespace llvm {

class Target;
class APInt;

Target &getTheEVMTarget();

namespace EVM {

unsigned getStackOpcode(unsigned Opcode);
unsigned getRegisterOpcode(unsigned Opcode);
unsigned getPUSHOpcode(const APInt &Imm);

} // namespace EVM

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_TARGETINFO_EVMTARGETINFO_H
