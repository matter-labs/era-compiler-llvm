//===-- EraVMAsmParser.cpp - Parse EraVM assembly to MCInst instructions --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMRegisterInfo.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "TargetInfo/EraVMTargetInfo.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "eravm-asm-parser"

using namespace llvm;

namespace {

/// Parses EraVM assembly from a stream.
class EraVMAsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  const MCRegisterInfo *MRI;

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool ParseRegister(unsigned &RegNo, SMLoc &StartLoc, SMLoc &EndLoc) override;
  OperandMatchResultTy tryParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                        SMLoc &EndLoc) override;

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool ParseDirective(AsmToken DirectiveID) override;

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

  MCAsmParser &getParser() const { return Parser; }
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }

  /// @name Auto-generated Matcher Functions
  /// {

#define GET_ASSEMBLER_HEADER
#include "EraVMGenAsmMatcher.inc"

  /// }

public:
  EraVMAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser),
        MRI(getContext().getRegisterInfo()) {
    MCAsmParserExtension::Initialize(Parser);

    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

/// A parsed EraVM assembly operand.
class EraVMOperand : public MCParsedAsmOperand {
  typedef MCParsedAsmOperand Base;

  enum KindTy {
    k_Imm,
    k_Reg,
    k_Tok,
    k_Mem,
    k_IndReg,
    k_PostIndReg
  } Kind;

  struct Memory {
    unsigned Reg;
    const MCExpr *Offset;
  };
  union {
    const MCExpr *Imm{};
    unsigned      Reg;
    StringRef     Tok;
    Memory        Mem;
  };

  SMLoc Start, End;

public:
  EraVMOperand(StringRef Tok, SMLoc const &S)
      : Kind(k_Tok), Tok(Tok), Start(S), End(S) {}
  EraVMOperand(KindTy Kind, unsigned Reg, SMLoc const &S, SMLoc const &E)
      : Kind(Kind), Reg(Reg), Start(S), End(E) {}
  EraVMOperand(MCExpr const *Imm, SMLoc const &S, SMLoc const &E)
      : Kind(k_Imm), Imm(Imm), Start(S), End(E) {}
  EraVMOperand(unsigned Reg, MCExpr const *Expr, SMLoc const &S, SMLoc const &E)
      : Kind(k_Mem), Mem({Reg, Expr}), Start(S), End(E) {}

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert((Kind == k_Reg || Kind == k_IndReg || Kind == k_PostIndReg) &&
           "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");

    Inst.addOperand(MCOperand::createReg(Reg));
  }

  void addExprOperand(MCInst &Inst, const MCExpr *Expr) const {
    // Add as immediate when possible
    if (!Expr)
      Inst.addOperand(MCOperand::createImm(0));
    else if (const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Imm && "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");

    addExprOperand(Inst, Imm);
  }

  void addMemOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Mem && "Unexpected operand kind");
    assert(N == 2 && "Invalid number of operands");

    Inst.addOperand(MCOperand::createReg(Mem.Reg));
    addExprOperand(Inst, Mem.Offset);
  }

  bool isReg()   const override { return Kind == k_Reg; }
  bool isImm()   const override { return Kind == k_Imm; }
  bool isToken() const override { return Kind == k_Tok; }
  bool isMem()   const override { return Kind == k_Mem; }
  bool isIndReg()         const { return Kind == k_IndReg; }
  bool isPostIndReg()     const { return Kind == k_PostIndReg; }

  bool isCGImm() const {
    if (Kind != k_Imm)
      return false;

    int64_t Val = 0;
    if (!Imm->evaluateAsAbsolute(Val))
      return false;

    if (Val == 0 || Val == 1 || Val == 2 || Val == 4 || Val == 8 || Val == -1)
      return true;

    return false;
  }

  StringRef getToken() const {
    assert(Kind == k_Tok && "Invalid access!");
    return Tok;
  }

  unsigned getReg() const override {
    assert(Kind == k_Reg && "Invalid access!");
    return Reg;
  }

  void setReg(unsigned RegNo) {
    assert(Kind == k_Reg && "Invalid access!");
    Reg = RegNo;
  }

  static std::unique_ptr<EraVMOperand> CreateToken(StringRef Str, SMLoc S) {
    return std::make_unique<EraVMOperand>(Str, S);
  }

  static std::unique_ptr<EraVMOperand> CreateReg(unsigned RegNum, SMLoc S,
                                                  SMLoc E) {
    return std::make_unique<EraVMOperand>(k_Reg, RegNum, S, E);
  }

  static std::unique_ptr<EraVMOperand> CreateImm(const MCExpr *Val, SMLoc S,
                                                  SMLoc E) {
    return std::make_unique<EraVMOperand>(Val, S, E);
  }

  static std::unique_ptr<EraVMOperand> CreateMem(unsigned RegNum,
                                                  const MCExpr *Val,
                                                  SMLoc S, SMLoc E) {
    return std::make_unique<EraVMOperand>(RegNum, Val, S, E);
  }

  static std::unique_ptr<EraVMOperand> CreateIndReg(unsigned RegNum, SMLoc S,
                                                     SMLoc E) {
    return std::make_unique<EraVMOperand>(k_IndReg, RegNum, S, E);
  }

  static std::unique_ptr<EraVMOperand> CreatePostIndReg(unsigned RegNum, SMLoc S,
                                                         SMLoc E) {
    return std::make_unique<EraVMOperand>(k_PostIndReg, RegNum, S, E);
  }

  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }

  void print(raw_ostream &O) const override {
    switch (Kind) {
    case k_Tok:
      O << "Token " << Tok;
      break;
    case k_Reg:
      O << "Register " << Reg;
      break;
    case k_Imm:
      O << "Immediate " << *Imm;
      break;
    case k_Mem:
      O << "Memory ";
      O << *Mem.Offset << "(" << Reg << ")";
      break;
    case k_IndReg:
      O << "RegInd " << Reg;
      break;
    case k_PostIndReg:
      O << "PostInc " << Reg;
      break;
    }
  }
};
} // end anonymous namespace

