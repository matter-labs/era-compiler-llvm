//===--- EVMStackifyCodeEmitter.h - Create stackified MIR -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file transforms MIR to the 'stackified' MIR.
//
//===----------------------------------------------------------------------===//

#include "EVMStackifyCodeEmitter.h"
#include "EVMStackShuffler.h"
#include "EVMStackSolver.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "evm-stackify-code-emitter"

// Return the number of input arguments of the call instruction.
static size_t getCallNumArgs(const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  assert(Call->explicit_uses().begin()->isGlobal() &&
         "First operand must be a function");
  size_t NumArgs = Call->getNumExplicitOperands() - Call->getNumExplicitDefs();
  // The first operand is a function, so don't count it.
  NumArgs = NumArgs - 1;
  // If function will return, we need to account for the return label.
  return isNoReturnCallMI(*Call) ? NumArgs : NumArgs + 1;
}

static std::string getUnreachableStackSlotError(const MachineFunction &MF,
                                                const Stack &CurrentStack,
                                                const StackSlot *Slot,
                                                size_t Depth, bool isSwap) {
  return (MF.getName() + Twine(": cannot ") + (isSwap ? "swap " : "dup ") +
          std::to_string(Depth) + "-th stack item, " + Slot->toString() +
          ".\nItem it located too deep in the stack: " +
          CurrentStack.toString())
      .str();
}

size_t EVMStackifyCodeEmitter::CodeEmitter::stackHeight() const {
  return StackHeight;
}

void EVMStackifyCodeEmitter::CodeEmitter::enterMBB(MachineBasicBlock *MBB,
                                                   int Height) {
  StackHeight = Height;
  CurMBB = MBB;
  LLVM_DEBUG(dbgs() << "\n"
                    << "Set stack height: " << StackHeight << "\n");
  LLVM_DEBUG(dbgs() << "Setting current location to: " << MBB->getNumber()
                    << "." << MBB->getName() << "\n");
}

void EVMStackifyCodeEmitter::CodeEmitter::emitInst(const MachineInstr *MI) {
  unsigned Opc = MI->getOpcode();
  assert(Opc != EVM::JUMP && Opc != EVM::JUMPI && Opc != EVM::JUMP_UNLESS &&
         Opc != EVM::ARGUMENT && Opc != EVM::RET && Opc != EVM::CONST_I256 &&
         Opc != EVM::COPY_I256 && Opc != EVM::FCALL &&
         "Unexpected instruction");

  size_t NumInputs = MI->getNumExplicitOperands() - MI->getNumExplicitDefs();
  assert(StackHeight >= NumInputs && "Not enough operands on the stack");
  StackHeight -= NumInputs;
  StackHeight += MI->getNumExplicitDefs();

  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitSWAP(unsigned Depth) {
  unsigned Opc = EVM::getSWAPOpcode(Depth);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitDUP(unsigned Depth) {
  StackHeight += 1;
  unsigned Opc = EVM::getDUPOpcode(Depth);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitPOP() {
  assert(StackHeight > 0 && "Expected at least one operand on the stack");
  StackHeight -= 1;
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::POP_S));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitConstant(const APInt &Val) {
  StackHeight += 1;
  unsigned Opc = EVM::getPUSHOpcode(Val);
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), DebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)));
  if (Opc != EVM::PUSH0)
    NewMI.addCImm(ConstantInt::get(MF.getFunction().getContext(), Val));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitConstant(uint64_t Val) {
  emitConstant(APInt(256, Val));
}

