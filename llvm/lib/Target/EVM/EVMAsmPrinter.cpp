//===-------- EVMAsmPrinter.cpp - EVM LLVM assembly writer ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the EVM assembly language.
//
//===----------------------------------------------------------------------===//

#include "EVMMCInstLower.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMTargetMachine.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "MCTargetDesc/EVMTargetStreamer.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

extern cl::opt<bool> EVMKeepRegisters;

#define DEBUG_TYPE "asm-printer"

namespace {
class EVMAsmPrinter : public AsmPrinter {
  // For each register class we need the mapping from a virtual register
  // to its number.
  // TODO: Once stackification is implemented this should be removed.
  using VRegMap = DenseMap<unsigned, unsigned>;
  using VRegRCMap = DenseMap<const TargetRegisterClass *, VRegMap>;
  VRegRCMap VRegMapping;

  // Maps a linker symbol name to corresponding MCSymbol.
  StringSet<> WideRelocSymbolsSet;
  StringMap<unsigned> ImmutablesMap;

  // True if there is a function that pushes deploy address.
  bool ModuleHasPushDeployAddress = false;

  bool FirstFunctIsHandled = false;

public:
  EVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EVM Assembly "; }

  void emitInstruction(const MachineInstr *MI) override;

  void emitBasicBlockStart(const MachineBasicBlock &MBB) override;

  void emitFunctionEntryLabel() override;

  void emitEndOfAsmFile(Module &) override;

  void emitFunctionBodyStart() override;
  void emitFunctionBodyEnd() override;

private:
  void emitAssemblySymbol(const MachineInstr *MI);
  void emitWideRelocatableSymbol(const MachineInstr *MI);
  void emitLoadImmutableLabel(const MachineInstr *MI);
  void emitJumpDest();
};
} // end of anonymous namespace

void EVMAsmPrinter::emitFunctionEntryLabel() {
  AsmPrinter::emitFunctionEntryLabel();

  VRegMapping.clear();

  // Go through all virtual registers of the MF to establish the mapping
  // between the global virtual register number and the per-class virtual
  // register number (though we have only one GPR register class).
  // We use the per-class virtual register number in the asm output.
  // TODO: This is a temporary solution while EVM-backend outputs virtual
  // registers. Once stackification is implemented this should be removed.
  const MachineRegisterInfo &MRI = MF->getRegInfo();
  const unsigned NumVRs = MRI.getNumVirtRegs();
  for (unsigned I = 0; I < NumVRs; I++) {
    const Register Vr = Register::index2VirtReg(I);
    const TargetRegisterClass *RC = MRI.getRegClass(Vr);
    DenseMap<unsigned, unsigned> &VRegMap = VRegMapping[RC];
    const unsigned N = VRegMap.size();
    VRegMap.insert(std::make_pair(Vr, N + 1));
  }
}

void EVMAsmPrinter::emitFunctionBodyStart() {
  if (const auto *MFI = MF->getInfo<EVMMachineFunctionInfo>();
      MFI->getHasPushDeployAddress()) {
    // TODO: #778. Move the function with PUSHDEPLOYADDRESS to the
    // beginning of the module layout.
    if (FirstFunctIsHandled)
      report_fatal_error("Function with PUSHDEPLOYADDRESS isn't first.");

    assert(!ModuleHasPushDeployAddress &&
           "Multiple functions with PUSHDEPLOYADDRESS.");

    // Deploy address is represented by a PUSH20 instruction at the
    // start of the bytecode.
    MCInst MCI;
    MCI.setOpcode(EVM::PUSH20_S);
    MCI.addOperand(MCOperand::createImm(0));
    EmitToStreamer(*OutStreamer, MCI);
    ModuleHasPushDeployAddress = true;
  }
}

void EVMAsmPrinter::emitFunctionBodyEnd() { FirstFunctIsHandled = true; }

void EVMAsmPrinter::emitBasicBlockStart(const MachineBasicBlock &MBB) {
  AsmPrinter::emitBasicBlockStart(MBB);

  // Emit JUMPDEST instruction at the beginning of the basic block, if
  // this is not a block that is only reachable by fallthrough.
  if (!EVMKeepRegisters && !AsmPrinter::isBlockOnlyReachableByFallthrough(&MBB))
    emitJumpDest();
}

void EVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EVMMCInstLower MCInstLowering(OutContext, *this, VRegMapping,
                                MF->getRegInfo());

  switch (MI->getOpcode()) {
  default:
    break;
  case EVM::PseudoCALL: {
    // Generate push instruction with the address of a function.
    MCInst Push;
    Push.setOpcode(EVM::PUSH4_S);
    assert(MI->getOperand(0).isGlobal() &&
           "The first operand of PseudoCALL should be a GlobalValue.");

    // TODO: #745: Refactor EVMMCInstLower::Lower so we could use lowerOperand
    // instead of creating a MCOperand directly.
    MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
        getSymbol(MI->getOperand(0).getGlobal()), OutContext));
    Push.addOperand(MCOp);
    EmitToStreamer(*OutStreamer, Push);

    // Jump to a function.
    MCInst Jump;
    Jump.setOpcode(EVM::JUMP_S);
    EmitToStreamer(*OutStreamer, Jump);

    // In case a function has a return label, emit it, and also
    // emit a JUMPDEST instruction.
    if (MI->getNumExplicitOperands() > 1) {
      assert(MI->getOperand(1).isMCSymbol() &&
             "The second operand of PseudoCALL should be a MCSymbol.");
      OutStreamer->emitLabel(MI->getOperand(1).getMCSymbol());
      emitJumpDest();
    }
    return;
  }
  case EVM::PseudoRET: {
    // TODO: #746: Use PseudoInstExpansion and do this expansion in tblgen.
    MCInst Jump;
    Jump.setOpcode(EVM::JUMP_S);
    EmitToStreamer(*OutStreamer, Jump);
    return;
  }
  case EVM::PseudoJUMP:
  case EVM::PseudoJUMPI: {
    MCInst Push;
    Push.setOpcode(EVM::PUSH4_S);

    // TODO: #745: Refactor EVMMCInstLower::Lower so we could use lowerOperand
    // instead of creating a MCOperand directly.
    MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
        MI->getOperand(0).getMBB()->getSymbol(), OutContext));
    Push.addOperand(MCOp);
    EmitToStreamer(*OutStreamer, Push);

    MCInst Jump;
    Jump.setOpcode(MI->getOpcode() == EVM::PseudoJUMP ? EVM::JUMP_S
                                                      : EVM::JUMPI_S);
    EmitToStreamer(*OutStreamer, Jump);
    return;
  }
  case EVM::LINKERSYMBOL_S:
    emitWideRelocatableSymbol(MI);
    return;
  case EVM::DATASIZE_S:
  case EVM::DATAOFFSET_S:
    emitAssemblySymbol(MI);
    return;
  case EVM::LOADIMMUTABLE_S:
    emitLoadImmutableLabel(MI);
    return;
  }

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

// Lowers LOADIMMUTABLE_S as show below:
//   LOADIMMUTABLE_S @immutable_id
//    ->
//   __load_immutable__immutable_id.N:
//   PUSH32 0
//
// where N = 1 ... 'number of @immutable_id
// references in the current module'.
//
void EVMAsmPrinter::emitLoadImmutableLabel(const MachineInstr *MI) {
  assert(MI->getOpcode() == EVM::LOADIMMUTABLE_S);

  const MCSymbol *Symbol = MI->getOperand(0).getMCSymbol();
  StringRef ImmutableId = Symbol->getName();
  std::string LoadImmutableLabel =
      EVM::getLoadImmutableSymbol(ImmutableId, ++ImmutablesMap[ImmutableId]);
  if (OutContext.lookupSymbol(LoadImmutableLabel))
    report_fatal_error(Twine("MC: duplicating immutable label ") +
                       LoadImmutableLabel);

  MCSymbol *Sym = OutContext.getOrCreateSymbol(LoadImmutableLabel);
  // Emit load immutable label right before PUSH32 instruction.
  OutStreamer->emitLabel(Sym);

  MCInst MCI;
  MCI.setOpcode(EVM::PUSH32_S);
  MCI.addOperand(MCOperand::createImm(0));
  EmitToStreamer(*OutStreamer, MCI);
}

void EVMAsmPrinter::emitAssemblySymbol(const MachineInstr *MI) {
  MCSymbol *LinkerSymbol = MI->getOperand(0).getMCSymbol();
  StringRef LinkerSymbolName = LinkerSymbol->getName();
  unsigned Opc = MI->getOpcode();
  assert(Opc == EVM::DATASIZE_S || Opc == EVM::DATAOFFSET_S);

  std::string NameHash = EVM::getLinkerSymbolHash(LinkerSymbolName);
  std::string SymbolNameHash = (Opc == EVM::DATASIZE_S)
                                   ? EVM::getDataSizeSymbol(NameHash)
                                   : EVM::getDataOffsetSymbol(NameHash);

  MCInst MCI;
  MCI.setOpcode(EVM::PUSH4_S);
  MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
      SymbolNameHash, MCSymbolRefExpr::VariantKind::VK_EVM_DATA, OutContext));
  MCI.addOperand(MCOp);
  EmitToStreamer(*OutStreamer, MCI);
}

