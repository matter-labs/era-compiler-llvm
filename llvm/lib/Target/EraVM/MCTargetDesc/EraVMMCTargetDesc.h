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

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class formatted_raw_ostream;
class Target;
class MCAsmBackend;
class MCCodeEmitter;
class MCInstrDesc;
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

/// Describes an entry of the tablegen-erated table (see EraVMOpcodesList
/// defined in EraVMOpcodes.td).
///
/// For example, "add" instruction is described as `{"add", 25, 8, 2, 2}` since
/// the opcode formula for "add" is
///
///     OpAdd src dst set_flags ⇒ 25 + 8 × src + 2 × dst + set_flags
///
/// and depending on the operand kinds the instructions used by the backend are
/// * ADDrrr_s for "add r1, r2, r3" (no stack operands at all) with
///   OutOperandList being `(outs GR256:$rd0)` and InOperandList being
///   `(ins GR256:$rs0, GR256:$rs1, pred:$cc)`
/// * ADDsrs_s for "add stack-[r1], r2, stack+=[r3+1]" (no distinction is made
///   for different stack input and different stack output variants) with
///   OutOperandList being `(outs)` (empty) and InOperandList being
///   `(ins stackin:$src0, GR256:$rs1, stackout:$dst0, pred:$cc)`
/// * etc.
struct EraVMOpcodeInfo {
  /// Opcode name (such as "add").
  const char *Name;
  /// Base value for 11-bit opcode field (such as 25 for add).
  unsigned BaseOpcode;
  /// Multiplier corresponding to "src" in opcode formula (such as 8 for add).
  unsigned SrcMultiplier;
  /// Multiplier corresponding to "dst" in opcode formula (such as 2 for add).
  unsigned DstMultiplier;
  /// Index of the stackout operand in InOperandList of the instruction, if any
  /// (such as 2 for both add and mul).
  ///
  /// Note that the stackout operand is input operand of the instruction as it
  /// provides an address to write the result to.
  unsigned IndexOfStackDstUse;

  static unsigned getMCOperandIndexForUseIndex(const MCInstrDesc &Desc,
                                               unsigned UseIndex);

  /// Computes index of the first MCOperand corresponding to stackin operand.
  unsigned getMCOperandIndexOfStackSrc(const MCInstrDesc &Desc) const {
    return getMCOperandIndexForUseIndex(Desc, 0);
  }

  /// Computes index of the first MCOperand corresponding to stackout operand.
  unsigned getMCOperandIndexOfStackDst(const MCInstrDesc &Desc) const {
    return getMCOperandIndexForUseIndex(Desc, IndexOfStackDstUse);
  }
};

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
  OperandInvalid,
  OperandCode,
  OperandStackAbsolute,
  OperandStackSPRelative,
  OperandStackSPDecrement,
  OperandStackSPIncrement,
};

void analyzeMCOperandsCode(const MCInst &MI, unsigned Idx, unsigned &Reg,
                           const MCSymbol *&Symbol, int &Addend);

void analyzeMCOperandsStack(const MCInst &MI, unsigned Idx, bool IsSrc,
                            unsigned &Reg, MemOperandKind &Kind,
                            const MCSymbol *&Symbol, int &Addend);

// Returns the same Kind as analyzeMCOperandsStack returns, without other info.
MemOperandKind getStackOperandKind(const MCInst &MI, unsigned Idx, bool IsSrc);

void appendMCOperands(MCContext &Ctx, MCInst &MI, MemOperandKind Kind,
                      unsigned Reg, const MCSymbol *Symbol, int Addend);

// encode_src_mode / encode_dst_mode
enum EncodedOperandMode {
  ModeNotApplicable = -1, // no such operand
  ModeReg = 0,            // SrcReg, DstReg
  ModeSpMod = 1,          // SrcSpRelativePop, DstSpRelativePush
  ModeSpRel = 2,          // SrcSpRelative, DstSpRelative
  ModeStackAbs = 3,       // SrcStackAbsolute, DstStackAbsolute
  NumDstModes = 4,
  ModeImm = 4,  // SrcImm
  ModeCode = 5, // SrcCodeAddr
  NumSrcModes = 6,
};

static inline EncodedOperandMode
getModeEncodingForOperandKind(MemOperandKind Kind) {
  switch (Kind) {
  case OperandInvalid:
    return ModeNotApplicable;
  case OperandCode:
    return ModeCode;
  case OperandStackAbsolute:
    return ModeStackAbs;
  case OperandStackSPRelative:
    return ModeSpRel;
  case OperandStackSPDecrement:
  case OperandStackSPIncrement:
    return ModeSpMod;
  }
}

const uint64_t EncodedOpcodeMask = UINT64_C(0x7ff);

const EraVMOpcodeInfo *findOpcodeInfo(unsigned Opcode);
const EraVMOpcodeInfo *analyzeEncodedOpcode(unsigned EncodedOpcode,
                                            EncodedOperandMode &SrcMode,
                                            EncodedOperandMode &DstMode);
std::string getLinkerSymbolHash(StringRef SymName);
std::string getLinkerIndexedName(StringRef Name, unsigned SubIdx);
std::string getLinkerSymbolSectionName(StringRef Name);
std::string stripLinkerSymbolNameIndex(StringRef Name);
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
