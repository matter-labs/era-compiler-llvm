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
class MCInst;
class MCSymbol;

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

std::unique_ptr<MCObjectTargetWriter> createEraVMELFObjectWriter(uint8_t OSABI);

namespace EraVM {

/// Width of the value emitted by .cell N in bits.
constexpr unsigned CellBitWidth = 256;

/// At now, stack operands are handled specially:
/// * At the machine instruction level, many instructions may have 6 source
///   operand kinds and 4 destination operand kinds, with each combination
///   yielding a separate 11-bit opcode. On both "sides", three of these kinds
///   represent the stack-referring operands (absolute, SP-relative,
///   SP-modifying).
/// * This backend has multiple instructions defined for each "base
///   instruction" as well (such as ADDrrr_s for the three-register form of
///   ADD and ADDsrr_s for the form with stack-referring input operand), but
///   no distinction is made for different stack-operands (though, they are
///   differentiated from non-stack operands).
/// * For this reason, stack-referring operands are encoded at the MC level
///   as a (marker, reg, addend) triple, where
///   * reg+addend comprise the "address" (the "subscript" inside "[...]" such
///     as "stack-=[r1+42]")
///   * addend is either MCConstantExpr, MCSymbolRefExpr or the sum of these,
///     in this particular order
///   * marker is SP register for SP-relative addressing (stack-[...]),
///     R0 register for SP-modyfing addressing (stack-=[...] or stack+=[...])
///     and dummy integer immediate for absolute addressing
///   * a few special cases exist - see appendMCOperands function.
enum MemOperandKind {
  OperandCode,
  OperandStackAbsolute,
  OperandStackSPRelative,
  OperandStackSPDecrement,
  OperandStackSPIncrement,
};

void appendMCOperands(MCContext &Ctx, MCInst &MI, MemOperandKind Kind,
                      unsigned Reg, const MCSymbol *Symbol, int Addend);
} // namespace EraVM

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
