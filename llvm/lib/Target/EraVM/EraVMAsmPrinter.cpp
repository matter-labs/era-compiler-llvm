//===-- EraVMAsmPrinter.cpp - EraVM LLVM assembly writer ------------------===//
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
#include "llvm/ADT/DenseMap.h"
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
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class EraVMAsmPrinter : public AsmPrinter {
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
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EraVM Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void emitInstruction(const MachineInstr *MI) override;
  using AliasMapTy = DenseMap<uint64_t, SmallVector<const GlobalAlias *, 1>>;
  void emitGlobalConstant(const DataLayout &DL, const Constant *CV,
                          AliasMapTy *AliasList = nullptr) override;

  void emitConstantPool() override;
  void emitEndOfAsmFile(Module &) override;
};
} // end of anonymous namespace

//===----------------------------------------------------------------------===//
void EraVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EraVMMCInstLower MCInstLowering(OutContext, *this);

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
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

    for (auto symbol : symbols) {
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

  if (const ConstantArray *CVA = dyn_cast<ConstantArray>(CV)) {
    auto aty = CVA->getType();
    uint64_t elem_size = aty->getNumElements();
    // For now only support integer types.
    assert(aty->getElementType()->isIntegerTy(256));

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
    assert(CDS->getElementByteSize() <= 256);

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
      assert(sty->getTypeAtIndex(i) &&
             sty->getTypeAtIndex(i)->getIntegerBitWidth() <= 256);
      Constant *C = CVS->getAggregateElement(i);
      // TODO: CPR-920 support operators.
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
    Streamer->emitGlobalConst(CI->getValue());
    return;
  }

  AsmPrinter::emitGlobalConstant(DL, CV);
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMAsmPrinter() {
  RegisterAsmPrinter<EraVMAsmPrinter> X(getTheEraVMTarget());
}
