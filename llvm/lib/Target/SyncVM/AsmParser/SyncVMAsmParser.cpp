//===- SyncVMAsmParser.cpp - Parse SyncVM assembly to MCInst instructions -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SyncVM.h"
#include "SyncVMRegisterInfo.h"
#include "MCTargetDesc/SyncVMMCTargetDesc.h"
#include "TargetInfo/SyncVMTargetInfo.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/TargetRegistry.h"

#define DEBUG_TYPE "syncvm-asm-parser"

using namespace llvm;

namespace {

/// Parses SyncVM assembly from a stream.
class SyncVMAsmParser : public MCTargetAsmParser {
  const MCSubtargetInfo &STI;
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
  bool ParseDirectiveRefSym(AsmToken DirectiveID);

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

  bool parseJccInstruction(ParseInstructionInfo &Info, StringRef Name,
                           SMLoc NameLoc, OperandVector &Operands);

  bool ParseOperand(OperandVector &Operands);

  bool ParseLiteralValues(unsigned Size, SMLoc L);

  MCAsmParser &getParser() const { return Parser; }
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }

  /// @name Auto-generated Matcher Functions
  /// {

#define GET_ASSEMBLER_HEADER
#include "SyncVMGenAsmMatcher.inc"

  /// }

public:
  SyncVMAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                  const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), STI(STI), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    MRI = getContext().getRegisterInfo();

    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

/// A parsed SyncVM assembly operand.
class SyncVMOperand : public MCParsedAsmOperand {
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
    const MCExpr *Imm;
    unsigned      Reg;
    StringRef     Tok;
    Memory        Mem;
  };

  SMLoc Start, End;

public:
  SyncVMOperand(StringRef Tok, SMLoc const &S)
      : Base(), Kind(k_Tok), Tok(Tok), Start(S), End(S) {}
  SyncVMOperand(KindTy Kind, unsigned Reg, SMLoc const &S, SMLoc const &E)
      : Base(), Kind(Kind), Reg(Reg), Start(S), End(E) {}
  SyncVMOperand(MCExpr const *Imm, SMLoc const &S, SMLoc const &E)
      : Base(), Kind(k_Imm), Imm(Imm), Start(S), End(E) {}
  SyncVMOperand(unsigned Reg, MCExpr const *Expr, SMLoc const &S, SMLoc const &E)
      : Base(), Kind(k_Mem), Mem({Reg, Expr}), Start(S), End(E) {}

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

    int64_t Val;
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

  static std::unique_ptr<SyncVMOperand> CreateToken(StringRef Str, SMLoc S) {
    return std::make_unique<SyncVMOperand>(Str, S);
  }

  static std::unique_ptr<SyncVMOperand> CreateReg(unsigned RegNum, SMLoc S,
                                                  SMLoc E) {
    return std::make_unique<SyncVMOperand>(k_Reg, RegNum, S, E);
  }

  static std::unique_ptr<SyncVMOperand> CreateImm(const MCExpr *Val, SMLoc S,
                                                  SMLoc E) {
    return std::make_unique<SyncVMOperand>(Val, S, E);
  }

  static std::unique_ptr<SyncVMOperand> CreateMem(unsigned RegNum,
                                                  const MCExpr *Val,
                                                  SMLoc S, SMLoc E) {
    return std::make_unique<SyncVMOperand>(RegNum, Val, S, E);
  }

  static std::unique_ptr<SyncVMOperand> CreateIndReg(unsigned RegNum, SMLoc S,
                                                     SMLoc E) {
    return std::make_unique<SyncVMOperand>(k_IndReg, RegNum, S, E);
  }

  static std::unique_ptr<SyncVMOperand> CreatePostIndReg(unsigned RegNum, SMLoc S,
                                                         SMLoc E) {
    return std::make_unique<SyncVMOperand>(k_PostIndReg, RegNum, S, E);
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

bool SyncVMAsmParser::MatchAndEmitInstruction(SMLoc Loc, unsigned &Opcode,
                                              OperandVector &Operands,
                                              MCStreamer &Out,
                                              uint64_t &ErrorInfo,
                                              bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  switch (MatchResult) {
  case Match_Success:
    Inst.setLoc(Loc);
    Out.emitInstruction(Inst, STI);
    return false;
  case Match_MnemonicFail:
    return Error(Loc, "invalid instruction mnemonic");
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = Loc;
    if (ErrorInfo != ~0U) {
      if (ErrorInfo >= Operands.size())
        return Error(ErrorLoc, "too few operands for instruction");

      ErrorLoc = ((SyncVMOperand &)*Operands[ErrorInfo]).getStartLoc();
      if (ErrorLoc == SMLoc())
        ErrorLoc = Loc;
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }
  default:
    return true;
  }
}

// Auto-generated by TableGen
static unsigned MatchRegisterName(StringRef Name);
static unsigned MatchRegisterAltName(StringRef Name);

bool SyncVMAsmParser::ParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                    SMLoc &EndLoc) {
  switch (tryParseRegister(RegNo, StartLoc, EndLoc)) {
  case MatchOperand_ParseFail:
    return Error(StartLoc, "invalid register name");
  case MatchOperand_Success:
    return false;
  case MatchOperand_NoMatch:
    return true;
  }

  llvm_unreachable("unknown match result type");
}

