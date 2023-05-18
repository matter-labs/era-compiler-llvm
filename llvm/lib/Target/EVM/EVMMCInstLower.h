//===----- EVMMCInstLower.h - Lower MachineInstr to MCInst ------*- C++ -*-===//
//
//
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
  typedef DenseMap<unsigned, unsigned> VRegMap;
  typedef DenseMap<const TargetRegisterClass *, VRegMap> VRegRCMap;

  const VRegRCMap &VRegMapping;
  const MachineRegisterInfo &MRI;

public:
  EVMMCInstLower(const VRegRCMap &VRegMapping, const MachineRegisterInfo &MRI)
      : VRegMapping(VRegMapping), MRI(MRI) {}

  void Lower(const MachineInstr *MI, MCInst &OutMI);

private:
  // Encodes the register class in the upper 4 bits along with the register
  // number in 28 lower bits.
  // Must be kept in sync with EVMInstPrinter::printRegName.
  // TODO: this can be removed once stackification is implemented.
  unsigned EncodeVReg(unsigned Reg);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMMCINSTLOWER_H
