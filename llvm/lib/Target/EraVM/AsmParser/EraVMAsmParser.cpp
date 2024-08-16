//===-- EraVMAsmParser.cpp - Assembler for EraVM ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains code to parse EraVM assembly to MCInst instructions.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMRegisterInfo.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"
#include "MCTargetDesc/EraVMTargetStreamer.h"
#include "TargetInfo/EraVMTargetInfo.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/MC/MCAssembler.h"
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
  const MCRegisterInfo *MRI;
  StringSet<> LinkerSymbolNames;

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool parseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                     SMLoc &EndLoc) override;
  OperandMatchResultTy tryParseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                                        SMLoc &EndLoc) override;

  const MCConstantExpr *createConstant(int64_t Value) {
    return MCConstantExpr::create(Value, getContext());
  }

  bool parseNameWithSuffixes(StringRef Name, SMLoc NameLoc,
                             OperandVector &Operands);

  bool parseRegOperand(OperandVector &Operands);
  OperandMatchResultTy tryParseUImm16Operand(OperandVector &Operands);
  OperandMatchResultTy tryParseJumpTargetOperand(OperandVector &Operands);
  bool parseAddend(int &Addend, bool SignRequired);
  bool parseRegisterWithAddend(MCRegister &RegNo, MCSymbol *&Symbol,
                               int &Addend);
  bool parseOperand(StringRef Mnemonic, OperandVector &Operands);

  OperandMatchResultTy tryParseStackOperand(OperandVector &Operands);
  OperandMatchResultTy tryParseCodeOperand(OperandVector &Operands);

  bool parseAdjSP(OperandVector &Operands);

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool ParseDirective(AsmToken DirectiveID) override;

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;
  void onEndOfFile() override;

  /// @name Auto-generated Matcher Functions
  /// {

#define GET_ASSEMBLER_HEADER
#include "EraVMGenAsmMatcher.inc"

  /// }

