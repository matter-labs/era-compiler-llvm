//===-- SyncVMAsmPrinter.cpp - SyncVM LLVM assembly writer ----------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the SyncVM assembly language.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SyncVMInstPrinter.h"
#include "MCTargetDesc/SyncVMTargetStreamer.h"
#include "SyncVM.h"
#include "SyncVMInstrInfo.h"
#include "SyncVMMCInstLower.h"
#include "SyncVMTargetMachine.h"
#include "TargetInfo/SyncVMTargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class SyncVMAsmPrinter : public AsmPrinter {

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
  SyncVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "SyncVM Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void PrintSymbolOperand(const MachineOperand &MO, raw_ostream &O) override;
  void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &O,
                    const char *Modifier = nullptr);
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &O) override;
  void emitInstruction(const MachineInstr *MI) override;
  void emitGlobalConstant(const DataLayout &DL, const Constant *CV) override;
  void EmitInterruptVectorSection(MachineFunction &ISR);

  void emitConstantPool() override;
  void emitEndOfAsmFile(Module &) override;
};
} // end of anonymous namespace

void SyncVMAsmPrinter::PrintSymbolOperand(const MachineOperand &MO,
                                          raw_ostream &O) {
  uint64_t Offset = MO.getOffset();
  if (Offset)
    O << '(' << Offset << '+';
  getSymbol(MO.getGlobal())->print(O, MAI);
  if (Offset)
    O << ')';
}

void SyncVMAsmPrinter::printOperand(const MachineInstr *MI, int OpNum,
                                    raw_ostream &O, const char *Modifier) {
  const MachineOperand &MO = MI->getOperand(OpNum);
  switch (MO.getType()) {
  default:
    llvm_unreachable("Not implemented yet!");
  case MachineOperand::MO_Register:
    O << SyncVMInstPrinter::getRegisterName(MO.getReg());
    return;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, O);
    return;
  }
}

/// PrintAsmOperand - Print out an operand for an inline asm expression.
///
bool SyncVMAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode, raw_ostream &O) {
  llvm_unreachable("Not implemented yet!");
  return false;
}

bool SyncVMAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                             unsigned OpNo,
                                             const char *ExtraCode,
                                             raw_ostream &O) {
  llvm_unreachable("Not implemented yet!");
  return false;
}

//===----------------------------------------------------------------------===//
void SyncVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  SyncVMMCInstLower MCInstLowering(OutContext, *this);

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

void SyncVMAsmPrinter::EmitInterruptVectorSection(MachineFunction &ISR) {
  llvm_unreachable("Not implemented yet!");
}

bool SyncVMAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

void SyncVMAsmPrinter::emitEndOfAsmFile(Module &) {
  MCSection *ReadOnlySection =
      OutContext.getELFSection(".rodata", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);

  OutStreamer->SwitchSection(ReadOnlySection);

  // combine labels pointing to same constants
  for (const Constant *C : UniqueConstants) {
    std::vector<MCSymbol *> symbols = ConstantPoolMap[C];

    for (auto symbol : symbols) {
      OutStreamer->emitLabel(symbol);
    }
    // then print constant:
    emitGlobalConstant(getDataLayout(), C);
  }
}

void SyncVMAsmPrinter::emitConstantPool() {
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

void SyncVMAsmPrinter::emitGlobalConstant(const DataLayout &DL,
                                          const Constant *CV) {
  auto *Streamer =
      static_cast<SyncVMTargetStreamer *>(OutStreamer->getTargetStreamer());

  if (const ConstantArray *CVA = dyn_cast<ConstantArray>(CV)) {
    auto aty = CVA->getType();
    uint64_t elem_size = aty->getNumElements();
    Type *elem_type = aty->getElementType();
    // For now only support integer types.
    assert(elem_type->isIntegerTy(256));

    for (uint64_t i = 0; i < elem_size; ++i) {
      Constant *C = CVA->getAggregateElement(i);
      ConstantInt *CI = cast<ConstantInt>(C);
      assert(CI && CI->getBitWidth() == 256);
      Streamer->emitGlobalConst(CI->getValue());
    }
    return;
  }

  if (const ConstantDataSequential *CDS =
          dyn_cast<ConstantDataSequential>(CV)) {
    size_t elem_size = CDS->getNumElements();
    size_t elem_ty_size = CDS->getElementByteSize();
    assert(elem_ty_size <= 256);

    for (size_t i = 0; i < elem_size; ++i) {
      APInt val = CDS->getElementAsAPInt(i);
      Streamer->emitGlobalConst(val);
    }
    return;
  }

  if (const ConstantStruct *CVS = dyn_cast<ConstantStruct>(CV)) {
    StructType *sty = CVS->getType();
    assert(!sty->isPacked());
    uint64_t elem_size = sty->getNumElements();

    for (uint64_t i = 0; i < elem_size; ++i) {
      Type *ty = sty->getTypeAtIndex(i);
      assert(ty->isIntegerTy() && ty->getIntegerBitWidth() <= 256);
      Constant *C = CVS->getAggregateElement(i);
      const ConstantInt *CI = cast<ConstantInt>(C);
      Streamer->emitGlobalConst(CI->getValue());
    }

    return;
  }

  if (const ConstantVector *V = dyn_cast<ConstantVector>(CV)) {
    assert(false);
    return;
  }

  if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
    printf("ConstantExpr for type\n");
    return;
  }

  if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
    assert(CI->getBitWidth() == 256);
    Streamer->emitGlobalConst(CI->getValue());
    return;
  }

  AsmPrinter::emitGlobalConstant(DL, CV);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMAsmPrinter() {
  RegisterAsmPrinter<SyncVMAsmPrinter> X(getTheSyncVMTarget());
}
