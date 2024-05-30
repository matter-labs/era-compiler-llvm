//===-- EraVMAsmPrinter.cpp - EraVM LLVM assembly printer -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the EraVM assembly language.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"
#include "EraVMInstrInfo.h"
#include "EraVMMCInstLower.h"
#include "EraVMTargetMachine.h"
#include "MCTargetDesc/EraVMInstPrinter.h"
#include "MCTargetDesc/EraVMTargetStreamer.h"
#include "TargetInfo/EraVMTargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class EraVMAsmPrinter : public AsmPrinter {
  EraVMMCInstLower MCInstLowering;

  // In a compiled machine module, there might be multiple instances of a
  // same constant values (across different compiled functions), however,
  // emitting only 1 is enough for the assembly because it is global.

  // Notice that constants are interned internally in LLVM, so we maintain a
  // map <constant -> SymbolList> to record and combine same constants with
  // different symbols. when we walk through the MCInst format.
  // To keep the order and maintain determinism, we actually use std::vector
  // so when emitting constants, the one that got inserted first will be printed
  // first

  using ConstantPoolMapType =
      std::map<const Constant *, std::vector<MCSymbol *>>;
  std::vector<const Constant *> UniqueConstants;
  ConstantPoolMapType ConstantPoolMap;

public:
  EraVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)), MCInstLowering(OutContext, *this) {
  }

  StringRef getPassName() const override { return "EraVM Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  bool emitPseudoExpansionLowering(MCStreamer &OutStreamer,
                                   const MachineInstr *MI);

  void emitInstruction(const MachineInstr *MI) override;
  using AliasMapTy = DenseMap<uint64_t, SmallVector<const GlobalAlias *, 1>>;
  void emitGlobalConstant(const DataLayout &DL, const Constant *CV,
                          AliasMapTy *AliasList = nullptr) override;

  // Wrapper needed for tblgenned pseudo lowering.
  bool lowerOperand(const MachineOperand &MO, MCOperand &MCOp) const {
    return MCInstLowering.lowerOperand(MO, MCOp);
  }

  void emitJumpTableInfo() override;
  void emitConstantPool() override;
  void emitEndOfAsmFile(Module &) override;
};
} // end of anonymous namespace

// Simple pseudo-instructions have their lowering (with expansion to real
// instructions) auto-generated.
#include "EraVMGenMCPseudoLowering.inc"

//===----------------------------------------------------------------------===//
void EraVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  // Do any auto-generated pseudo lowerings.
  if (emitPseudoExpansionLowering(*OutStreamer, MI))
    return;

  MCInst TmpInst;

  // Do some manual expansion
  unsigned Opc = MI->getOpcode();
  switch (Opc) {
  default:
    // Change nothing by default
    break;
  case EraVM::J_s: {
    MCOperand MCOp;
    TmpInst.setOpcode(EraVM::JCs);
    // Operand: dest
    lowerOperand(MI->getOperand(0), MCOp);
    TmpInst.addOperand(MCOp);
    lowerOperand(MI->getOperand(1), MCOp);
    TmpInst.addOperand(MCOp);
    lowerOperand(MI->getOperand(2), MCOp);
    TmpInst.addOperand(MCOp);
    // Operand: cc
    TmpInst.addOperand(MCOperand::createImm(0));
    EmitToStreamer(*OutStreamer, TmpInst);
    return;
  }
  case EraVM::DEFAULT_FAR_REVERT:
  case EraVM::DEFAULT_FAR_RETURN: {
    bool IsRevert = Opc == EraVM::DEFAULT_FAR_REVERT;
    MCSymbol *DefaultFarReturnSym = OutContext.getOrCreateSymbol(
        IsRevert ? "DEFAULT_FAR_REVERT" : "DEFAULT_FAR_RETURN");
    // Expand to: ret/revert.to_label $rs0, @DEFAULT_FAR_RETURN
    MCOperand MCOp;
    TmpInst.setOpcode(IsRevert ? EraVM::REVERTrl : EraVM::RETrl);
    // Operand: rs0
    lowerOperand(MI->getOperand(0), MCOp);
    TmpInst.addOperand(MCOp);
    // Operand: default dest
    TmpInst.addOperand(MCOperand::createExpr(
        MCSymbolRefExpr::create(DefaultFarReturnSym, OutContext)));
    // Operand: cc
    lowerOperand(MI->getOperand(1), MCOp);
    TmpInst.addOperand(MCOp);
    EmitToStreamer(*OutStreamer, TmpInst);
    return;
  }
  }

  if (MI->isPseudo()) {
#ifndef NDEBUG
    MI->dump();
#endif
    llvm_unreachable("pseudo opcode found in emitInstruction()");
  }

  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

