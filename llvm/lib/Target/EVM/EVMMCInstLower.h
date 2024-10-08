//===----- EVMMCInstLower.h - Lower MachineInstr to MCInst ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMMCINSTLOWER_H
#define LLVM_LIB_TARGET_EVM_EVMMCINSTLOWER_H

#include "llvm/ADT/DenseMap.h"

namespace llvm {
class AsmPrinter;
class MCContext;
class MCInst;
class MCOperand;
class MCSymbol;
class MachineInstr;
class MachineOperand;
class MachineRegisterInfo;
class TargetRegisterClass;

/// EVMMCInstLower - This class is used to lower an MachineInstr
/// into an MCInst.
class LLVM_LIBRARY_VISIBILITY EVMMCInstLower {
  // TODO: Once stackification is implemented this should be removed,
  // see the comments in EVMAsmPrinter.
  using VRegMap = DenseMap<unsigned, unsigned>;
  using VRegRCMap = DenseMap<const TargetRegisterClass *, VRegMap>;

  MCContext &Ctx;
  AsmPrinter &Printer;
  const VRegRCMap &VRegMapping;
  const MachineRegisterInfo &MRI;

public:
  EVMMCInstLower(MCContext &Ctx, AsmPrinter &Printer,
                 const VRegRCMap &VRegMapping, const MachineRegisterInfo &MRI)
      : Ctx(Ctx), Printer(Printer), VRegMapping(VRegMapping), MRI(MRI) {}

  void Lower(const MachineInstr *MI, MCInst &OutMI);

private:
  // Encodes the register class in the upper 4 bits along with the register
  // number in 28 lower bits.
  // Must be kept in sync with EVMInstPrinter::printRegName.
  // TODO: this can be removed once stackification is implemented.
  unsigned EncodeVReg(unsigned Reg);

  MCOperand LowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym) const;
  MCSymbol *GetGlobalAddressSymbol(const MachineOperand &MO) const;
  MCSymbol *GetExternalSymbolSymbol(const MachineOperand &MO) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMMCINSTLOWER_H