public:
  EraVMAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII),
        MRI(Parser.getContext().getRegisterInfo()) {
    MCAsmParserExtension::Initialize(Parser);

    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

/// A parsed EraVM assembly operand.
class EraVMOperand : public MCParsedAsmOperand {
  using Base = MCParsedAsmOperand;
  enum KindTy { k_Imm, k_Reg, k_Tok, k_Mem };

  KindTy Kind;

  struct Memory {
    MCContext *Ctx;
    const MCSymbol *Symbol;
    MCRegister Reg;
    int Addend;
    EraVM::MemOperandKind Kind;
  };

  union {
    const MCExpr *Imm{};
    MCRegister Reg;
    StringRef Tok;
    Memory Mem;
  };

  SMLoc Start, End;

public:
  // Exactly one constructor per union member.
  EraVMOperand(SMLoc S, SMLoc E, const MCExpr *Imm)
      : Kind(k_Imm), Imm(Imm), Start(S), End(E) {}
  EraVMOperand(SMLoc S, SMLoc E, MCRegister Reg)
      : Kind(k_Reg), Reg(Reg), Start(S), End(E) {}
  EraVMOperand(SMLoc S, SMLoc E, StringRef Tok)
      : Kind(k_Tok), Tok(Tok), Start(S), End(E) {}
  EraVMOperand(SMLoc S, SMLoc E, MCContext *Ctx, const MCSymbol *Symbol,
               MCRegister Reg, int Addend, EraVM::MemOperandKind K)
      : Kind(k_Mem), Start(S), End(E) {
    Mem.Ctx = Ctx;
    Mem.Symbol = Symbol;
    Mem.Reg = Reg;
    Mem.Addend = Addend;
    Mem.Kind = K;
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Reg && "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");

    Inst.addOperand(MCOperand::createReg(Reg));
  }

  template <bool IsInput> bool isStackReference() const {
    if (Kind != k_Mem)
      return false;

    switch (Mem.Kind) {
    case EraVM::OperandInvalid:
    case EraVM::OperandCode:
      return false;
    case EraVM::OperandStackAbsolute:
    case EraVM::OperandStackSPRelative:
      return true;
    case EraVM::OperandStackSPDecrement:
    case EraVM::OperandStackSPIncrement:
      return IsInput == (Mem.Kind == EraVM::OperandStackSPDecrement);
    }
  }

  bool isCodeReference() const {
    return Kind == k_Mem && Mem.Kind == EraVM::OperandCode;
  }

  void addExprOperand(MCInst &Inst, const MCExpr *Expr) const {
    // Add as immediate when possible
    if (!Expr)
      Inst.addOperand(MCOperand::createImm(0));
    else if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Imm && "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");

    addExprOperand(Inst, Imm);
  }

  void addStackReferenceOperands(MCInst &Inst, unsigned N) const {
    assert(N == 3 && "Invalid number of operands!");
    assert(Kind == k_Mem && "Unexpected operand kind");
    EraVM::appendMCOperands(*Mem.Ctx, Inst, Mem.Kind, Mem.Reg, Mem.Symbol,
                            Mem.Addend);
  }

  void addCodeReferenceOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    assert(Kind == k_Mem && "Unexpected operand kind");
    EraVM::appendMCOperands(*Mem.Ctx, Inst, EraVM::OperandCode, Mem.Reg,
                            Mem.Symbol, Mem.Addend);
  }

  bool isReg() const override { return Kind == k_Reg; }
  bool isImm() const override { return Kind == k_Imm; }
  bool isToken() const override { return Kind == k_Tok; }
  bool isMem() const override { return false; }

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

  const MCExpr *getImm() const {
    assert(Kind == k_Imm && "Invalid access!");
    return Imm;
  }

  static std::unique_ptr<EraVMOperand> CreateToken(StringRef Str, SMLoc S) {
    return std::make_unique<EraVMOperand>(S, S, Str);
  }

  static std::unique_ptr<EraVMOperand> CreateReg(MCRegister Reg, SMLoc S,
                                                 SMLoc E) {
    return std::make_unique<EraVMOperand>(S, E, Reg);
  }

  static std::unique_ptr<EraVMOperand> CreateImm(const MCExpr *Val, SMLoc S,
                                                 SMLoc E) {
    return std::make_unique<EraVMOperand>(S, E, Val);
  }

  static std::unique_ptr<EraVMOperand>
  CreateMem(MCContext *Ctx, EraVM::MemOperandKind K, MCRegister Reg,
            const MCSymbol *Symbol, int Addend, SMLoc S, SMLoc E) {
    return std::make_unique<EraVMOperand>(S, E, Ctx, Symbol, Reg, Addend, K);
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
      O << "MemOperand(kind = " << Mem.Kind << " reg = " << Mem.Reg
        << " sym = " << Mem.Symbol << " addend = " << Mem.Addend << ")";
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
  MCInst Inst;

  if (Operands.empty())
    return true;

  switch (MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm)) {
  default:
    return Error(Loc, "cannot parse instruction");
  case Match_MnemonicFail: {
    auto MnemonicOperand = static_cast<EraVMOperand &>(*Operands.front());
    StringRef Mnemonic = "<unknown>";
    if (MnemonicOperand.isToken())
      Mnemonic = MnemonicOperand.getToken();
    return Error(Loc, "unknown mnemonic: " + Mnemonic);
  }
  case Match_Success:
    Inst.setLoc(Loc);
    Out.emitInstruction(Inst, *STI);
    return false;
  }
}

// Auto-generated by TableGen
static unsigned MatchRegisterName(StringRef Name);
static unsigned MatchRegisterAltName(StringRef Name);
static void applyMnemonicAliases(StringRef &Mnemonic,
                                 const FeatureBitset &Features,
                                 unsigned VariantID);

bool EraVMAsmParser::parseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  return true;
}

