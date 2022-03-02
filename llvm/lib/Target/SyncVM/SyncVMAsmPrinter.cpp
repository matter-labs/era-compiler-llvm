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
  std::vector<std::pair<MCSymbol*, const Constant*>> ConstantPool;
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
  void emitEndOfAsmFile	(Module &) override;

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

  for (auto & pair : ConstantPool) {
    OutStreamer->emitLabel(pair.first);
    emitGlobalConstant(getDataLayout(), pair.second);
  }
}

void SyncVMAsmPrinter::emitConstantPool() {
  // use a custom constant pool emitter
  const MachineConstantPool *MCP = MF->getConstantPool();
  const std::vector<MachineConstantPoolEntry> &CP = MCP->getConstants();
  if (CP.empty()) return;

  // Iterate over current function's constant pool and save the emit info,
  // and print the saved info at the very end.
  for (unsigned i = 0, e = CP.size(); i != e; ++i) {
    const MachineConstantPoolEntry &CPE = CP[i];
    const Constant *C = CPE.Val.ConstVal;

    MCSymbol *Sym = GetCPISymbol(i);
    ConstantPool.emplace_back(std::make_pair(Sym, C));
  }
}

void SyncVMAsmPrinter::emitGlobalConstant(const DataLayout &DL,
                                          const Constant *CV) {
  const ConstantInt *CI = cast<ConstantInt>(CV);
  assert(CI->getBitWidth() == 256);
  auto *Streamer =
      static_cast<SyncVMTargetStreamer *>(OutStreamer->getTargetStreamer());
  Streamer->emitGlobalConst(CI->getValue());
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSyncVMAsmPrinter() {
  RegisterAsmPrinter<SyncVMAsmPrinter> X(getTheSyncVMTarget());
}
