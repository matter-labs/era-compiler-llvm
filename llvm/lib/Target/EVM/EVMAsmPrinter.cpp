//===-------- EVMAsmPrinter.cpp - EVM LLVM assembly writer ----------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the EVM assembly language.
//
//===----------------------------------------------------------------------===//

#include "EVMMCInstLower.h"
#include "EVMTargetMachine.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class EVMAsmPrinter : public AsmPrinter {
  // For each register class we need the mapping from a virtual register
  // to its number.
  // TODO: Once stackification is implemented this should be removed.
  typedef DenseMap<unsigned, unsigned> VRegMap;
  typedef DenseMap<const TargetRegisterClass *, VRegMap> VRegRCMap;
  VRegRCMap VRegMapping;

public:
  EVMAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "EVM Assembly "; }

  void emitInstruction(const MachineInstr *MI) override;

  void emitFunctionEntryLabel() override;
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
  unsigned int NumVRs = MRI.getNumVirtRegs();
  for (unsigned I = 0; I < NumVRs; I++) {
    Register Vr = Register::index2VirtReg(I);
    const TargetRegisterClass *RC = MRI.getRegClass(Vr);
    DenseMap<unsigned, unsigned> &VRegMap = VRegMapping[RC];
    unsigned N = VRegMap.size();
    VRegMap.insert(std::make_pair(Vr, N + 1));
  }
}

void EVMAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EVMMCInstLower MCInstLowering(OutContext, *this, VRegMapping,
                                MF->getRegInfo());

  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEVMAsmPrinter() {
  RegisterAsmPrinter<EVMAsmPrinter> X(getTheEVMTarget());
}