OperandMatchResultTy EraVMAsmParser::tryParseRegister(MCRegister &RegNo,
                                                      SMLoc &StartLoc,
                                                      SMLoc &EndLoc) {
  if (!getLexer().is(AsmToken::Identifier))
    return MatchOperand_NoMatch;

  auto Name = getTok().getIdentifier().lower();
  RegNo = MatchRegisterName(Name);
  if (RegNo == EraVM::NoRegister) {
    RegNo = MatchRegisterAltName(Name);
    if (RegNo == EraVM::NoRegister)
      return MatchOperand_NoMatch;
  }

  AsmToken const &T = getTok();
  StartLoc = T.getLoc();
  EndLoc = T.getEndLoc();
  Lex(); // eat register token

  return MatchOperand_Success;
}

static int parseExplicitCondition(StringRef Code) {
  return StringSwitch<int>(Code)
      .Case("eq", EraVMCC::COND_E)
      .Case("lt", EraVMCC::COND_LT)
      .Case("gt", EraVMCC::COND_GT)
      .Case("ne", EraVMCC::COND_NE)
      .Case("ge", EraVMCC::COND_GE)
      .Case("le", EraVMCC::COND_LE)
      .Case("gtlt", EraVMCC::COND_GTOrLT)
      .Default(EraVMCC::COND_INVALID);
}

bool EraVMAsmParser::parseNameWithSuffixes(StringRef Name, SMLoc NameLoc,
                                           OperandVector &Operands) {
  // Parses "<name>[!][.<cond>]", where name includes ".s" and possibly
  // other dot-separated parts. Spaces are not allowed around "!".

  // Make sure no spaces are between the tokens.
  const char *ExpectedNextLocPtr = NameLoc.getPointer() + Name.size();
  auto IsSeparatedBySpace = [&ExpectedNextLocPtr](const AsmToken &Tok) {
    if (ExpectedNextLocPtr != Tok.getLoc().getPointer())
      return true;

    ExpectedNextLocPtr = Tok.getEndLoc().getPointer();
    return false;
  };

  // From the lexer point of view, condition code can either be part of
  // Name or be a separate AsmToken::Identifier separated from Name with
  // AsmToken::Exclaim. For that reason, TryParseCC function ensures at most
  // one condition code string is accepted (so instructions like "sub.lt!.ge"
  // are rejected since ".ge" is not a valid operand).
  int CondCode = EraVMCC::COND_INVALID;
  auto TryParseCC = [&CondCode](const StringRef &MaybeCondStr) {
    // Do not process condition code twice.
    if (CondCode != EraVMCC::COND_INVALID)
      return false;

    CondCode = parseExplicitCondition(MaybeCondStr);
    return CondCode != EraVMCC::COND_INVALID;
  };

  // Drop ".<cond>" suffix from Name, if any.
  StringRef MaybeMnemonic, MaybeCondStr;
  std::tie(MaybeMnemonic, MaybeCondStr) = Name.rsplit('.');
  if (TryParseCC(MaybeCondStr))
    Name = MaybeMnemonic;

  Operands.push_back(EraVMOperand::CreateToken(Name, NameLoc));

  if (getTok().is(AsmToken::Exclaim)) {
    if (IsSeparatedBySpace(getTok()))
      return TokError("unexpected whitespace before '!'");
    if (CondCode != EraVMCC::COND_INVALID)
      return TokError("unexpected '!' after condition code");

    Lex(); // eat "!" token
    Operands.push_back(EraVMOperand::CreateToken("!", NameLoc));
  }

  if (getTok().is(AsmToken::Identifier) &&
      getTok().getString().startswith(".") &&
      TryParseCC(getTok().getString().drop_front(1))) {
    if (IsSeparatedBySpace(getTok()))
      return TokError("unexpected whitespace before condition code");
    Lex(); // eat ".<cond>" token
  }

  // If no condition code was parsed, set the default.
  if (CondCode == EraVMCC::COND_INVALID)
    CondCode = EraVMCC::COND_NONE;

  const MCExpr *CondCodeExpr = createConstant(CondCode);
  Operands.push_back(EraVMOperand::CreateImm(CondCodeExpr, NameLoc, NameLoc));

  return false;
}