OperandMatchResultTy SyncVMAsmParser::tryParseRegister(unsigned &RegNo,
                                                       SMLoc &StartLoc,
                                                       SMLoc &EndLoc) {
  if (getLexer().getKind() == AsmToken::Identifier) {
    auto Name = getLexer().getTok().getIdentifier().lower();
    RegNo = MatchRegisterName(Name);
    if (RegNo == SyncVM::NoRegister) {
      RegNo = MatchRegisterAltName(Name);
      if (RegNo == SyncVM::NoRegister)
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

bool SyncVMAsmParser::parseJccInstruction(ParseInstructionInfo &Info,
                                          StringRef Name, SMLoc NameLoc,
                                          OperandVector &Operands) {
  if (!Name.startswith_lower("j"))
    return true;

  auto CC = Name.drop_front().lower();
  unsigned CondCode;
  if (CC == "ne" || CC == "nz")
    CondCode = SyncVMCC::COND_NE;
  else if (CC == "eq" || CC == "z")
    CondCode = SyncVMCC::COND_E;
  else if (CC == "lo" || CC == "nc")
    CondCode = SyncVMCC::COND_LO;
  else if (CC == "hs" || CC == "c")
    CondCode = SyncVMCC::COND_HS;
  else if (CC == "n")
    CondCode = SyncVMCC::COND_N;
  else if (CC == "ge")
    CondCode = SyncVMCC::COND_GE;
  else if (CC == "l")
    CondCode = SyncVMCC::COND_L;
  else if (CC == "mp")
    CondCode = SyncVMCC::COND_NONE;
  else
    return Error(NameLoc, "unknown instruction");

  if (CondCode == (unsigned)SyncVMCC::COND_NONE)
    Operands.push_back(SyncVMOperand::CreateToken("jmp", NameLoc));
  else {
    Operands.push_back(SyncVMOperand::CreateToken("j", NameLoc));
    const MCExpr *CCode = MCConstantExpr::create(CondCode, getContext());
    Operands.push_back(SyncVMOperand::CreateImm(CCode, SMLoc(), SMLoc()));
  }

  // Skip optional '$' sign.
  if (getLexer().getKind() == AsmToken::Dollar)
    getLexer().Lex(); // Eat '$'

  const MCExpr *Val;
  SMLoc ExprLoc = getLexer().getLoc();
  if (getParser().parseExpression(Val))
    return Error(ExprLoc, "expected expression operand");

  int64_t Res;
  if (Val->evaluateAsAbsolute(Res))
    if (Res < -512 || Res > 511)
      return Error(ExprLoc, "invalid jump offset");

  Operands.push_back(SyncVMOperand::CreateImm(Val, ExprLoc,
                                              getLexer().getLoc()));

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    getParser().eatToEndOfStatement();
    return Error(Loc, "unexpected token");
  }

  getParser().Lex(); // Consume the EndOfStatement.
  return false;
}

bool SyncVMAsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                       StringRef Name, SMLoc NameLoc,
                                       OperandVector &Operands) {
  // Drop .w suffix
  if (Name.endswith_lower(".w"))
    Name = Name.drop_back(2);

  if (!parseJccInstruction(Info, Name, NameLoc, Operands))
    return false;

  // First operand is instruction mnemonic
  Operands.push_back(SyncVMOperand::CreateToken(Name, NameLoc));

  // If there are no more operands, then finish
  if (getLexer().is(AsmToken::EndOfStatement))
    return false;

  // Parse first operand
  if (ParseOperand(Operands))
    return true;

  // Parse second operand if any
  if (getLexer().is(AsmToken::Comma)) {
    getLexer().Lex(); // Eat ','
    if (ParseOperand(Operands))
      return true;
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    getParser().eatToEndOfStatement();
    return Error(Loc, "unexpected token");
  }

  getParser().Lex(); // Consume the EndOfStatement.
  return false;
}

bool SyncVMAsmParser::ParseDirectiveRefSym(AsmToken DirectiveID) {
  StringRef Name;
  if (getParser().parseIdentifier(Name))
    return TokError("expected identifier in directive");

  MCSymbol *Sym = getContext().getOrCreateSymbol(Name);
  getStreamer().emitSymbolAttribute(Sym, MCSA_Global);
  return false;
}

bool SyncVMAsmParser::ParseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getIdentifier();
  if (IDVal.lower() == ".long") {
    ParseLiteralValues(4, DirectiveID.getLoc());
  } else if (IDVal.lower() == ".word" || IDVal.lower() == ".short") {
    ParseLiteralValues(2, DirectiveID.getLoc());
  } else if (IDVal.lower() == ".byte") {
    ParseLiteralValues(1, DirectiveID.getLoc());
  } else if (IDVal.lower() == ".refsym") {
    return ParseDirectiveRefSym(DirectiveID);
  }
  return true;
}

