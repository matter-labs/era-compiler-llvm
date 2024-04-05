//===-- EraVMFoldImm.cpp - Fold imm -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "EraVM.h"

#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

#include "EraVMInstrInfo.h"
#include "EraVMSubtarget.h"
#include "MCTargetDesc/EraVMMCTargetDesc.h"

using namespace llvm;

#define DEBUG_TYPE "eravm-fold-imm"
#define ERAVM_FOLD_IMM_NAME "EraVM fold imm"

namespace {

class EraVMFoldImm : public MachineFunctionPass {
public:
  static char ID;
  EraVMFoldImm() : MachineFunctionPass(ID) {
    initializeEraVMFoldImmPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return ERAVM_FOLD_IMM_NAME; }

private:
  const EraVMInstrInfo *TII{};
  MachineRegisterInfo *RegInfo{};

  bool tryFoldImm(MachineBasicBlock &MBB);
};

char EraVMFoldImm::ID = 0;
} // namespace

INITIALIZE_PASS(EraVMFoldImm, DEBUG_TYPE, ERAVM_FOLD_IMM_NAME, false, false)

bool EraVMFoldImm::tryFoldImm(MachineBasicBlock &MBB) {
  SmallVector<MachineInstr *, 16> ToRemove;
  SmallPtrSet<MachineInstr *, 16> UsesToUpdate;
  MachineFunction &MF = *MBB.getParent();

  for (auto &MI : MBB) {
    if (MI.getOpcode() != EraVM::LOADCONST && MI.getOpcode() != EraVM::MOVEIMM)
      continue;

    bool IsMoveImmNeg = MI.getOpcode() == EraVM::MOVEIMM &&
                        MI.getOperand(1).getCImm()->isNegative();

    // Skip combining sub with its uses in optsize mode. For this to work, we
    // need to emit constant pool entry for negative values, which increases
    // code size.
    if (IsMoveImmNeg && MBB.getParent()->getFunction().hasOptSize())
      continue;

    Register OutReg = MI.getOperand(0).getReg();
    if (any_of(RegInfo->use_nodbg_instructions(OutReg),
               [this, &UsesToUpdate](const MachineInstr &UseMI) {
                 return UsesToUpdate.count(&UseMI) ||
                        !EraVM::hasRRInAddressingMode(UseMI) ||
                        TII->getCCCode(UseMI) != EraVMCC::COND_NONE;
               }))
      continue;

    ToRemove.emplace_back(&MI);
    SmallVector<MachineOperand, 2> In0Ops;
    if (IsMoveImmNeg) {
      MachineConstantPool *ConstantPool = MF.getConstantPool();
      const Constant *C =
          ConstantInt::get(MF.getFunction().getContext(),
                           MI.getOperand(1).getCImm()->getValue());
      unsigned Idx = ConstantPool->getConstantPoolIndex(C, Align(32));
      In0Ops.emplace_back(MachineOperand::CreateImm(0));
      In0Ops.emplace_back(MachineOperand::CreateCPI(Idx, 0));
    } else {
      if (MI.getOpcode() == EraVM::LOADCONST)
        In0Ops.emplace_back(MachineOperand::CreateImm(0));
      In0Ops.emplace_back(MI.getOperand(1));
    }

    for (auto &UseMI : RegInfo->use_nodbg_instructions(OutReg)) {
      int NewOpcode = MI.getOpcode() == EraVM::LOADCONST || IsMoveImmNeg
                          ? EraVM::getWithCRInAddrMode(UseMI.getOpcode())
                          : EraVM::getWithIRInAddrMode(UseMI.getOpcode());
      EraVM::ArgumentKind ArgNo = EraVM::ArgumentKind::In0;

      if (EraVM::in0Iterator(UseMI)->getReg() != OutReg) {
        ArgNo = EraVM::ArgumentKind::In1;
        if (!UseMI.isCommutable())
          NewOpcode = EraVM::getWithInsSwapped(NewOpcode);
        assert(NewOpcode != -1);
      }

      EraVM::replaceArgument(UseMI, ArgNo, In0Ops, TII->get(NewOpcode));
      LLVM_DEBUG(dbgs() << "== Combine load const"; MI.dump();
                 dbgs() << "   and use:"; UseMI.dump(); dbgs() << " into:";
                 std::prev(UseMI.getIterator())->dump(););
      UsesToUpdate.insert(&UseMI);
      ToRemove.emplace_back(&UseMI);
    }
  }

  for (auto *MI : ToRemove)
    MI->eraseFromParent();

  return !ToRemove.empty();
}

bool EraVMFoldImm::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********** EraVM FOLD IMM **********\n"
                    << "********** Function: " << MF.getName() << '\n');

  RegInfo = &MF.getRegInfo();
  TII = cast<EraVMInstrInfo>(MF.getSubtarget<EraVMSubtarget>().getInstrInfo());
  assert(TII && "TargetInstrInfo must be a valid object");

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    Changed |= tryFoldImm(MBB);

  return Changed;
}

/// createEraVMFoldImmOperandsPass - returns an instance of the fold imm.
FunctionPass *llvm::createEraVMFoldImmPass() { return new EraVMFoldImm(); }
