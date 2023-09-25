//===-- EraVMFrameLowering.cpp - EraVM Frame Information ------------------===//
//
// This file contains the EraVM implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "EraVMFrameLowering.h"
#include "EraVMInstrInfo.h"
#include "EraVMMachineFunctionInfo.h"
#include "EraVMSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

bool EraVMFrameLowering::hasFP(const MachineFunction &MF) const {
  return false;
}

bool EraVMFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

void EraVMFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  assert(&MF.front() == &MBB && "Shrink-wrapping not yet supported");
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const EraVMInstrInfo &TII =
      *static_cast<const EraVMInstrInfo *>(MF.getSubtarget().getInstrInfo());

  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  uint64_t NumCells = MFI.getStackSize() / 32;

  if (NumCells)
    BuildMI(MBB, MBBI, DL, TII.get(EraVM::NOPSP)).addImm(NumCells).addImm(0);
}

void EraVMFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  // Ret instruction restores SP to whatever is was before a near or a far
  // call, so no epilogue is needed.
}

MachineBasicBlock::iterator EraVMFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  const EraVMInstrInfo &TII =
      *static_cast<const EraVMInstrInfo *>(MF.getSubtarget().getInstrInfo());
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
        New = BuildMI(MF, Old.getDebugLoc(), TII.get(EraVM::NOPSP))
                  .addImm(Amount)
                  .addImm(0);
      } else {
        assert(Old.getOpcode() == TII.getCallFrameDestroyOpcode());
        // factor out the amount the callee already popped.
        Amount -= TII.getFramePoppedByCallee(Old);
        if (Amount)
          New = BuildMI(MF, Old.getDebugLoc(), TII.get(EraVM::NOPSP))
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
      MachineInstr *New = BuildMI(MF, Old.getDebugLoc(), TII.get(EraVM::NOPSP))
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

void EraVMFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *) const {}