bool SyncVMAsmParser::ParseOperand(OperandVector &Operands) {
  switch (getLexer().getKind()) {
  default: return true;
  case AsmToken::Identifier: {
    // try rN
    unsigned RegNo;
    SMLoc StartLoc, EndLoc;
    if (!ParseRegister(RegNo, StartLoc, EndLoc)) {
      Operands.push_back(SyncVMOperand::CreateReg(RegNo, StartLoc, EndLoc));
      return false;
    }
    LLVM_FALLTHROUGH;
  }
  case AsmToken::Integer:
  case AsmToken::Plus:
  case AsmToken::Minus: {
    SMLoc StartLoc = getParser().getTok().getLoc();
    const MCExpr *Val;
    // Try constexpr[(rN)]
    if (!getParser().parseExpression(Val)) {
      unsigned RegNo = SyncVM::PC;
      SMLoc EndLoc = getParser().getTok().getLoc();
      // Try (rN)
      if (getLexer().getKind() == AsmToken::LParen) {
        getLexer().Lex(); // Eat '('
        SMLoc RegStartLoc;
        if (ParseRegister(RegNo, RegStartLoc, EndLoc))
          return true;
        if (getLexer().getKind() != AsmToken::RParen)
          return true;
        EndLoc = getParser().getTok().getEndLoc();
        getLexer().Lex(); // Eat ')'
      }
      Operands.push_back(SyncVMOperand::CreateMem(RegNo, Val, StartLoc,
                                                  EndLoc));
      return false;
    }
    return true;
  }
  case AsmToken::Amp: {
    // Try &constexpr
    SMLoc StartLoc = getParser().getTok().getLoc();
    getLexer().Lex(); // Eat '&'
    const MCExpr *Val;
    if (!getParser().parseExpression(Val)) {
      SMLoc EndLoc = getParser().getTok().getLoc();
      Operands.push_back(SyncVMOperand::CreateMem(SyncVM::SR, Val, StartLoc,
                                                  EndLoc));
      return false;
    }
    return true;
  }
  case AsmToken::At: {
    // Try @rN[+]
    SMLoc StartLoc = getParser().getTok().getLoc();
    getLexer().Lex(); // Eat '@'
    unsigned RegNo;
    SMLoc RegStartLoc, EndLoc;
    if (ParseRegister(RegNo, RegStartLoc, EndLoc))
      return true;
    if (getLexer().getKind() == AsmToken::Plus) {
      Operands.push_back(SyncVMOperand::CreatePostIndReg(RegNo, StartLoc, EndLoc));
      getLexer().Lex(); // Eat '+'
      return false;
    }
    if (Operands.size() > 1) // Emulate @rd in destination position as 0(rd)
      Operands.push_back(SyncVMOperand::CreateMem(RegNo,
                                                  MCConstantExpr::create(0, getContext()), StartLoc, EndLoc));
    else
      Operands.push_back(SyncVMOperand::CreateIndReg(RegNo, StartLoc, EndLoc));
    return false;
  }
  case AsmToken::Hash:
    // Try #constexpr
    SMLoc StartLoc = getParser().getTok().getLoc();
    getLexer().Lex(); // Eat '#'
    const MCExpr *Val;
    if (!getParser().parseExpression(Val)) {
      SMLoc EndLoc = getParser().getTok().getLoc();
      Operands.push_back(SyncVMOperand::CreateImm(Val, StartLoc, EndLoc));
      return false;
    }
    return true;
  }
}