bool EraVMAsmParser::parseRegOperand(OperandVector &Operands) {
  MCRegister RegNo = 0;
  SMLoc StartLoc, EndLoc;
  if (tryParseRegister(RegNo, StartLoc, EndLoc))
    return true;

  Operands.push_back(EraVMOperand::CreateReg(RegNo, StartLoc, EndLoc));
  return false;
}

OperandMatchResultTy
EraVMAsmParser::tryParseUImm16Operand(OperandVector &Operands) {
  // First check if this is a symbol + addend.
  if (getTok().is(AsmToken::At)) {
    MCSymbol *Symbol = nullptr;
    SMLoc StartOfOperand = getLexer().getLoc();
    int Addend = 0;

    Lex(); // eat "@" token
    Symbol = getContext().getOrCreateSymbol(getTok().getString());
    Lex(); // eat symbol name
    if (getTok().is(AsmToken::Plus) || getTok().is(AsmToken::Minus))
      if (parseAddend(Addend, /*SignRequired=*/true))
        return MatchOperand_ParseFail;

    const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, getContext());
    // FIXME Should we support negative addends?
    Addend &= (unsigned)0xffff;
    if (Addend) {
      const MCExpr *AddendExpr = createConstant(Addend);
      Expr = MCBinaryExpr::createAdd(Expr, AddendExpr, getContext());
    }
    Operands.push_back(
        EraVMOperand::CreateImm(Expr, StartOfOperand, getTok().getEndLoc()));
    return MatchOperand_Success;
  }

  if (getLexer().is(AsmToken::Minus) &&
      getLexer().peekTok().is(AsmToken::Integer)) {
    TokError("negative immediate operands are not supported");
    return MatchOperand_ParseFail;
  }

  if (!getLexer().is(AsmToken::Integer))
    return MatchOperand_NoMatch;

  const AsmToken &Tok = getTok();
  uint64_t IntValue = Tok.getIntVal();
  if (!isUIntN(16, IntValue)) {
    TokError("uint16 immediate expected");
    return MatchOperand_ParseFail;
  }
  const MCExpr *Expr = createConstant(IntValue);
  Operands.push_back(
      EraVMOperand::CreateImm(Expr, Tok.getLoc(), Tok.getEndLoc()));
  Lex();

  return MatchOperand_Success;
}

OperandMatchResultTy
EraVMAsmParser::tryParseJumpTargetOperand(OperandVector &Operands) {
  SMLoc StartLoc = getTok().getLoc();

  if (!getLexer().is(AsmToken::At))
    return MatchOperand_NoMatch;
  Lex(); // eat "@" token

  if (!getTok().is(AsmToken::Identifier)) {
    TokError("identifier expected");
    return MatchOperand_ParseFail;
  }
  MCSymbol *Symbol = getContext().getOrCreateSymbol(getTok().getString());
  SMLoc EndLoc = getTok().getEndLoc();
  Lex(); // eat symbol name

  const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, getContext());
  Operands.push_back(EraVMOperand::CreateImm(Expr, StartLoc, EndLoc));
  return MatchOperand_Success;
}

bool EraVMAsmParser::parseAddend(int &Addend, bool SignRequired) {
  int Multiplier = 1;
  switch (getTok().getKind()) {
  case AsmToken::Plus:
    Multiplier = 1;
    Lex(); // eat "+" token
    break;
  case AsmToken::Minus:
    Multiplier = -1;
    Lex(); // eat "-" token
    break;
  default:
    if (SignRequired)
      return TokError("'+' or '-' expected");
    break;
  }

  if (!getLexer().is(AsmToken::Integer))
    return TokError("integer addend expected");
  Addend = Multiplier * getTok().getIntVal();
  Lex(); // eat integer token

  return false;
}