void EVMStackifyCodeEmitter::CodeEmitter::emitSymbol(const MachineInstr *MI,
                                                     MCSymbol *Symbol) {
  assert(isLinkerPseudoMI(*MI) && "Unexpected symbol instruction");
  StackHeight += 1;
  // This is codegen-only instruction, that will be converted into PUSH4.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(MI->getOpcode())))
                   .addSym(Symbol);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitLabelReference(
    const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  StackHeight += 1;
  auto [It, Inserted] = CallReturnSyms.try_emplace(Call);
  if (Inserted)
    It->second = MF.getContext().createTempSymbol("FUNC_RET", true);
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::PUSH_LABEL))
          .addSym(It->second);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitFuncCall(const MachineInstr *MI) {
  assert(MI->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  assert(CurMBB == MI->getParent());

  size_t NumInputs = getCallNumArgs(MI);
  assert(StackHeight >= NumInputs && "Not enough operands on the stack");
  StackHeight -= NumInputs;

  // PUSH_LABEL increases the stack height on 1, but we don't increase it
  // explicitly here, as the label will be consumed by the following JUMP.
  StackHeight += MI->getNumExplicitDefs();

  // Create pseudo jump to the function, that will be expanded into PUSH and
  // JUMP instructions in the AsmPrinter.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoCALL))
                   .addGlobalAddress(MI->explicit_uses().begin()->getGlobal());

  // If this function returns, add a return label so we can emit it together
  // with JUMPDEST. This is taken care in the AsmPrinter.
  if (!isNoReturnCallMI(*MI))
    NewMI.addSym(CallReturnSyms.at(MI));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitRet(const MachineInstr *MI) {
  assert(MI->getOpcode() == EVM::RET && "Unexpected ret instruction");
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoRET));
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitUncondJump(
    const MachineInstr *MI, MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMP &&
         "Unexpected unconditional jump instruction");
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoJUMP))
                   .addMBB(Target);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitCondJump(
    const MachineInstr *MI, MachineBasicBlock *Target) {
  assert(MI->getOpcode() == EVM::JUMPI ||
         MI->getOpcode() == EVM::JUMP_UNLESS &&
             "Unexpected conditional jump instruction");
  assert(StackHeight > 0 && "Expected at least one operand on the stack");
  StackHeight -= 1;
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
              TII->get(MI->getOpcode() == EVM::JUMPI ? EVM::PseudoJUMPI
                                                     : EVM::PseudoJUMP_UNLESS))
          .addMBB(Target);
  verify(NewMI);
}

void EVMStackifyCodeEmitter::CodeEmitter::emitStop() {
  auto NewMI =
      BuildMI(*CurMBB, CurMBB->end(), DebugLoc(), TII->get(EVM::STOP_S));
  verify(NewMI);
}

// Verify that a stackified instruction doesn't have registers and dump it.
void EVMStackifyCodeEmitter::CodeEmitter::verify(const MachineInstr *MI) const {
  assert(EVMInstrInfo::isStack(MI) &&
         "Only stackified instructions are allowed");
  assert(all_of(MI->operands(),
                [](const MachineOperand &MO) { return !MO.isReg(); }) &&
         "Registers are not allowed in stackified instructions");

  LLVM_DEBUG(dbgs() << "Adding: " << *MI << "stack height: " << StackHeight
                    << "\n");
}
void EVMStackifyCodeEmitter::CodeEmitter::finalize() {
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : make_early_inc_range(MBB))
      // Remove all the instructions that are not stackified.
      // TODO: #749: Fix debug info for stackified instructions and don't
      // remove debug instructions.
      if (!EVMInstrInfo::isStack(&MI))
        MI.eraseFromParent();
}

void EVMStackifyCodeEmitter::adjustStackForInst(const MachineInstr *MI,
                                                size_t NumArgs) {
  // Remove arguments from CurrentStack.
  CurrentStack.erase(CurrentStack.end() - NumArgs, CurrentStack.end());

  // Push return values to CurrentStack.
  append_range(CurrentStack, StackModel.getSlotsForInstructionDefs(MI));
  assert(Emitter.stackHeight() == CurrentStack.size());
}

void EVMStackifyCodeEmitter::emitMI(const MachineInstr &MI) {
  assert(Emitter.stackHeight() == CurrentStack.size());

  if (MI.getOpcode() == EVM::FCALL) {
    size_t NumArgs = getCallNumArgs(&MI);
    assert(CurrentStack.size() >= NumArgs);

    // Assert that we got the correct return label on stack.
    if (!isNoReturnCallMI(MI)) {
      [[maybe_unused]] const auto *ReturnLabelSlot = dyn_cast<CallerReturnSlot>(
          CurrentStack[CurrentStack.size() - NumArgs]);
      assert(ReturnLabelSlot && ReturnLabelSlot->getCall() == &MI);
    }
    Emitter.emitFuncCall(&MI);
    adjustStackForInst(&MI, NumArgs);
  } else if (!isPushOrDupLikeMI(MI)) {
    size_t NumArgs = MI.getNumExplicitOperands() - MI.getNumExplicitDefs();
    assert(CurrentStack.size() >= NumArgs);
    // TODO: assert that we got a correct stack for the call.

    Emitter.emitInst(&MI);
    adjustStackForInst(&MI, NumArgs);
  }

  // If the MI doesn't define anything, we are done.
  if (!MI.getNumExplicitDefs())
    return;

  // Invalidate occurrences of the assigned variables.
  for (auto *&CurrentSlot : CurrentStack)
    if (const auto *RegSlot = dyn_cast<RegisterSlot>(CurrentSlot))
      if (MI.definesRegister(RegSlot->getReg()))
        CurrentSlot = EVMStackModel::getUnusedSlot();

  // Assign variables to current stack top.
  assert(CurrentStack.size() >= MI.getNumExplicitDefs());
  llvm::copy(StackModel.getSlotsForInstructionDefs(&MI),
             CurrentStack.end() - MI.getNumExplicitDefs());
}

