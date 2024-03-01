//===-------- EVMTargetInfo.cpp - EVM Target Implementation ---------------===//
//
// This file registers the EVM target.
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheEVMTarget() {
  static Target TheEVMTarget;
  return TheEVMTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMTargetInfo() {
  const RegisterTarget<Triple::evm, /*HasJIT*/ false> X(
      getTheEVMTarget(), "evm",
      "Ethereum Virtual Machine [experimental] (256-bit big-endian)", "EVM");
}

#define GET_INSTRMAP_INFO
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "EVMGenInstrInfo.inc"

unsigned llvm::EVM::getRegisterOpcode(unsigned Opcode) {
  assert(Opcode <= std::numeric_limits<uint16_t>::max());
  auto Opc = static_cast<uint16_t>(Opcode);
  int Res = EVM::getRegisterOpcode(Opc);
  assert(Res >= 0);
  return static_cast<unsigned>(Res);
}

unsigned llvm::EVM::getStackOpcode(unsigned Opcode) {
  assert(Opcode <= std::numeric_limits<uint16_t>::max());
  auto Opc = static_cast<uint16_t>(Opcode);
  int Res = EVM::getStackOpcode(Opc);
  assert(Res >= 0);
  return static_cast<unsigned>(Res);
}

unsigned llvm::EVM::getPUSHOpcode(const APInt &Imm) {
  if (Imm.isZero())
    return EVM::PUSH0;

  const unsigned ByteWidth = alignTo(Imm.getActiveBits(), 8) / 8;
  switch (ByteWidth) {
  case 1:
    return EVM::PUSH1;
  case 2:
    return EVM::PUSH2;
  case 3:
    return EVM::PUSH3;
  case 4:
    return EVM::PUSH4;
  case 5:
    return EVM::PUSH5;
  case 6:
    return EVM::PUSH6;
  case 7:
    return EVM::PUSH7;
  case 8:
    return EVM::PUSH8;
  case 9:
    return EVM::PUSH9;
  case 10:
    return EVM::PUSH10;
  case 11:
    return EVM::PUSH11;
  case 12:
    return EVM::PUSH12;
  case 13:
    return EVM::PUSH13;
  case 14:
    return EVM::PUSH14;
  case 15:
    return EVM::PUSH15;
  case 16:
    return EVM::PUSH16;
  case 17:
    return EVM::PUSH17;
  case 18:
    return EVM::PUSH18;
  case 19:
    return EVM::PUSH19;
  case 20:
    return EVM::PUSH20;
  case 21:
    return EVM::PUSH21;
  case 22:
    return EVM::PUSH22;
  case 23:
    return EVM::PUSH23;
  case 24:
    return EVM::PUSH24;
  case 25:
    return EVM::PUSH25;
  case 26:
    return EVM::PUSH26;
  case 27:
    return EVM::PUSH27;
  case 28:
    return EVM::PUSH28;
  case 29:
    return EVM::PUSH29;
  case 30:
    return EVM::PUSH30;
  case 31:
    return EVM::PUSH31;
  case 32:
    return EVM::PUSH32;
  default:
    llvm_unreachable("Unexpected stack depth");
  }
}
