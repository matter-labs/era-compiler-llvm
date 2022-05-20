//===-- SyncVMFrameLowering.cpp - SyncVM Frame Information ----------------===//
//
// This file contains the SyncVM implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "SyncVMFrameLowering.h"
#include "SyncVMInstrInfo.h"
#include "SyncVMMachineFunctionInfo.h"
#include "SyncVMSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

bool SyncVMFrameLowering::hasFP(const MachineFunction &MF) const {
  return false;
}

bool SyncVMFrameLowering::hasReservedCallFrame(
    const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

void SyncVMFrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  assert(&MF.front() == &MBB && "Shrink-wrapping not yet supported");
  MachineFrameInfo &MFI = MF.getFrameInfo();
  SyncVMMachineFunctionInfo *SyncVMFI = MF.getInfo<SyncVMMachineFunctionInfo>();
  const SyncVMInstrInfo &TII =
      *static_cast<const SyncVMInstrInfo *>(MF.getSubtarget().getInstrInfo());

  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // Get the number of bytes to allocate from the FrameInfo.
  uint64_t StackSize = MFI.getStackSize() - MFI.getMaxCallFrameSize();

  uint64_t NumCells = (StackSize - SyncVMFI->getCalleeSavedFrameSize()) / 32;

  // TODO: Handle callee saved registers once support them
  if (NumCells)
    BuildMI(MBB, MBBI, DL, TII.get(SyncVM::NOPSP)).addImm(NumCells).addImm(0);
}

void SyncVMFrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  SyncVMMachineFunctionInfo *SyncVMFI = MF.getInfo<SyncVMMachineFunctionInfo>();
  const SyncVMInstrInfo &TII =
      *static_cast<const SyncVMInstrInfo *>(MF.getSubtarget().getInstrInfo());

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL = MBBI->getDebugLoc();

  // Get the number of bytes to allocate from the FrameInfo
  uint64_t StackSize = MFI.getStackSize() - MFI.getMaxCallFrameSize();
  unsigned CSSize = SyncVMFI->getCalleeSavedFrameSize();
  uint64_t NumCells = (StackSize - CSSize) / 32;

  DL = MBBI->getDebugLoc();

  // TODO: Handle callee saved registers once support them
  // adjust stack pointer back: SP += numbytes
  if (NumCells)
    BuildMI(MBB, MBBI, DL, TII.get(SyncVM::NOPSP)).addImm(-NumCells).addImm(0);
}

// FIXME: Can we eleminate these in favour of generic code?
bool SyncVMFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  return true;
}

bool SyncVMFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  return true;
}

MachineBasicBlock::iterator SyncVMFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  const SyncVMInstrInfo &TII =
      *static_cast<const SyncVMInstrInfo *>(MF.getSubtarget().getInstrInfo());
  if (!hasReservedCallFrame(MF)) {
    // If the stack pointer can be changed after prologue, turn the
    // adjcallstackup instruction into a 'sub SP, <amt>' and the
    // adjcallstackdown instruction into 'add SP, <amt>'
    MachineInstr &Old = *I;
    uint64_t Amount = TII.getFrameSize(Old);
    if (Amount != 0) {
      // We need to keep the stack aligned properly.  To do this, we round the
      // amount of space needed for the outgoing arguments up to the next
      // alignment boundary.
      Amount = alignTo(Amount, getStackAlign());

      MachineInstr *New = nullptr;
      if (Old.getOpcode() == TII.getCallFrameSetupOpcode()) {
        New = BuildMI(MF, Old.getDebugLoc(), TII.get(SyncVM::NOPSP))
                  .addImm(Amount)
                  .addImm(0);
      } else {
        assert(Old.getOpcode() == TII.getCallFrameDestroyOpcode());
        // factor out the amount the callee already popped.
        Amount -= TII.getFramePoppedByCallee(Old);
        if (Amount)
          New = BuildMI(MF, Old.getDebugLoc(), TII.get(SyncVM::NOPSP))
                    .addImm(-Amount)
                    .addImm(0);
      }

      if (New) {
        // The SRW implicit def is dead.
        New->getOperand(3).setIsDead();

        // Replace the pseudo instruction with a new instruction...
        MBB.insert(I, New);
      }
    }
  } else if (I->getOpcode() == TII.getCallFrameDestroyOpcode()) {
    // If we are performing frame pointer elimination and if the callee pops
    // something off the stack pointer, add it back.
    if (uint64_t CalleeAmt = TII.getFramePoppedByCallee(*I)) {
      MachineInstr &Old = *I;
      MachineInstr *New = BuildMI(MF, Old.getDebugLoc(), TII.get(SyncVM::NOPSP))
                              .addImm(CalleeAmt)
                              .addImm(0);
      // The SRW implicit def is dead.
      New->getOperand(3).setIsDead();

      MBB.insert(I, New);
    }
  }

  return MBB.erase(I);
  return I;
}

void SyncVMFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *) const {}
