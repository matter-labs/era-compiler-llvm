#include "EVMStackHelpers.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"

#include <variant>

using namespace llvm;

StringRef llvm::getInstName(const MachineInstr *MI) {
  const MachineFunction *MF = MI->getParent()->getParent();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  return TII->getName(MI->getOpcode());
}

const Function *llvm::getCalledFunction(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isGlobal())
      continue;
    const Function *Func = dyn_cast<Function>(MO.getGlobal());
    if (Func != nullptr)
      return Func;
  }
  return nullptr;
}

std::string llvm::stackSlotToString(const StackSlot &Slot) {
  return std::visit(
      Overload{
          [](const FunctionCallReturnLabelSlot &Ret) -> std::string {
            return "RET[" +
                   std::string(getCalledFunction(*Ret.Call)->getName()) + "]";
          },
          [](const FunctionReturnLabelSlot &) -> std::string { return "RET"; },
          [](const VariableSlot &Var) -> std::string {
            SmallString<64> S;
            llvm::raw_svector_ostream OS(S);
            OS << printReg(Var.VirtualReg, nullptr, 0, nullptr);
            return std::string(S);
            ;
          },
          [](const LiteralSlot &Lit) -> std::string {
            SmallString<64> S;
            Lit.Value.toStringSigned(S);
            return std::string(S);
          },
          [](const TemporarySlot &Tmp) -> std::string {
            SmallString<128> S;
            llvm::raw_svector_ostream OS(S);
            OS << "TMP[" << getInstName(Tmp.MI) << ", ";
            OS << std::to_string(Tmp.Index) + "]";
            return std::string(S);
          },
          [](const JunkSlot &Junk) -> std::string { return "JUNK"; }},
      Slot);
  ;
}

std::string llvm::stackToString(Stack const &S) {
  std::string Result("[ ");
  for (auto const &Slot : S)
    Result += stackSlotToString(Slot) + ' ';
  Result += ']';
  return Result;
}
