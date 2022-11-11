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
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class EraVMAsmPrinter : public AsmPrinter {
public:
  EraVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EraVM Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void emitInstruction(const MachineInstr *MI) override;
  using AliasMapTy = DenseMap<uint64_t, SmallVector<const GlobalAlias *, 1>>;
  void emitGlobalConstant(const DataLayout &DL, const Constant *CV,
                          AliasMapTy *AliasList = nullptr) override;
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

void EraVMAsmPrinter::emitGlobalConstant(const DataLayout &DL,
                                         const Constant *CV,
                                         AliasMapTy *AliasList) {
  const ConstantInt *CI = cast<ConstantInt>(CV);
  auto *Streamer =
      static_cast<EraVMTargetStreamer *>(OutStreamer->getTargetStreamer());
  Streamer->emitGlobalConst(CI->getValue());
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEraVMAsmPrinter() {
  RegisterAsmPrinter<EraVMAsmPrinter> X(getTheEraVMTarget());
}
