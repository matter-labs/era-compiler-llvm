//==- EraVMFrameLowering.h - Define frame lowering for EraVM ----*- C++ -*--==//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ERAVM_ERAVMFRAMELOWERING_H
#define LLVM_LIB_TARGET_ERAVM_ERAVMFRAMELOWERING_H

#include "EraVM.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class EraVMFrameLowering : public TargetFrameLowering {
protected:
public:
  explicit EraVMFrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsUp, Align(32), 0,
                            Align(32)) {}

  /// emitProlog/emitEpilog - These methods insert prolog and epilog code into
  /// the function.
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  bool hasFP(const MachineFunction &MF) const override;
  bool hasReservedCallFrame(const MachineFunction &MF) const override;
  void processFunctionBeforeFrameFinalized(
      MachineFunction &MF, RegScavenger *RS = nullptr) const override;
};

} // namespace llvm

#endif
