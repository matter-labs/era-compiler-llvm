//===----- EVMCalculateModuleSize.cpp - Calculate module size --*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A utility set for determining the overall size of a module.
//
//===----------------------------------------------------------------------===//

#include "EVMCalculateModuleSize.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Module.h"

using namespace llvm;

// This is the copied from AsmPrinter::isBlockOnlyReachableByFallthrough.
static bool isBlockOnlyReachableByFallthrough(const MachineBasicBlock *MBB) {
  // If this is a landing pad, it isn't a fall through.  If it has no preds,
  // then nothing falls through to it.
  if (MBB->isEHPad() || MBB->pred_empty())
    return false;

  // If there isn't exactly one predecessor, it can't be a fall through.
  if (MBB->pred_size() > 1)
    return false;

  // The predecessor has to be immediately before this block.
  MachineBasicBlock *Pred = *MBB->pred_begin();
  if (!Pred->isLayoutSuccessor(MBB))
    return false;

  // If the block is completely empty, then it definitely does fall through.
  if (Pred->empty())
    return true;

  // Check the terminators in the previous blocks
  for (const auto &MI : Pred->terminators()) {
    // If it is not a simple branch, we are in a table somewhere.
    if (!MI.isBranch() || MI.isIndirectBranch())
      return false;

    // If we are the operands of one of the branches, this is not a fall
    // through. Note that targets with delay slots will usually bundle
    // terminators with the delay slot instruction.
    for (ConstMIBundleOperands OP(MI); OP.isValid(); ++OP) {
      if (OP->isJTI())
        return false;
      if (OP->isMBB() && OP->getMBB() == MBB)
        return false;
    }
  }

  return true;
}

// Return the size of an instruction in bytes. For some pseudo instructions,
// don't use the size from the instruction description, since during code
// generation, some instructions will be relaxed to smaller instructions.
static unsigned getInstSize(const MachineInstr &MI,
                            const TargetInstrInfo *TII) {
  // Skip debug instructions.
  if (MI.isDebugInstr())
    return 0;

  unsigned Size = 0;
  switch (MI.getOpcode()) {
  default:
    Size = MI.getDesc().getSize();
    break;
  case EVM::PseudoCALL:
    // In case that function call has a return label, we will emit JUMPDEST,
    // so take it into account.
    if (MI.getNumExplicitOperands() > 1)
      Size = TII->get(EVM::JUMPDEST_S).getSize();
    LLVM_FALLTHROUGH;
  case EVM::PseudoJUMPI:
  case EVM::PseudoJUMP:
    // We emit PUSH4_S here. The linker usually relaxes it to PUSH2_S,
    // since a 16-bit immediate covers the 24,576-byte EVM runtime code cap
    // (EIP-170). If a wider immediate were ever required, the contract
    // already exceeds the cap, so the push width is moot.
    Size += TII->get(EVM::PUSH2_S).getSize() + TII->get(EVM::JUMP_S).getSize();
    break;
  case EVM::PUSH_FRAME:
    // Typical frame index offsets can be encoded in a single byte, but to
    // be conservative, let’s assume 2 bytes per offset.
    LLVM_FALLTHROUGH;
  case EVM::PUSH_LABEL:
    // We emit PUSH4_S here. The linker usually relaxes it to PUSH2_S,
    // since a 16-bit immediate covers the 24,576-byte EVM runtime code cap
    // (EIP-170). If a wider immediate were ever required, the contract
    // already exceeds the cap, so the push width is moot.
    Size = TII->get(EVM::PUSH2_S).getSize();
    break;
  }
  return Size;
}

uint64_t llvm::EVM::calculateFunctionCodeSize(const MachineFunction &MF) {
  uint64_t Size = 0;
  const auto *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();

  // If the function has a PUSHDEPLOYADDRESS, it starts with a PUSH20.
  if (const auto *MFI = MF.getInfo<EVMMachineFunctionInfo>();
      MFI->getHasPushDeployAddress())
    Size += TII->get(EVM::PUSH20).getSize();

  for (const MachineBasicBlock &MBB : MF) {
    // If the block is not only reachable by fallthrough, it starts with
    // a JUMPDEST instruction.
    if (!isBlockOnlyReachableByFallthrough(&MBB))
      Size += TII->get(EVM::JUMPDEST_S).getSize();

    Size += std::accumulate(MBB.begin(), MBB.end(), 0,
                            [&TII](unsigned Sum, const MachineInstr &MI) {
                              return Sum + getInstSize(MI, TII);
                            });
  }
  return Size;
}

static uint64_t calculateReadOnlyDataSize(const Module &M) {
  uint64_t Size = 0;
  for (const GlobalVariable &GV : M.globals()) {
    if (GV.getAddressSpace() != EVMAS::AS_CODE || !GV.hasInitializer())
      continue;

    if (const auto *CV = dyn_cast<ConstantDataSequential>(GV.getInitializer()))
      Size += CV->getRawDataValues().size();
  }
  return Size;
}

uint64_t llvm::EVM::calculateModuleCodeSize(Module &M,
                                            const MachineModuleInfo &MMI) {
  uint64_t TotalSize = 0;
  for (Function &F : M) {
    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;
    TotalSize += llvm::EVM::calculateFunctionCodeSize(*MF);
  }

  // Take into account the read-only data that we append to the .text section.
  TotalSize += calculateReadOnlyDataSize(M);
  // Take into account INVALID instruction at the end of the .text section.
  TotalSize++;
  return TotalSize;
}
