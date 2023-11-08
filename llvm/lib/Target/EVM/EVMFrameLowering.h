//==----- EVMFrameLowering.h - Define frame lowering for EVM --*- C++ -*----==//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EVM_EVMFRAMELOWERING_H
#define LLVM_LIB_TARGET_EVM_EVMFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class EVMFrameLowering final : public TargetFrameLowering {
protected:
public:
  explicit EVMFrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsUp,
                            /*StackAlignment=*/Align(32),
                            /*LocalAreaOffset=*/0,
                            /*TransientStackAlignment=*/Align(32)) {}

  /// emitProlog/emitEpilog - These methods insert prolog and epilog code into
  /// the function.
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  bool hasFP(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EVM_EVMFRAMELOWERING_H