bool SyncVMAsmParser::ParseLiteralValues(unsigned Size, SMLoc L) {
  auto parseOne = [&]() -> bool {
    const MCExpr *Value;
    if (getParser().parseExpression(Value))
      return true;
    getParser().getStreamer().emitValue(Value, Size, L);
    return false;
  };
  return (parseMany(parseOne));
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMAsmParser() {
  RegisterMCAsmParser<SyncVMAsmParser> X(getTheSyncVMTarget());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "SyncVMGenAsmMatcher.inc"

static unsigned convertGR16ToGR8(unsigned Reg) {
  switch (Reg) {
  default:
    llvm_unreachable("Unknown GR16 register");
  case SyncVM::PC:  return SyncVM::PCB;
  case SyncVM::SP:  return SyncVM::SPB;
  case SyncVM::R4:  return SyncVM::R4B;
  case SyncVM::R5:  return SyncVM::R5B;
  case SyncVM::R6:  return SyncVM::R6B;
  case SyncVM::R7:  return SyncVM::R7B;
  case SyncVM::R8:  return SyncVM::R8B;
  case SyncVM::R9:  return SyncVM::R9B;
  case SyncVM::R10: return SyncVM::R10B;
  case SyncVM::R11: return SyncVM::R11B;
  case SyncVM::R12: return SyncVM::R12B;
  case SyncVM::R13: return SyncVM::R13B;
  case SyncVM::R14: return SyncVM::R14B;
  case SyncVM::R15: return SyncVM::R15B;
  }
}

unsigned SyncVMAsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                     unsigned Kind) {
  SyncVMOperand &Op = static_cast<SyncVMOperand &>(AsmOp);

  if (!Op.isReg())
    return Match_InvalidOperand;

  unsigned Reg = Op.getReg();
  bool isGR16 =
      SyncVMMCRegisterClasses[SyncVM::GR16RegClassID].contains(Reg);

  if (isGR16 && (Kind == MCK_GR8)) {
    Op.setReg(convertGR16ToGR8(Reg));
    return Match_Success;
  }

  return Match_InvalidOperand;
}
