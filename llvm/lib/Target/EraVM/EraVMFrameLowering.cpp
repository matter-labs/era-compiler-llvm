//===-- EraVMFrameLowering.cpp - EraVM Frame Information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
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
    TII.insertIncSP(MBB, MBBI, DL, NumCells);
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
  const DebugLoc &DL = I->getDebugLoc();

  if (!hasReservedCallFrame(MF)) {
    // If the stack pointer can be changed after prologue, turn the
    // adjcallstackup instruction into a 'sub SP, <amt>' and the
    // adjcallstackdown instruction into 'add SP, <amt>'
    uint64_t Amount = TII.getFrameSize(*I);
    if (Amount != 0) {
      // We need to keep the stack aligned properly.  To do this, we round the
      // amount of space needed for the outgoing arguments up to the next
      // alignment boundary.
      Amount = alignTo(Amount, getStackAlign());

      MachineInstr *New = nullptr;
      if (I->getOpcode() == TII.getCallFrameSetupOpcode()) {
        New = TII.insertIncSP(MBB, I, DL, Amount);
      } else {
        assert(I->getOpcode() == TII.getCallFrameDestroyOpcode());
        // factor out the amount the callee already popped.
        Amount -= TII.getFramePoppedByCallee(*I);
        if (Amount)
          New = TII.insertDecSP(MBB, I, DL, Amount);
      }

      if (New) {
        // The SRW implicit def is dead.
        New->implicit_operands().begin()->setIsDead();
      }
    }
  } else if (I->getOpcode() == TII.getCallFrameDestroyOpcode()) {
    // If we are performing frame pointer elimination and if the callee pops
    // something off the stack pointer, add it back.
    if (uint64_t CalleeAmt = TII.getFramePoppedByCallee(*I)) {
      MachineInstr *New = TII.insertIncSP(MBB, I, DL, CalleeAmt);
      // The SRW implicit def is dead.
      New->implicit_operands().begin()->setIsDead();
    }
  }

  return MBB.erase(I);
}

void EraVMFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *) const {}