bool EraVMAsmParser::parseRegisterWithAddend(MCRegister &RegNo,
                                             MCSymbol *&Symbol, int &Addend) {
  auto ParseRegister = [this, &RegNo]() {
    SMLoc S, E;
    if (tryParseRegister(RegNo, S, E))
      return TokError("register name expected");
    return false;
  };

  RegNo = 0;
  Symbol = nullptr;
  Addend = 0;

  // For simplicity, only accept @global at the beginning of [...] expression
  if (getTok().is(AsmToken::At)) {
    Lex(); // eat "@" token
    if (!getTok().is(AsmToken::Identifier))
      return TokError("symbol name expected");

    Symbol = getContext().getOrCreateSymbol(getTok().getString());
    Lex(); // eat symbol name token

    switch (getTok().getKind()) {
    case AsmToken::Plus:
      Lex(); // eat "+" token
      break; // ... then just parse any remaining tokens
    case AsmToken::Minus:
      // process "-" as always
      break;
    case AsmToken::RBrac:
      return false; // keep "]" token for the caller
    default:
      return TokError("'+' or '-' expected");
    }
  }

  if (getLexer().is(AsmToken::Identifier)) {
    if (ParseRegister())
      return true;
    if (getTok().is(AsmToken::RBrac))
      return false; // keep "]" token for the caller
    return parseAddend(Addend, /*SignRequired=*/true);
  }

  if (parseAddend(Addend, /*SignRequired=*/false))
    return true;
  if (getTok().is(AsmToken::RBrac))
    return false; // keep "]" token for the caller
  if (!getTok().is(AsmToken::Plus))
    return TokError("'+' expected");
  Lex(); // eat "+" token

  return ParseRegister();
}

bool EraVMAsmParser::parseOperand(StringRef Mnemonic, OperandVector &Operands) {
  OperandMatchResultTy Result =
      MatchOperandParserImpl(Operands, Mnemonic, /*ParseForAllFeatures=*/true);
  if (Result == llvm::MatchOperand_Success)
    return false;
  if (Result == MatchOperand_ParseFail)
    return true;

  MCRegister RegNo = 0;
  SMLoc StartLoc, EndLoc;
  Result = tryParseRegister(RegNo, StartLoc, EndLoc);
  if (Result == llvm::MatchOperand_Success) {
    Operands.push_back(EraVMOperand::CreateReg(RegNo, StartLoc, EndLoc));
    return false;
  }
  if (Result == MatchOperand_ParseFail)
    return true;

  return TokError("cannot parse operand");
}

OperandMatchResultTy
EraVMAsmParser::tryParseStackOperand(OperandVector &Operands) {
  EraVM::MemOperandKind MemOpKind = EraVM::OperandStackAbsolute;
  MCRegister RegNo = 0;
  MCSymbol *Symbol = nullptr;
  int Addend = 0;

  if (!getLexer().is(AsmToken::Identifier))
    return MatchOperand_NoMatch;

  SMLoc StartOfOperand = getLexer().getLoc();
  if (getTok().getString() != "stack")
    return MatchOperand_NoMatch;
  Lex(); // eat "stack" token

  if (getTok().is(AsmToken::Minus)) {
    Lex(); // eat "-" token
    if (getTok().is(AsmToken::Equal)) {
      // stack-=[...]
      MemOpKind = EraVM::OperandStackSPDecrement;
      Lex(); // eat "=" token
    } else {
      MemOpKind = EraVM::OperandStackSPRelative;
    }
  } else if (getTok().is(AsmToken::Plus)) {
    Lex(); // eat "+" token
    if (getTok().is(AsmToken::Equal)) {
      // stack+=[...]
      MemOpKind = EraVM::OperandStackSPIncrement;
      Lex(); // eat "=" token
    } else {
      TokError("'=' expected");
      return MatchOperand_ParseFail;
    }
  } else if (getTok().is(AsmToken::Equal)) {
    // alternative syntax: stack=[...] is alias of stack[...]
    Lex(); // eat "=" token
  }

  if (!getTok().is(AsmToken::LBrac)) {
    TokError("expected '['");
    return MatchOperand_ParseFail;
  }
  Lex(); // eat "[" token

  if (parseRegisterWithAddend(RegNo, Symbol, Addend))
    return MatchOperand_ParseFail;

  // FIXME Should we support negative addends?
  Addend &= (unsigned)0xffff;

  if (parseToken(AsmToken::RBrac, "']' expected"))
    return MatchOperand_ParseFail;

  if (Symbol && MemOpKind != EraVM::OperandStackAbsolute) {
    TokError("global stack symbols only supported with absolute addressing");
    return MatchOperand_ParseFail;
  }

  Operands.push_back(EraVMOperand::CreateMem(&getContext(), MemOpKind, RegNo,
                                             Symbol, Addend, StartOfOperand,
                                             getTok().getEndLoc()));

  return MatchOperand_Success;
}