bool EraVMAsmParser::MatchAndEmitInstruction(SMLoc Loc, unsigned &Opcode,
                                              OperandVector &Operands,
                                              MCStreamer &Out,
                                              uint64_t &ErrorInfo,
                                              bool MatchingInlineAsm) {
  return true;
}

// Auto-generated by TableGen
static unsigned MatchRegisterName(StringRef Name);
static unsigned MatchRegisterAltName(StringRef Name);

bool EraVMAsmParser::ParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                    SMLoc &EndLoc) {
  return true;
}

OperandMatchResultTy EraVMAsmParser::tryParseRegister(unsigned &RegNo,
                                                       SMLoc &StartLoc,
                                                       SMLoc &EndLoc) {
  if (getLexer().getKind() == AsmToken::Identifier) {
    auto Name = getLexer().getTok().getIdentifier().lower();
    RegNo = MatchRegisterName(Name);
    if (RegNo == EraVM::NoRegister) {
      RegNo = MatchRegisterAltName(Name);
      if (RegNo == EraVM::NoRegister)
        return MatchOperand_NoMatch;
    }

    AsmToken const &T = getParser().getTok();
    StartLoc = T.getLoc();
    EndLoc = T.getEndLoc();
    getLexer().Lex(); // eat register token

    return MatchOperand_Success;
  }

  return MatchOperand_ParseFail;
}

bool EraVMAsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                       StringRef Name, SMLoc NameLoc,
                                       OperandVector &Operands) {
  return false;
}

bool EraVMAsmParser::ParseDirective(AsmToken DirectiveID) {
  return true;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMAsmParser() {
  RegisterMCAsmParser<EraVMAsmParser> X(getTheEraVMTarget());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "EraVMGenAsmMatcher.inc"

unsigned EraVMAsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                     unsigned Kind) {
  return Match_Success;
}