// Checks if it's valid to transition from \p SourceStack to \p TargetStack,
// that is \p SourceStack matches each slot in \p TargetStack that is not a
// UnusedSlot exactly.
[[maybe_unused]] static bool match(const Stack &Source, const Stack &Target) {
  return Source.size() == Target.size() &&
         all_of(zip_equal(Source, Target), [](const auto &Pair) {
           const auto [Src, Tgt] = Pair;
           return isa<UnusedSlot>(Tgt) || (Src == Tgt);
         });
}

void EVMStackifyCodeEmitter::emitStackPermutations(const Stack &TargetStack) {
  assert(Emitter.stackHeight() == CurrentStack.size());
  const unsigned StackDepthLimit = StackModel.stackDepthLimit();

  calculateStack(
      CurrentStack, TargetStack, StackDepthLimit,
      // Swap.
      [&](unsigned I) {
        assert(CurrentStack.size() == Emitter.stackHeight());
        assert(I > 0 && I < CurrentStack.size());
        if (I <= StackDepthLimit) {
          Emitter.emitSWAP(I);
          return;
        }
        const StackSlot *Slot = CurrentStack[CurrentStack.size() - I - 1];
        std::string ErrMsg = getUnreachableStackSlotError(
            MF, CurrentStack, Slot, I + 1, /* isSwap */ true);
        report_fatal_error(ErrMsg.c_str());
      },
      // Push or dup.
      [&](const StackSlot *Slot) {
        assert(CurrentStack.size() == Emitter.stackHeight());

        // Dup the slot, if already on stack and reachable.
        auto SlotIt = llvm::find(llvm::reverse(CurrentStack), Slot);
        if (SlotIt != CurrentStack.rend()) {
          unsigned Depth = std::distance(CurrentStack.rbegin(), SlotIt);
          if (Depth < StackDepthLimit) {
            Emitter.emitDUP(static_cast<unsigned>(Depth + 1));
            return;
          }
          if (!Slot->isRematerializable()) {
            std::string ErrMsg = getUnreachableStackSlotError(
                MF, CurrentStack, Slot, Depth + 1, /* isSwap */ false);
            report_fatal_error(ErrMsg.c_str());
          }
        }

        // Rematerialize the slot.
        assert(Slot->isRematerializable());
        if (const auto *L = dyn_cast<LiteralSlot>(Slot)) {
          Emitter.emitConstant(L->getValue());
        } else if (const auto *S = dyn_cast<SymbolSlot>(Slot)) {
          Emitter.emitSymbol(S->getMachineInstr(), S->getSymbol());
        } else if (const auto *CallRet = dyn_cast<CallerReturnSlot>(Slot)) {
          Emitter.emitLabelReference(CallRet->getCall());
        } else {
          assert(isa<UnusedSlot>(Slot));
          // Note: this will always be popped, so we can push anything.
          Emitter.emitConstant(0);
        }
      },
      // Pop.
      [&]() { Emitter.emitPOP(); });

  assert(Emitter.stackHeight() == CurrentStack.size());
}

// Emit the stack required for enterting the MI.
void EVMStackifyCodeEmitter::emitMIEntryStack(const MachineInstr &MI) {
  // Check if we can choose cheaper stack shuffling if the MI is commutable.
  const Stack &TargetStack = StackModel.getInstEntryStack(&MI);
  bool SwapCommutable = false;
  if (MI.isCommutable()) {
    assert(TargetStack.size() > 1);

    size_t DefaultCost =
        calculateStackTransformCost(CurrentStack, TargetStack,
                                    StackModel.stackDepthLimit())
            .value_or(std::numeric_limits<unsigned>::max());

    // Swap the commutable stack items and measure the stack shuffling cost.
    // Commutable operands always take top two stack slots.
    Stack CommutedTargetStack = TargetStack;
    std::swap(CommutedTargetStack[CommutedTargetStack.size() - 1],
              CommutedTargetStack[CommutedTargetStack.size() - 2]);
    size_t CommutedCost =
        calculateStackTransformCost(CurrentStack, CommutedTargetStack,
                                    StackModel.stackDepthLimit())
            .value_or(std::numeric_limits<unsigned>::max());

    // Choose the cheapest transformation.
    SwapCommutable = CommutedCost < DefaultCost;
    emitStackPermutations(SwapCommutable ? CommutedTargetStack : TargetStack);
  } else {
    emitStackPermutations(TargetStack);
  }

#ifndef NDEBUG
  // Assert that we have the inputs of the MI on stack top.
  const Stack &SavedInput = StackModel.getMIInput(MI);
  assert(CurrentStack.size() == Emitter.stackHeight());
  assert(CurrentStack.size() >= SavedInput.size());
  Stack Input(CurrentStack.end() - SavedInput.size(), CurrentStack.end());

  // Adjust the Input if needed.
  if (SwapCommutable)
    std::swap(Input[Input.size() - 1], Input[Input.size() - 2]);

  assert(match(Input, SavedInput));
#endif // NDEBUG
}

