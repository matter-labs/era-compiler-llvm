//===-- EraVMMovExpansion.cpp - EraVM MOV and PTR_MOV expansion -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass maps MOV and PTR_MOV instruction variants to ADD and PTR_ADD
// respectively, adding register R0 as `in1` operand.
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"

using namespace llvm;
using namespace EraVM;

#define DEBUG_TYPE "eravm-mov-expansion"
#define ERAVM_MOV_EXPANSION_NAME "EraVM expand MOV pseudo-instructions"

STATISTIC(NumMovExpanded, "Number of MOV instructions expanded to ADD");
STATISTIC(NumMovPtrExpanded, "Number of PTR_MOV instructions expanded to PTR_ADD");

namespace {

class EraVMMovExpansion : public MachineFunctionPass {
public:
  static char ID;
  EraVMMovExpansion() : MachineFunctionPass(ID) {
    initializeEraVMMovExpansionPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_MOV_EXPANSION_NAME; }

private:
  const TargetInstrInfo *TII{};
};

char EraVMMovExpansion::ID = 0;


  using InstructionDescr = std::tuple<unsigned, ArgumentType, ArgumentType, bool>;

// Maps opcode of MOV pseudo instruction to the opcodes of ADD pseudo instruction
// It is used to implement MOV -> ADD mapping
// For example, "MOVir_p src, dst" -> "ADDirr_p src, R0, dst"
  std::optional<InstructionDescr> getMovDescr(unsigned Opcode) {

#define CASE_MOV(t_in, t_out)                                                    \
  case MOV##t_in##t_out##_p:                                                     \
    return std::make_tuple(ADD##t_in##r##t_out##_p, t_in, t_out, false);

#define CASE_PTR_MOV(t_in, t_out)                                                \
  case PTR_MOV##t_in##t_out##_p:                                                 \
    return std::make_tuple(PTR_ADD##t_in##r##t_out##_p, t_in, t_out, true);

  const auto i = ArgumentType::Immediate;
  const auto r = ArgumentType::Register;
  const auto s = ArgumentType::Stack;
  const auto c = ArgumentType::Code;

  switch (Opcode) {
    CASE_MOV(i, r)
    CASE_MOV(i, s)
    CASE_MOV(c, r)
    CASE_MOV(s, s)
    CASE_MOV(s, r)
    CASE_MOV(r, s)
    CASE_MOV(r, r)

    CASE_PTR_MOV(s, s)
    CASE_PTR_MOV(s, r)
    CASE_PTR_MOV(r, s)
    CASE_PTR_MOV(r, r)
  default:
    return std::nullopt;
  }

#undef CASE_MOV
#undef CASE_PTR_MOV
  }

  bool TryConvertMovToAdd(TargetInstrInfo const &TII, MachineInstr &MI) {
    if (const auto MovDescr = getMovDescr(MI.getOpcode())) {
    const auto &[MovOpcode, MovIn0, MovOut0, IsPtrInstruction] = *MovDescr;

    LLVM_DEBUG(dbgs() << "Expanding: " << MI);

    MachineInstrBuilder MovBuilder =
        BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(), TII.get(MovOpcode));

    MovBuilder->setFlags(MI.getFlags());
    // This differentiates between instructions with stack and reg destinations
    if (MovOut0 == ArgumentType::Register) {
      auto &DestOp = MI.getOperand(0);
      assert(DestOp.isReg() && "Instruction malformed: the choice of opcode implies that the destination will be a register");
      MovBuilder.addReg(DestOp.getReg(), RegState::Define);
    }

    // `in0Iterator()` is not implemented for MOV and MOVptr instructions; it
    // requires putting more logic in `argumentType`. Therefore we

    const auto *In0Start = MI.operands_begin() + MI.getNumExplicitDefs();
    const auto *In0End = In0Start + argumentSize(MovIn0);
    const auto *End = MI.operands_end();

    EraVM::copyOperands(MovBuilder, In0Start, In0End);
    MovBuilder.addReg(EraVM::R0);
    EraVM::copyOperands(MovBuilder, In0End, End);
    MovBuilder.copyImplicitOps(MI);
    MovBuilder.cloneMemRefs(MI);

    { // Statistics
      LLVM_DEBUG(dbgs() << "Expanded into: " << *MovBuilder.getInstr());
      if (IsPtrInstruction) NumMovPtrExpanded++;
      else NumMovExpanded++;
    }
    return true;
    }
  return false;
  }

  } // namespace

  INITIALIZE_PASS(EraVMMovExpansion, DEBUG_TYPE, ERAVM_MOV_EXPANSION_NAME,
                  false, false)

  bool EraVMMovExpansion::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(
      dbgs() << "********** EraVM EXPAND MOV Pseudo INSTRUCTIONS **********\n"
             << "********** Function: " << MF.getName() << '\n');

  bool Changed = false;
  TII = MF.getSubtarget<EraVMSubtarget>().getInstrInfo();
  assert(TII && "TargetInstrInfo must be a valid object");

  std::vector<MachineInstr *> PseudoInstToErase;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (TryConvertMovToAdd(*TII, MI)) {
        PseudoInstToErase.push_back(&MI);
        Changed = true;
      }
    }
  std::for_each(PseudoInstToErase.cbegin(), PseudoInstToErase.cend(),
                [](auto *ins) { ins->eraseFromParent(); });

  return Changed;
}

/// createEraVMMovExpansionPass - returns an instance of the pseudo
/// instruction expansion pass.
FunctionPass *llvm::createEraVMMovExpansionPass() {
  return new EraVMMovExpansion();
}
