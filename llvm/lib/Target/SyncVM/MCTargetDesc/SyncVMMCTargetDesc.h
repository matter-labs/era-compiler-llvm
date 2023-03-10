//===-- SyncVMMCTargetDesc.h - SyncVM Target Descriptions -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides SyncVM specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_MCTARGETDESC_SYNCVMMCTARGETDESC_H
#define LLVM_LIB_TARGET_SYNCVM_MCTARGETDESC_SYNCVMMCTARGETDESC_H

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

MCTargetStreamer *createSyncVMTargetAsmStreamer(MCStreamer &S,
                                                formatted_raw_ostream &OS,
                                                MCInstPrinter *InstPrint,
                                                bool isVerboseAsm);
MCTargetStreamer *createSyncVMObjectTargetStreamer(MCStreamer &S,
                                                   const MCSubtargetInfo &STI);
/// Creates a machine code emitter for SyncVM.
MCCodeEmitter *createSyncVMMCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

MCAsmBackend *createSyncVMMCAsmBackend(const Target &T,
                                       const MCSubtargetInfo &STI,
                                       const MCRegisterInfo &MRI,
                                       const MCTargetOptions &Options);

MCTargetStreamer *createSyncVMObjectTargetStreamer(MCStreamer &S,
                                                   const MCSubtargetInfo &STI);

std::unique_ptr<MCObjectTargetWriter>
createSyncVMELFObjectWriter(uint8_t OSABI);

} // namespace llvm

// Defines symbolic names for SyncVM registers.
// This defines a mapping from register name to register number.
#define GET_REGINFO_ENUM
#include "SyncVMGenRegisterInfo.inc"

// Defines symbolic names for the SyncVM instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "SyncVMGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "SyncVMGenSubtargetInfo.inc"

#endif
