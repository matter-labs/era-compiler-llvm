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
  const MCRegisterInfo *MRI;

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool parseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                     SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  const MCConstantExpr *createConstant(int64_t Value) {
    return MCConstantExpr::create(Value, getContext());
  }

  bool parseNameWithSuffixes(StringRef Name, SMLoc NameLoc,
                             OperandVector &Operands);
  bool parseRegOperand(OperandVector &Operands);
  ParseStatus tryParseUImm16Operand(OperandVector &Operands);
  bool parseRegisterWithAddend(MCRegister &RegNo, int &Addend);
  bool parseOperand(StringRef Mnemonic, OperandVector &Operands);

  ParseStatus tryParseStackOperand(OperandVector &Operands);
  ParseStatus tryParseCodeOperand(OperandVector &Operands);

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  const unsigned CellBitWidth = 256;
  bool ParseDirective(AsmToken DirectiveID) override;

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

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

  bool isStackReference() const { return Kind == k_Mem && !isCodeReference(); }

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

  MCRegister getReg() const override {
    assert(Kind == k_Reg && "Invalid access!");
    return Reg;
  }

  void setReg(unsigned RegNo) {
    assert(Kind == k_Reg && "Invalid access!");
    Reg = RegNo;
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
static MCRegister MatchRegisterName(StringRef Name);
static MCRegister MatchRegisterAltName(StringRef Name);
static void applyMnemonicAliases(StringRef &Mnemonic,
                                 const FeatureBitset &Features,
                                 unsigned VariantID);

bool EraVMAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  return true;
}

