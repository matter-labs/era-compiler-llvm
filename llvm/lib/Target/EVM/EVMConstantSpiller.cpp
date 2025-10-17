//===----- EVMConstantSpiller.cpp - Spill constants to memory --*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "EVMConstantSpiller.h"
#include "EVMInstrInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Module.h"

using namespace llvm;

#define DEBUG_TYPE "evm-spill-constants"

constexpr uint64_t SpillSlotSize = 32;

static cl::opt<unsigned> ConstantSpillThreshold(
    "evm-constant-spill-threshold", cl::Hidden, cl::init(30),
    cl::desc("Minimum number of uses of a constant across the module required "
             "before spilling it to memory is considered profitable"));

static MachineInstr *emitPush(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator InsertBefore,
                              const EVMInstrInfo *TII, const APInt &Imm,
                              LLVMContext &Ctx, const DebugLoc &DL) {
  unsigned Opc = EVM::getPUSHOpcode(Imm);
  auto MI = BuildMI(MBB, InsertBefore, DL, TII->get(EVM::getStackOpcode(Opc)));
  if (Opc != EVM::PUSH0)
    MI.addCImm(ConstantInt::get(Ctx, Imm));
  return MI;
}

uint64_t EVMConstantSpiller::getSpillSize() const {
  return ConstantUseCount.size() * SpillSlotSize;
}

void EVMConstantSpiller::emitConstantSpills(uint64_t SpillOffset,
                                            MachineFunction &EntryMF) {
  LLVMContext &Ctx = EntryMF.getFunction().getContext();
  const EVMInstrInfo *TII = EntryMF.getSubtarget<EVMSubtarget>().getInstrInfo();

  DenseMap<APInt, uint64_t> ConstantToSpillOffset;
  for (const auto &KV : ConstantUseCount) {
    ConstantToSpillOffset[KV.first] = SpillOffset;
    SpillOffset += SpillSlotSize;
  }

  // Emit constant stores in prologue of the '__entry' function.
  MachineBasicBlock &SpillMBB = EntryMF.front();
  for (const auto &[Imm, Offset] : ConstantToSpillOffset) {
    LLVM_DEBUG({
      dbgs() << "Spilling constant: " << Imm
             << ", number of uses: " << ConstantUseCount.at(Imm)
             << ", at offset: " << Offset << '\n';
    });

    BuildMI(SpillMBB, SpillMBB.begin(), DebugLoc(), TII->get(EVM::MSTORE_S));
    emitPush(SpillMBB, SpillMBB.begin(), TII, APInt(256, Offset), Ctx,
             DebugLoc());
    emitPush(SpillMBB, SpillMBB.begin(), TII, Imm, Ctx, DebugLoc());
  }

  // Reload spilled constants.
  for (MachineInstr *MI : ReloadCandidates) {
    const APInt Imm = MI->getOperand(0).getCImm()->getValue().zext(256);
    uint64_t Offset = ConstantToSpillOffset.at(Imm);

    MachineBasicBlock *MBB = MI->getParent();
    emitPush(*MBB, MI, TII, APInt(256, Offset), Ctx, MI->getDebugLoc());
    auto Load = BuildMI(*MBB, MI, MI->getDebugLoc(), TII->get(EVM::MLOAD_S));
    Load->setAsmPrinterFlag(MachineInstr::ReloadReuse);
    MI->eraseFromParent();
  }
}

static bool shouldSkip(const MachineInstr &MI) {
  if (!EVMInstrInfo::isPush(&MI) || (MI.getOpcode() == EVM::PUSH0_S))
    return true;

  const APInt Imm = MI.getOperand(0).getCImm()->getValue();
  return Imm.getActiveBits() < 8 * 8;
}

void EVMConstantSpiller::analyzeModule(Module &M, MachineModuleInfo &MMI) {
  for (Function &F : M) {
    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;

    for (MachineBasicBlock &MBB : *MF) {
      for (MachineInstr &MI : MBB) {
        if (shouldSkip(MI))
          continue;

        const APInt Imm = MI.getOperand(0).getCImm()->getValue().zext(256);
        if (Imm.isAllOnes())
          continue;

        ConstantUseCount[Imm]++;
        ReloadCandidates.push_back(&MI);
      }
    }
  }
}

void EVMConstantSpiller::filterCandidates() {
  SmallVector<APInt> ImmToRemove;
  for (const auto &[Imm, NumUses] : ConstantUseCount)
    if (NumUses < ConstantSpillThreshold)
      ImmToRemove.push_back(Imm);

  for (const APInt &Imm : ImmToRemove)
    ConstantUseCount.erase(Imm);

  erase_if(ReloadCandidates, [this](const MachineInstr *MI) {
    const APInt &Imm = MI->getOperand(0).getCImm()->getValue().zext(256);
    return !ConstantUseCount.contains(Imm);
  });
}

EVMConstantSpiller::EVMConstantSpiller(Module &M, MachineModuleInfo &MMI) {
  analyzeModule(M, MMI);
  filterCandidates();
}
