//===----- EVMConstantSpiller.cpp - Spill constants to memory --*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file identifies CONSTANT_RELOAD instructions representing spilled
// constants throughout the module. It spills constants at the start of the
// entry function and replaces CONSTANT_RELOAD with the corresponding reload
// instructions.
//
//===----------------------------------------------------------------------===//

#include "EVMConstantSpiller.h"
#include "EVMInstrInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "evm-spill-constants"

constexpr uint64_t SpillSlotSize = 32;

uint64_t EVMConstantSpiller::getSpillSize() const {
  return ConstantToUseCount.size() * SpillSlotSize;
}

void EVMConstantSpiller::emitSpills(uint64_t SpillOffset,
                                    MachineFunction &EntryMF) {
  const EVMInstrInfo *TII = EntryMF.getSubtarget<EVMSubtarget>().getInstrInfo();

  DenseMap<APInt, uint64_t> ConstantToSpillOffset;
  for (const auto &KV : ConstantToUseCount) {
    ConstantToSpillOffset[KV.first] = SpillOffset;
    SpillOffset += SpillSlotSize;
  }

  // Emit constant stores in prologue of the entry function.
  MachineBasicBlock &SpillMBB = EntryMF.front();
  for (const auto &[Imm, Offset] : ConstantToSpillOffset) {
    LLVM_DEBUG({
      dbgs() << "Spilling constant: " << Imm
             << ", number of uses: " << ConstantToUseCount.at(Imm)
             << ", at offset: " << Offset << '\n';
    });

    // Push the constant
    TII->insertPush(Imm, SpillMBB, SpillMBB.begin(), DebugLoc());
    // Push the offset
    TII->insertPush(APInt(256, Offset), SpillMBB, std::next(SpillMBB.begin()),
                    DebugLoc());
    BuildMI(SpillMBB, std::next(SpillMBB.begin(), 2), DebugLoc(),
            TII->get(EVM::MSTORE_S));
  }

  // Reload spilled constants.
  for (MachineInstr *MI : Reloads) {
    const APInt Imm = MI->getOperand(0).getCImm()->getValue().zext(256);
    uint64_t Offset = ConstantToSpillOffset.at(Imm);

    MachineBasicBlock *MBB = MI->getParent();
    TII->insertPush(APInt(256, Offset), *MBB, MI, MI->getDebugLoc());
    auto Load = BuildMI(*MBB, MI, MI->getDebugLoc(), TII->get(EVM::MLOAD_S));
    Load->setAsmPrinterFlag(MachineInstr::ReloadReuse);
    MI->eraseFromParent();
  }
}

EVMConstantSpiller::EVMConstantSpiller(SmallVector<MachineFunction *> &MFs) {
  for (MachineFunction *MF : MFs) {
    for (MachineBasicBlock &MBB : *MF) {
      for (MachineInstr &MI : MBB) {
        if (MI.getOpcode() != EVM::CONSTANT_RELOAD)
          continue;

        const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
        ConstantToUseCount[Imm]++;
        Reloads.push_back(&MI);
      }
    }
  }
}