OperandMatchResultTy
EraVMAsmParser::tryParseCodeOperand(OperandVector &Operands) {
  SMLoc StartOfOperand = getLexer().getLoc();
  MCSymbol *SymbolInSubscript = nullptr;
  MCRegister RegNo = 0;
  int Addend = 0;

  // "code[...]"
  if (!getTok().is(AsmToken::Identifier) ||
      !getLexer().peekTok().is(AsmToken::LBrac) ||
      getTok().getString() != "code")
    return MatchOperand_NoMatch;

  Lex(); // eat "code" token
  Lex(); // eat "[" token

  if (parseRegisterWithAddend(RegNo, SymbolInSubscript, Addend))
    return MatchOperand_ParseFail;

  // FIXME Should we support negative addends?
  Addend &= (unsigned)0xffff;

  if (parseToken(AsmToken::RBrac, "']' expected"))
    return MatchOperand_ParseFail;

  Operands.push_back(EraVMOperand::CreateMem(
      &getContext(), EraVM::OperandCode, RegNo, SymbolInSubscript, Addend,
      StartOfOperand, getTok().getEndLoc()));

  return MatchOperand_Success;
}

bool EraVMAsmParser::parseAdjSP(OperandVector &Operands) {
  SMLoc S, E;
  MCRegister Reg;
  int Addend = 0;

  while (!parseOptionalToken(AsmToken::EndOfStatement)) {
    switch (getTok().getKind()) {
    default:
      return TokError("unexpected token");
    case AsmToken::Identifier:
      if (tryParseRegister(Reg, S, E))
        return Error(getTok().getLoc(), "cannot parse register");
      Operands.push_back(EraVMOperand::CreateReg(Reg, S, E));
      break;
    case AsmToken::Plus:
      Operands.push_back(EraVMOperand::CreateToken("+", getTok().getLoc()));
      Lex(); // eat "+" token
      break;
    case AsmToken::Integer:
      Addend = getTok().getIntVal();
      S = getTok().getLoc();
      E = getTok().getEndLoc();
      Operands.push_back(EraVMOperand::CreateImm(createConstant(Addend), S, E));
      Lex(); // eat integer token
      break;
    }
  }
  return false;
}

bool EraVMAsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  if (parseNameWithSuffixes(Name, NameLoc, Operands))
    return true;

  StringRef Mnemonic = static_cast<EraVMOperand &>(*Operands[0]).getToken();
  applyMnemonicAliases(Mnemonic, getAvailableFeatures(), /*VariantID=*/0);

  if (Mnemonic == "incsp" || Mnemonic == "decsp")
    return parseAdjSP(Operands);

  auto ParseOne = [this, Mnemonic, &Operands]() {
    return parseOperand(Mnemonic, Operands);
  };
  return parseMany(ParseOne);
}

