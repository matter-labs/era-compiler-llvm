//==- SyncVMFrameLowering.h - Define frame lowering for SyncVM --*- C++ -*--==//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SYNCVM_SYNCVMFRAMELOWERING_H
#define LLVM_LIB_TARGET_SYNCVM_SYNCVMFRAMELOWERING_H

#include "SyncVM.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class SyncVMFrameLowering : public TargetFrameLowering {
protected:

public:
  explicit SyncVMFrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsUp, Align(32), 0,
                            Align(32)) {}

  /// emitProlog/emitEpilog - These methods insert prolog and epilog code into
  /// the function.
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;
  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override;

  bool hasFP(const MachineFunction &MF) const override;
  bool hasReservedCallFrame(const MachineFunction &MF) const override;
  void processFunctionBeforeFrameFinalized(MachineFunction &MF,
                                     RegScavenger *RS = nullptr) const override;
};

} // End llvm namespace

#endif