void EVMStackifyCodeEmitter::run() {
  assert(CurrentStack.empty() && Emitter.stackHeight() == 0);

  SmallPtrSet<MachineBasicBlock *, 32> Visited;
  SmallVector<MachineBasicBlock *, 32> WorkList{&MF.front()};
  while (!WorkList.empty()) {
    auto *MBB = WorkList.pop_back_val();
    if (!Visited.insert(MBB).second)
      continue;

    CurrentStack = StackModel.getMBBEntryStack(MBB);
    Emitter.enterMBB(MBB, CurrentStack.size());

    // Get branch information before we start to change the BB.
    auto [BranchTy, TBB, FBB, BrInsts, Condition] = getBranchInfo(MBB);
    bool HasReturn = MBB->isReturnBlock();
    const MachineInstr *ReturnMI = HasReturn ? &MBB->back() : nullptr;

    for (const auto &MI : StackModel.instructionsToProcess(MBB)) {
      // We are done if the MI is in the stack form.
      if (EVMInstrInfo::isStack(&MI))
        break;

      emitMIEntryStack(MI);

      [[maybe_unused]] size_t BaseHeight =
          CurrentStack.size() - StackModel.getMIInput(MI).size();

      emitMI(MI);

#ifndef NDEBUG
      // Assert that the MI produced its proclaimed output.
      size_t NumDefs = MI.getNumExplicitDefs();
      size_t StackSize = CurrentStack.size();
      assert(StackSize == Emitter.stackHeight());
      assert(StackSize == BaseHeight + NumDefs);
      assert(StackSize >= NumDefs);
      // Check that the top NumDefs slots are the MI defs.
      for (size_t I = StackSize - NumDefs; I < StackSize; ++I)
        assert(
            MI.definesRegister(cast<RegisterSlot>(CurrentStack[I])->getReg()));
#endif // NDEBUG
    }

    // Exit the block.
    if (BranchTy == EVMInstrInfo::BT_None) {
      if (HasReturn) {
        assert(!MF.getFunction().hasFnAttribute(Attribute::NoReturn));
        assert(StackModel.getReturnArguments(*ReturnMI) ==
               StackModel.getMBBExitStack(MBB));
        // Create the function return stack and jump.
        emitStackPermutations(StackModel.getMBBExitStack(MBB));
        Emitter.emitRet(ReturnMI);
      }
    } else if (BranchTy == EVMInstrInfo::BT_Uncond ||
               BranchTy == EVMInstrInfo::BT_NoBranch) {
      if (!MBB->succ_empty()) {
        // Create the stack expected at the jump target.
        emitStackPermutations(StackModel.getMBBEntryStack(TBB));
        assert(match(CurrentStack, StackModel.getMBBEntryStack(TBB)));

        if (!BrInsts.empty())
          Emitter.emitUncondJump(BrInsts[0], TBB);

        WorkList.push_back(TBB);
      } else {
        Emitter.emitStop();
      }
    } else {
      assert(BranchTy == EVMInstrInfo::BT_Cond ||
             BranchTy == EVMInstrInfo::BT_CondUncond);
      // Create the shared entry stack of the jump targets, which is
      // stored as exit stack of the current MBB.
      emitStackPermutations(StackModel.getMBBExitStack(MBB));
      assert(!CurrentStack.empty() &&
             CurrentStack.back() == StackModel.getStackSlot(*Condition));

      // Emit the conditional jump to the non-zero label and update the
      // stored stack.
      assert(!BrInsts.empty());
      Emitter.emitCondJump(BrInsts[BrInsts.size() - 1], TBB);
      CurrentStack.pop_back();

      // Assert that we have a valid stack for both jump targets.
      assert(match(CurrentStack, StackModel.getMBBEntryStack(TBB)));
      assert(match(CurrentStack, StackModel.getMBBEntryStack(FBB)));

      // Generate unconditional jump if needed.
      if (BrInsts.size() == 2)
        Emitter.emitUncondJump(BrInsts[0], FBB);

      WorkList.push_back(TBB);
      WorkList.push_back(FBB);
    }
  }
  Emitter.finalize();
}
