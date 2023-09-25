//===-- EraVMMCTargetDesc.h - EraVM Target Descriptions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides EraVM specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMMCTARGETDESC_H
#define LLVM_LIB_TARGET_ERAVM_MCTARGETDESC_ERAVMMCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class formatted_raw_ostream;
class Target;
class MCAsmBackend;
class MCCodeEmitter;
class MCInstrInfo;
class MCInstPrinter;
class MCSubtargetInfo;
class MCRegisterInfo;
class MCContext;
class MCTargetOptions;
class MCObjectTargetWriter;
class MCStreamer;
class MCTargetStreamer;

MCTargetStreamer *createEraVMTargetAsmStreamer(MCStreamer &S,
                                                formatted_raw_ostream &OS,
                                                MCInstPrinter *InstPrint,
                                                bool isVerboseAsm);
MCTargetStreamer *createEraVMObjectTargetStreamer(MCStreamer &S,
                                                   const MCSubtargetInfo &STI);
/// Creates a machine code emitter for EraVM.
MCCodeEmitter *createEraVMMCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

MCAsmBackend *createEraVMMCAsmBackend(const Target &T,
                                       const MCSubtargetInfo &STI,
                                       const MCRegisterInfo &MRI,
                                       const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter>
createEraVMELFObjectWriter(uint8_t OSABI);

} // namespace llvm

// Defines symbolic names for EraVM registers.
// This defines a mapping from register name to register number.
#define GET_REGINFO_ENUM
#include "EraVMGenRegisterInfo.inc"

// Defines symbolic names for the EraVM instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "EraVMGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "EraVMGenSubtargetInfo.inc"

#endif