// Lowers LINKERSYMBOL_S instruction as shown below:
//
//   LINKERSYMBOL_S @BaseSymbol
//     ->
//   PUSH20_S @"__linker_symbol__$KECCAK256(BaseSymbol)$__"
//
//   .section ".symbol_name__$KECCAK256(BaseName)$__","S",@progbits
//          .ascii "~ \".%%^ [];,<.>?  .sol:GreaterHelper"

void EVMAsmPrinter::emitWideRelocatableSymbol(const MachineInstr *MI) {
  constexpr unsigned LinkerSymbolSize = 20;
  MCSymbol *BaseSymbol = MI->getOperand(0).getMCSymbol();
  StringRef BaseSymbolName = BaseSymbol->getName();
  std::string BaseSymbolNameHash = EVM::getLinkerSymbolHash(BaseSymbolName);
  std::string SymbolName = EVM::getLinkerSymbolName(BaseSymbolNameHash);

  MCInst MCI;
  // This represents a library address symbol which is 20-bytes wide.
  MCI.setOpcode(EVM::PUSH20_S);
  MCOperand MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
      BaseSymbolNameHash, MCSymbolRefExpr::VariantKind::VK_EVM_DATA,
      OutContext));
  MCI.addOperand(MCOp);

  if (!WideRelocSymbolsSet.contains(SymbolName)) {
    for (unsigned Idx = 0; Idx < LinkerSymbolSize / sizeof(uint32_t); ++Idx) {
      std::string SubSymName = EVM::getSymbolIndexedName(SymbolName, Idx);
      if (OutContext.lookupSymbol(SubSymName))
        report_fatal_error(Twine("MC: duplicating reference sub-symbol ") +
                           SubSymName);
    }
  }

  auto *TS = static_cast<EVMTargetStreamer *>(OutStreamer->getTargetStreamer());
  TS->emitWideRelocatableSymbol(MCI, SymbolName, LinkerSymbolSize);

  // The linker symbol and the related section already exist, so just exit.
  if (WideRelocSymbolsSet.contains(SymbolName))
    return;

  WideRelocSymbolsSet.insert(SymbolName);

  MCSection *CurrentSection = OutStreamer->getCurrentSectionOnly();

  // Emit the .symbol_name section that contains the actual symbol
  // name.
  std::string SymbolSectionName = EVM::getSymbolSectionName(SymbolName);
  MCSection *SymbolSection = OutContext.getELFSection(
      SymbolSectionName, ELF::SHT_PROGBITS, ELF::SHF_STRINGS);
  OutStreamer->switchSection(SymbolSection);
  OutStreamer->emitBytes(BaseSymbolName);
  OutStreamer->switchSection(CurrentSection);
}

void EVMAsmPrinter::emitEndOfAsmFile(Module &) {

  // The deploy and runtime code must end with INVALID instruction to
  // comply with 'solc'. To ensure this, we append an INVALID
  // instruction at the end of the .text section.
  OutStreamer->pushSection();
  OutStreamer->switchSection(OutContext.getObjectFileInfo()->getTextSection());
  MCInst MCI;
  MCI.setOpcode(EVM::INVALID_S);

  // Construct a local MCSubtargetInfo to use the streamer, as MachineFunction
  // is not available here. Since we're at the global level we can use the
  // default constructed subtarget.
  std::unique_ptr<MCSubtargetInfo> STI(TM.getTarget().createMCSubtargetInfo(
      TM.getTargetTriple().str(), TM.getTargetCPU(),
      TM.getTargetFeatureString()));

  OutStreamer->emitInstruction(MCI, *STI);
  OutStreamer->popSection();

  WideRelocSymbolsSet.clear();
  ImmutablesMap.clear();
  ModuleHasPushDeployAddress = false;
  FirstFunctIsHandled = false;
}

void EVMAsmPrinter::emitJumpDest() {
  MCInst JumpDest;
  JumpDest.setOpcode(EVM::JUMPDEST_S);
  EmitToStreamer(*OutStreamer, JumpDest);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMAsmPrinter() {
  const RegisterAsmPrinter<EVMAsmPrinter> X(getTheEVMTarget());
}
