//===----- EVMMCTargetDesc.h - EVM Target Descriptions ----------*- C++ -*-===//
//
// This file provides EVM specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCTARGETDESC_H
#define LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class Target;
class MCAsmBackend;
class MCCodeEmitter;
class MCInstrInfo;
class MCSubtargetInfo;
class MCRegisterInfo;
class MCContext;
class MCTargetOptions;
class MCObjectTargetWriter;

/// Creates a machine code emitter for EVM.
MCCodeEmitter *createEVMMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);

MCAsmBackend *createEVMMCAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                    const MCRegisterInfo &MRI,
                                    const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createEVMELFObjectWriter(uint8_t OSABI);

} // namespace llvm

// Defines symbolic names for EVM registers.
// This defines a mapping from register name to register number.
#define GET_REGINFO_ENUM
#include "EVMGenRegisterInfo.inc"

// Defines symbolic names for the EVM instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "EVMGenInstrInfo.inc"

#endif // LLVM_LIB_TARGET_EVM_MCTARGETDESC_EVMMCTARGETDESC_H