void EraVMAsmPrinter::emitJumpTableInfo() {
  // The default implementation would try to emit 256-bit fixup, so provide
  // custom implementation based on emitJumpTableInfo and emitJumpTableEntry
  // from AsmPrinter (the latter is not virtual) that emits 16-bit relocation
  // and takes scaling into account.

  auto *TS =
      static_cast<EraVMTargetStreamer *>(OutStreamer->getTargetStreamer());
  const DataLayout &DL = MF->getDataLayout();
  const MachineJumpTableInfo *MJTI = MF->getJumpTableInfo();
  if (!MJTI)
    return;
  assert(MJTI->getEntryKind() == MachineJumpTableInfo::EK_BlockAddress);
  const std::vector<MachineJumpTableEntry> &JT = MJTI->getJumpTables();
  if (JT.empty())
    return;

  // Switch section.
  const Function &F = MF->getFunction();
  MCSection *Section = getObjFileLowering().getSectionForJumpTable(F, TM);
  OutStreamer->switchSection(Section);

  emitAlignment(Align(MJTI->getEntryAlignment(DL)));

  for (unsigned JTI = 0, e = JT.size(); JTI != e; ++JTI) {
    const std::vector<MachineBasicBlock *> &JTBBs = JT[JTI].MBBs;

    // If this jump table was deleted, ignore it.
    if (JTBBs.empty())
      continue;

    OutStreamer->emitLabel(GetJTISymbol(JTI));

    for (const MachineBasicBlock *MBB : JTBBs) {
      assert(MBB && MBB->getNumber() >= 0 && "Invalid basic block");
      const MCExpr *Value =
          MCSymbolRefExpr::create(MBB->getSymbol(), OutContext);
      TS->emitJumpTarget(Value);
    }
  }
}

bool EraVMAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

void EraVMAsmPrinter::emitEndOfAsmFile(Module &) {
  MCSection *ReadOnlySection =
      OutContext.getELFSection(".rodata", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);

  OutStreamer->switchSection(ReadOnlySection);

  // combine labels pointing to same constants
  for (const Constant *C : UniqueConstants) {
    std::vector<MCSymbol *> symbols = ConstantPoolMap[C];

    for (auto *symbol : symbols) {
      OutStreamer->emitLabel(symbol);
    }
    // then print constant:
    emitGlobalConstant(getDataLayout(), C);
  }

  // after emitting all the things, we also need to clear symbol cache
  UniqueConstants.clear();
  ConstantPoolMap.clear();
}

void EraVMAsmPrinter::emitConstantPool() {
  // use a custom constant pool emitter
  const MachineConstantPool *MCP = MF->getConstantPool();
  const std::vector<MachineConstantPoolEntry> &CP = MCP->getConstants();
  if (CP.empty())
    return;

  // Iterate over current function's constant pool and save the emit info,
  // and print the saved info at the very end.
  for (unsigned i = 0, e = CP.size(); i != e; ++i) {
    const MachineConstantPoolEntry &CPE = CP[i];
    const Constant *C = CPE.Val.ConstVal;

    MCSymbol *Sym = GetCPISymbol(i);

    // Insert the symbol to constant pool map
    auto result = std::find(UniqueConstants.begin(), UniqueConstants.end(), C);
    if (result == UniqueConstants.end()) {
      std::vector<MCSymbol *> symbol_vector;
      symbol_vector.push_back(Sym);
      ConstantPoolMap.insert({C, std::move(symbol_vector)});
      UniqueConstants.push_back(C);
    } else {
      auto m = ConstantPoolMap.find(C);
      assert(m != ConstantPoolMap.end());
      m->second.push_back(Sym);
    }
  }
}

void EraVMAsmPrinter::emitGlobalConstant(const DataLayout &DL,
                                         const Constant *CV,
                                         AliasMapTy *AliasList) {
  auto *Streamer =
      static_cast<EraVMTargetStreamer *>(OutStreamer->getTargetStreamer());

  if (const auto *CVA = dyn_cast<ConstantArray>(CV)) {
    auto *aty = CVA->getType();
    uint64_t elem_size = aty->getNumElements();
    // For now only support integer types.
    assert(aty->getElementType()->isIntegerTy(256));

    for (uint64_t i = 0; i < elem_size; ++i) {
      Constant *C = CVA->getAggregateElement(i);
      auto *CI = cast<ConstantInt>(C);
      assert(CI && CI->getBitWidth() == 256);
      Streamer->emitCell(CI->getValue());
    }
    return;
  }

  if (const auto *CDS = dyn_cast<ConstantDataSequential>(CV)) {
    size_t elem_size = CDS->getNumElements();
    assert(CDS->getElementByteSize() <= 256);

    for (size_t i = 0; i < elem_size; ++i) {
      APInt val = CDS->getElementAsAPInt(i);
      Streamer->emitCell(val);
    }
    return;
  }

  if (const auto *CVS = dyn_cast<ConstantStruct>(CV)) {
    StructType *sty = CVS->getType();
    assert(!sty->isPacked());
    uint64_t elem_size = sty->getNumElements();

    for (uint64_t i = 0; i < elem_size; ++i) {
      assert(sty->getTypeAtIndex(i) &&
             sty->getTypeAtIndex(i)->getIntegerBitWidth() <= 256);
      Constant *C = CVS->getAggregateElement(i);
      // TODO: CPR-920 support operators.
      const ConstantInt *CI = cast<ConstantInt>(C);
      Streamer->emitCell(CI->getValue());
    }

    return;
  }

  if (const auto *V = dyn_cast<ConstantVector>(CV)) {
    assert(false);
    return;
  }

  if (const auto *CE = dyn_cast<ConstantExpr>(CV))
    return;

  if (const auto *CI = dyn_cast<ConstantInt>(CV)) {
    Streamer->emitCell(CI->getValue());
    return;
  }

  if (isa<ConstantPointerNull>(CV)) {
    Streamer->emitCell(APInt::getZero(256));
    return;
  }

  AsmPrinter::emitGlobalConstant(DL, CV);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMAsmPrinter() {
  RegisterAsmPrinter<EraVMAsmPrinter> X(getTheEraVMTarget());
}