bool EraVMAsmParser::ParseDirective(AsmToken DirectiveID) {
  if (DirectiveID.getString() == ".note.GNU") {
    // Make print-then-parse round-trip work out of box.
    // FIXME Do not produce .note.GNU-stack
    if (parseToken(AsmToken::Minus, "'-' expected"))
      return true;
    if (!getTok().is(AsmToken::Identifier) || getTok().getString() != "stack")
      return TokError("invalid directive");
    Lex();
    return false;
  }
  if (DirectiveID.getString() == ".cell") {
    // At now, assume either one signed integer or a BB name follows.
    // If an arbitrary MCExpr should be accepted as well, an MCTargetExpr
    // for 256-bit integer constant can be implemented and provided to
    // parseExpression machinery by overriding the parsePrimaryExpr
    // function in this class.

    SmallVector<std::unique_ptr<MCParsedAsmOperand>, 1> Operands;
    if (MatchOperand_Success == tryParseJumpTargetOperand(Operands)) {
      assert(Operands.size() == 1);
      const MCExpr *Imm =
          static_cast<EraVMOperand *>(Operands[0].get())->getImm();

      if (parseEOL())
        return true;

      auto *TS = getStreamer().getTargetStreamer();
      static_cast<EraVMTargetStreamer *>(TS)->emitJumpTarget(Imm);

      return false;
    }

    bool IsNegated = false;
    if (getTok().is(AsmToken::Minus)) {
      IsNegated = true;
      Lex(); // eat "-" token
    }

    if (!getTok().is(AsmToken::Integer) && !getTok().is(AsmToken::BigNum))
      return TokError("integer literal expected");

    APInt Value = getTok().getAPIntVal();
    if (Value.getActiveBits() > EraVM::CellBitWidth)
      return TokError("integer too wide");

    // emitIntValue(APInt Value) emits the amount of data based on
    // the bit width of Value, so extend to exactly 256 bits.
    Value = Value.zextOrTrunc(EraVM::CellBitWidth);
    if (IsNegated)
      Value = -Value;

    Lex(); // eat integer token

    if (parseEOL())
      return true;

    auto *TS = getStreamer().getTargetStreamer();
    static_cast<EraVMTargetStreamer *>(TS)->emitCell(Value);

    return false;
  }
  // Parses directive:
  // ::= .library_address_cell @(@identificator | "string")
  if (DirectiveID.getString() == ".linker_symbol_cell") {
    if (!getLexer().is(AsmToken::At))
      return TokError("expected symbol name starting with @");
    Lex(); // eat "@" token

    StringRef SymbolName;
    if (getParser().parseIdentifier(SymbolName))
      return TokError("expected symbol name");

    if (parseEOL())
      return true;

    if (getContext().lookupSymbol(SymbolName))
      return TokError("duplicating library symbols");

    [[maybe_unused]] auto It = LinkerSymbolNames.insert(SymbolName);
    assert(It.second);

    MCSymbol *Symbol = getContext().getOrCreateSymbol(SymbolName);
    auto *TS = getStreamer().getTargetStreamer();
    static_cast<EraVMTargetStreamer *>(TS)->emitLinkerSymbol(Symbol);

    return false;
  }
  return true;
}

void EraVMAsmParser::onEndOfFile() {
  const MCAssembler *MCAsm = getStreamer().getAssemblerPtr();
  if (!MCAsm)
    return;

  StringSet<> SectionNames;
  for (const auto &Sec : llvm::make_range(MCAsm->begin(), MCAsm->end()))
    SectionNames.insert(Sec.getName());

  for (const StringSet<>::value_type &Entry : LinkerSymbolNames) {
    StringRef LinkerSymName = Entry.first();
    std::string SecName = EraVM::getLinkerSymbolSectionName(LinkerSymName);
    getParser().check(!SectionNames.count(SecName),
                      "No section corresponding to the linker symbol: " +
                          LinkerSymName);
  }
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMAsmParser() {
  RegisterMCAsmParser<EraVMAsmParser> X(getTheEraVMTarget());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "EraVMGenAsmMatcher.inc"

unsigned EraVMAsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                    unsigned Kind) {
  return Match_InvalidOperand;
}