ParseStatus EraVMAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                             SMLoc &EndLoc) {
  if (!getLexer().is(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  auto Name = getTok().getIdentifier().lower();
  Reg = MatchRegisterName(Name);
  if (Reg == EraVM::NoRegister) {
    Reg = MatchRegisterAltName(Name);
    if (Reg == EraVM::NoRegister)
      return ParseStatus::NoMatch;
  }

  AsmToken const &T = getTok();
  StartLoc = T.getLoc();
  EndLoc = T.getEndLoc();
  Lex(); // eat register token

  return ParseStatus::Success;
}

static int parseExplicitCondition(StringRef Code) {
  return StringSwitch<int>(Code)
      .Cases("eq", "if_eq", EraVMCC::COND_E)
      .Cases("lt", "if_lt", EraVMCC::COND_LT)
      .Cases("gt", "if_gt", EraVMCC::COND_GT)
      .Cases("ne", "if_not_eq", EraVMCC::COND_NE)
      .Cases("ge", "if_ge", "if_gt_or_eq", EraVMCC::COND_GE)
      .Cases("le", "if_le", EraVMCC::COND_LE)
      .Default(EraVMCC::COND_INVALID);
}

bool EraVMAsmParser::parseNameWithSuffixes(StringRef Name, SMLoc NameLoc,
                                           OperandVector &Operands) {
  // Parses "<name>[!][.<cond>]", where name includes ".s" and possibly
  // other dot-separated parts. Spaces are not allowed around "!".

  // Make sure no spaces are between the tokens.
  const char *ExpectedNextLocPtr = NameLoc.getPointer() + Name.size();
  auto CheckNoSpaces = [&ExpectedNextLocPtr](const AsmToken &Tok) {
    if (ExpectedNextLocPtr != Tok.getLoc().getPointer())
      return false;

    ExpectedNextLocPtr = Tok.getEndLoc().getPointer();
    return true;
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
    if (!CheckNoSpaces(getTok()))
      return TokError("unexpected whitespace before '!'");
    if (CondCode != EraVMCC::COND_INVALID)
      return TokError("unexpected '!' after condition code");

    Lex(); // eat "!" token
    Operands.push_back(EraVMOperand::CreateToken("!", NameLoc));
  }

  if (getTok().is(AsmToken::Identifier) &&
      getTok().getString().starts_with(".") &&
      TryParseCC(getTok().getString().drop_front(1))) {
    if (!CheckNoSpaces(getTok()))
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
  if (tryParseRegister(RegNo, StartLoc, EndLoc).isSuccess())
    return true;

  Operands.push_back(EraVMOperand::CreateReg(RegNo, StartLoc, EndLoc));
  return false;
}

ParseStatus EraVMAsmParser::tryParseUImm16Operand(OperandVector &Operands) {
  if (getLexer().is(AsmToken::Minus) &&
      getLexer().peekTok().is(AsmToken::Integer)) {
    TokError("negative immediate operands are not supported");
    return ParseStatus::Failure;
  }

  if (!getLexer().is(AsmToken::Integer))
    return ParseStatus::NoMatch;

  const AsmToken &Tok = getTok();
  uint64_t IntValue = Tok.getIntVal();
  if (!isUIntN(16, IntValue)) {
    TokError("uint16 immediate expected");
    return ParseStatus::Failure;
  }
  const MCExpr *Expr = createConstant(IntValue);
  Operands.push_back(
      EraVMOperand::CreateImm(Expr, Tok.getLoc(), Tok.getEndLoc()));
  Lex();

  return ParseStatus::Success;
}

bool EraVMAsmParser::parseRegisterWithAddend(MCRegister &RegNo, int &Addend) {
  // If both register and addend are specified, let's only parse them
  // in that order as both "r1 + 42" and "r1 - 42" are possible, but
  // not "42 - r1", only "42 + r1" (as well as "-42 + r1").

  int Multiplier = 1;

  RegNo = 0;
  Addend = 0;

  // The register name is the first token, if it exists.
  if (getLexer().is(AsmToken::Identifier)) {
    SMLoc S, E;
    if (tryParseRegister(RegNo, S, E).isNoMatch())
      return TokError("register name expected");
  }

  // "+" or "-" is mandatory if a register name was parsed and addend has to be
  // parsed next, optional otherwise.
  switch (getTok().getKind()) {
  case AsmToken::RBrac:
    // "]" is the next token - keep it and stop further processing.
    // Return an error if and only if nothing was parsed at all.
    if (RegNo == 0)
      return TokError("empty subscript");
    return false;
  case AsmToken::Plus:
    Multiplier = 1;
    Lex(); // eat "+" token
    break;
  case AsmToken::Minus:
    Multiplier = -1;
    Lex(); // eat "-" token
    break;
  default:
    // If a register was parsed and this is not the end of bracket-enclosed
    // sub-expression, it should be followed by "+" or "-" token, otherwise
    // these are optional.
    if (RegNo)
      return TokError("'+' or '-' expected");
    break;
  }

  // Parse integer addend - at this point it is mandatory as the register-only
  // case was already handled above.
  if (!getLexer().is(AsmToken::Integer))
    return TokError("integer addend expected");

  Addend = Multiplier * getTok().getIntVal();
  Lex(); // eat integer token

  return false;
}

bool EraVMAsmParser::parseOperand(StringRef Mnemonic, OperandVector &Operands) {
  ParseStatus Result =
      MatchOperandParserImpl(Operands, Mnemonic, /*ParseForAllFeatures=*/true);
  if (Result.isSuccess())
    return false;
  if (Result.isFailure())
    return true;

  MCRegister RegNo = 0;
  SMLoc StartLoc, EndLoc;
  Result = tryParseRegister(RegNo, StartLoc, EndLoc);
  if (Result.isSuccess()) {
    Operands.push_back(EraVMOperand::CreateReg(RegNo, StartLoc, EndLoc));
    return false;
  }
  if (Result.isFailure())
    return true;

  return TokError("cannot parse operand");
}

ParseStatus EraVMAsmParser::tryParseStackOperand(OperandVector &Operands) {
  EraVM::MemOperandKind MemOpKind = EraVM::OperandStackAbsolute;
  MCRegister RegNo = 0;
  int Addend = 0;

  if (!getLexer().is(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  SMLoc StartOfOperand = getLexer().getLoc();
  if (getTok().getString() != "stack")
    return ParseStatus::NoMatch;
  Lex(); // eat "stack" token

  if (getTok().is(AsmToken::Minus)) {
    MemOpKind = EraVM::OperandStackSPRelative;
    Lex(); // eat "-" token
  }

  if (!getTok().is(AsmToken::LBrac)) {
    TokError("expected '['");
    return ParseStatus::Failure;
  }
  Lex(); // eat "[" token

  if (parseRegisterWithAddend(RegNo, Addend))
    return ParseStatus::Failure;

  if (parseToken(AsmToken::RBrac, "']' expected"))
    return ParseStatus::Failure;

  Operands.push_back(EraVMOperand::CreateMem(&getContext(), MemOpKind, RegNo,
                                             nullptr, Addend, StartOfOperand,
                                             getTok().getEndLoc()));

  return ParseStatus::Success;
}

ParseStatus EraVMAsmParser::tryParseCodeOperand(OperandVector &Operands) {
  SMLoc StartOfOperand = getLexer().getLoc();
  MCSymbol *Symbol = nullptr;
  MCRegister RegNo = 0;
  int Addend = 0;

  // Decide if this is a code operand
  SmallVector<AsmToken, 2> PeekedTokens(2);
  getLexer().peekTokens(PeekedTokens);
  if (getTok().is(AsmToken::At)) {
    // "@symbol_name[...]"
    if (!PeekedTokens[0].is(AsmToken::Identifier) ||
        !PeekedTokens[1].is(AsmToken::LBrac))
      return ParseStatus::NoMatch;

    Lex(); // eat "@" token
    Symbol = getContext().getOrCreateSymbol(getTok().getString());
    Lex(); // eat constant symbol name
    Lex(); // eat "[" token
  } else {
    // "code[...]"
    if (!getTok().is(AsmToken::Identifier) ||
        !PeekedTokens[0].is(AsmToken::LBrac) || getTok().getString() != "code")
      return ParseStatus::NoMatch;

    Lex(); // eat "code" token
    Lex(); // eat "[" token
  }

  if (parseRegisterWithAddend(RegNo, Addend))
    return ParseStatus::Failure;

  if (parseToken(AsmToken::RBrac, "']' expected"))
    return ParseStatus::Failure;

  if (Symbol) {
    // @symbol_name[reg + imm]
    Operands.push_back(EraVMOperand::CreateMem(
        &getContext(), EraVM::OperandCode, RegNo, Symbol, Addend,
        StartOfOperand, getTok().getEndLoc()));
  } else {
    // code[...]
    Operands.push_back(EraVMOperand::CreateMem(
        &getContext(), EraVM::OperandCode, RegNo, nullptr, Addend,
        StartOfOperand, getTok().getEndLoc()));
  }

  return ParseStatus::Success;
}

bool EraVMAsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  if (parseNameWithSuffixes(Name, NameLoc, Operands))
    return true;

  StringRef Mnemonic = static_cast<EraVMOperand &>(*Operands[0]).getToken();
  applyMnemonicAliases(Mnemonic, getAvailableFeatures(), /*VariantID=*/0);
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
    // At now, assume exactly one signed integer follows.
    // If an arbitrary MCExpr should be accepted as well, an MCTargetExpr
    // for 256-bit integer constant can be implemented and provided to
    // parseExpression machinery by overriding the parsePrimaryExpr
    // function in this class.

    bool IsNegated = false;
    if (getTok().is(AsmToken::Minus)) {
      IsNegated = true;
      Lex(); // eat "-" token
    }

    if (!getTok().is(AsmToken::Integer) && !getTok().is(AsmToken::BigNum))
      return TokError("integer literal expected");

    APInt Value = getTok().getAPIntVal();
    if (Value.getActiveBits() > CellBitWidth)
      return TokError("integer too wide");

    // emitIntValue(APInt Value) emits the amount of data based on
    // the bit width of Value, so extend to exactly 256 bits.
    Value = Value.zextOrTrunc(CellBitWidth);
    if (IsNegated)
      Value = -Value;

    Lex(); // eat integer token

    if (parseEOL())
      return true;

    getStreamer().emitIntValue(Value);

    return false;
  }
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
  return Match_InvalidOperand;
}
