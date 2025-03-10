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

// Return whether the function of the call instruction will return.
static bool callWillReturn(const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  const MachineOperand *FuncOp = Call->explicit_uses().begin();
  assert(FuncOp->isGlobal() && "Expected a global value");
  const auto *Func = cast<Function>(FuncOp->getGlobal());
  return !Func->hasFnAttribute(Attribute::NoReturn);
}

// Return the number of input arguments of the call instruction.
static size_t getCallArgCount(const MachineInstr *Call) {
  assert(Call->getOpcode() == EVM::FCALL && "Unexpected call instruction");
  assert(Call->explicit_uses().begin()->isGlobal() &&
         "First operand must be a function");
  size_t NumExplicitInputs =
      Call->getNumExplicitOperands() - Call->getNumExplicitDefs();

  // The first operand is a function, so don't count it. If function
  // will return, we need to account for the return label.
  constexpr size_t NumFuncOp = 1;
  return NumExplicitInputs - NumFuncOp + callWillReturn(Call);
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
  assert(Opc != EVM::JUMP && Opc != EVM::JUMPI && Opc != EVM::ARGUMENT &&
         Opc != EVM::RET && Opc != EVM::CONST_I256 && Opc != EVM::COPY_I256 &&
         Opc != EVM::FCALL && "Unexpected instruction");

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
  unsigned Opc = MI->getOpcode();
  assert(Opc == EVM::DATASIZE || Opc == EVM::DATAOFFSET ||
         Opc == EVM::LINKERSYMBOL ||
         Opc == EVM::LOADIMMUTABLE && "Unexpected symbol instruction");
  StackHeight += 1;
  // This is codegen-only instruction, that will be converted into PUSH4.
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::getStackOpcode(Opc)))
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

  size_t NumInputs = getCallArgCount(MI);
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
  if (callWillReturn(MI))
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
  assert(MI->getOpcode() == EVM::JUMPI &&
         "Unexpected conditional jump instruction");
  assert(StackHeight > 0 && "Expected at least one operand on the stack");
  StackHeight -= 1;
  auto NewMI = BuildMI(*CurMBB, CurMBB->end(), MI->getDebugLoc(),
                       TII->get(EVM::PseudoJUMPI))
                   .addMBB(Target);
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

void EVMStackifyCodeEmitter::emitMI(const MachineInstr *MI) {
  if (MI->getOpcode() == EVM::FCALL) {
    size_t NumArgs = getCallArgCount(MI);
    // Validate stack.
    assert(Emitter.stackHeight() == CurrentStack.size());
    assert(CurrentStack.size() >= NumArgs);

    // Assert that we got the correct return label on stack.
    if (callWillReturn(MI)) {
      [[maybe_unused]] const auto *ReturnLabelSlot = dyn_cast<CallerReturnSlot>(
          CurrentStack[CurrentStack.size() - NumArgs]);
      assert(ReturnLabelSlot && ReturnLabelSlot->getCall() == MI);
    }
    // Emit call.
    Emitter.emitFuncCall(MI);
    adjustStackForInst(MI, NumArgs);
  } else if (!isConstCopyOrLinkerMI(*MI)) {
    size_t NumArgs = MI->getNumExplicitOperands() - MI->getNumExplicitDefs();
    // Validate stack.
    assert(Emitter.stackHeight() == CurrentStack.size());
    assert(CurrentStack.size() >= NumArgs);
    // TODO: assert that we got a correct stack for the call.

    // Emit instruction.
    Emitter.emitInst(MI);
    adjustStackForInst(MI, NumArgs);
  }

  if (MI->getNumExplicitDefs()) {
    assert(Emitter.stackHeight() == CurrentStack.size());
    // Invalidate occurrences of the assigned variables.
    for (auto *&CurrentSlot : CurrentStack)
      if (const auto *RegSlot = dyn_cast<RegisterSlot>(CurrentSlot))
        if (MI->definesRegister(RegSlot->getReg()))
          CurrentSlot = EVMStackModel::getUnusedSlot();

    // Assign variables to current stack top.
    assert(CurrentStack.size() >= MI->getNumExplicitDefs());
    llvm::copy(StackModel.getSlotsForInstructionDefs(MI),
               CurrentStack.end() - MI->getNumExplicitDefs());
  }
}

// Checks if it's valid to transition from \p SourceStack to \p TargetStack,
// that is \p SourceStack matches each slot in \p TargetStack that is not a
// UnusedSlot exactly.
[[maybe_unused]] static bool areStacksCompatible(const Stack &SourceStack,
                                                 const Stack &TargetStack) {
  return SourceStack.size() == TargetStack.size() &&
         all_of(zip_equal(SourceStack, TargetStack), [](const auto &Pair) {
           const auto [Src, Tgt] = Pair;
           return isa<UnusedSlot>(Tgt) || (Src == Tgt);
         });
}

void EVMStackifyCodeEmitter::createStackLayout(const Stack &TargetStack) {
  assert(Emitter.stackHeight() == CurrentStack.size());
  // ::calculateStack asserts that it has successfully achieved the target
  // stack state.
  const unsigned StackDepthLimit = StackModel.stackDepthLimit();
  ::calculateStack(
      CurrentStack, TargetStack, StackDepthLimit,
      // Swap callback.
      [&](unsigned I) {
        assert(CurrentStack.size() == Emitter.stackHeight());
        assert(I > 0 && I < CurrentStack.size());
        if (I <= StackDepthLimit) {
          Emitter.emitSWAP(I);
        } else {
          const StackSlot *Slot = CurrentStack[CurrentStack.size() - I - 1];
          std::string ErrMsg = getUnreachableStackSlotError(
              MF, CurrentStack, Slot, I + 1, /* isSwap */ true);
          report_fatal_error(ErrMsg.c_str());
        }
      },
      // Push or dup callback.
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

        // The slot can be rematerialized.
        if (const auto *L = dyn_cast<LiteralSlot>(Slot)) {
          Emitter.emitConstant(L->getValue());
        } else if (const auto *S = dyn_cast<SymbolSlot>(Slot)) {
          Emitter.emitSymbol(S->getMachineInstr(), S->getSymbol());
        } else if (const auto *CallRet = dyn_cast<CallerReturnSlot>(Slot)) {
          Emitter.emitLabelReference(CallRet->getCall());
        } else if (isa<CalleeReturnSlot>(Slot)) {
          llvm_unreachable("Cannot produce callee return.");
        } else if (isa<RegisterSlot>(Slot)) {
          llvm_unreachable("Variable not found on stack.");
        } else {
          assert(isa<UnusedSlot>(Slot));
          // Note: this will always be popped, so we can push anything.
          Emitter.emitConstant(0);
        }
      },
      // Pop callback.
      [&]() { Emitter.emitPOP(); });

  assert(Emitter.stackHeight() == CurrentStack.size());
}

// Emit the stack required for enterting the MI.
void EVMStackifyCodeEmitter::emitMIEntryStack(const MachineInstr *MI) {
  // Check if we can choose cheaper stack shuffling if the MI is commutable.
  const Stack &TargetStack = StackModel.getInstEntryStack(MI);
  bool SwapCommutable = false;
  if (MI->isCommutable()) {
    // Get the stack layout before the instruction.
    size_t DefaultCost = calculateStackTransformCost(
        CurrentStack, TargetStack, StackModel.stackDepthLimit());

    // Commutable operands always take top two stack slots.
    const unsigned OpIdx1 = 0, OpIdx2 = 1;
    assert(TargetStack.size() > 1);

    // Swap the commutable stack items and measure the stack shuffling cost
    // again.
    Stack CommutedTargetStack = TargetStack;
    std::swap(CommutedTargetStack[CommutedTargetStack.size() - OpIdx1 - 1],
              CommutedTargetStack[CommutedTargetStack.size() - OpIdx2 - 1]);
    size_t CommutedCost = calculateStackTransformCost(
        CurrentStack, CommutedTargetStack, StackModel.stackDepthLimit());
    // Choose the cheapest transformation.
    SwapCommutable = CommutedCost < DefaultCost;
    createStackLayout(SwapCommutable ? CommutedTargetStack : TargetStack);
  } else {
    createStackLayout(TargetStack);
  }

  // Assert that we have the inputs of the MI on stack top.
  const Stack &SavedInput = StackModel.getMIInput(*MI);
  assert(CurrentStack.size() == Emitter.stackHeight());
  assert(CurrentStack.size() >= SavedInput.size());
  Stack Input(CurrentStack.end() - SavedInput.size(), CurrentStack.end());
  // Adjust the Input if needed.
  if (SwapCommutable)
    std::swap(Input[Input.size() - 1], Input[Input.size() - 2]);

  assert(areStacksCompatible(Input, SavedInput));
}

void EVMStackifyCodeEmitter::run() {
  assert(CurrentStack.empty() && Emitter.stackHeight() == 0);

  SmallPtrSet<MachineBasicBlock *, 32> Visited;
  SmallVector<MachineBasicBlock *, 32> WorkList{&MF.front()};
  while (!WorkList.empty()) {
    auto *MBB = WorkList.pop_back_val();
    if (!Visited.insert(MBB).second)
      continue;

    // Might set some slots to unused, if not required by the block.
    CurrentStack = StackModel.getMBBEntryStack(MBB);
    Emitter.enterMBB(MBB, CurrentStack.size());

    // Get branch information before we start to change the BB.
    auto [BranchTy, TBB, FBB, BrInsts, Condition] = getBranchInfo(MBB);
    bool HasReturn = MBB->isReturnBlock();
    const MachineInstr *ReturnMI = HasReturn ? &MBB->back() : nullptr;

    // To process only instructions present before the emitter,  
    // store their pointers in a vector.  
    SmallVector<const MachineInstr *> MIs;
    for (const auto &MI : StackModel.instructionsToProcess(MBB))
      MIs.push_back(&MI);

    for (const auto *MI : MIs) {
      emitMIEntryStack(MI);

      [[maybe_unused]] size_t BaseHeight =
          CurrentStack.size() - StackModel.getMIInput(*MI).size();

      emitMI(MI);

#ifndef NDEBUG
      // Assert that the MI produced its proclaimed output.
      size_t NumDefs = MI->getNumExplicitDefs();
      size_t StackSize = CurrentStack.size();
      assert(StackSize == Emitter.stackHeight());
      assert(StackSize == BaseHeight + NumDefs);
      assert(StackSize >= NumDefs);
      // Check that the top NumDefs slots are the MI defs.
      for (size_t I = StackSize - NumDefs; I < StackSize; ++I)
        assert(
            MI->definesRegister(cast<RegisterSlot>(CurrentStack[I])->getReg()));
#endif // NDEBUG
    }

    // Exit the block.
    switch (BranchTy) {
    case EVMInstrInfo::BT_None: {
      if (!HasReturn)
        break;
      assert(!MF.getFunction().hasFnAttribute(Attribute::NoReturn));
      // Create the function return layout and jump.
      assert(StackModel.getReturnArguments(*ReturnMI) ==
             StackModel.getMBBExitStack(MBB));
      createStackLayout(StackModel.getMBBExitStack(MBB));
      Emitter.emitRet(ReturnMI);
    } break;
    case EVMInstrInfo::BT_Uncond:
    case EVMInstrInfo::BT_NoBranch: {
      if (MBB->succ_empty())
        break;
      // Create the stack expected at the jump target.
      createStackLayout(StackModel.getMBBEntryStack(TBB));

      // Assert that we have a valid stack for the target.
      assert(areStacksCompatible(CurrentStack,
                                 StackModel.getMBBEntryStack(TBB)));

      if (!BrInsts.empty())
        Emitter.emitUncondJump(BrInsts[0], TBB);
      WorkList.push_back(TBB);
    } break;
    case EVMInstrInfo::BT_Cond:
    case EVMInstrInfo::BT_CondUncond: {
      // Create the shared entry layout of the jump targets, which is
      // stored as exit layout of the current block.
      createStackLayout(StackModel.getMBBExitStack(MBB));

      // Assert that we have the correct condition on stack.
      assert(!CurrentStack.empty());
      assert(CurrentStack.back() == StackModel.getStackSlot(*Condition));

      // Emit the conditional jump to the non-zero label and update the
      // stored stack.
      assert(!BrInsts.empty());
      Emitter.emitCondJump(BrInsts[BrInsts.size() - 1], TBB);
      CurrentStack.pop_back();

      // Assert that we have a valid stack for both jump targets.
      assert(areStacksCompatible(CurrentStack,
                                 StackModel.getMBBEntryStack(TBB)));
      assert(areStacksCompatible(CurrentStack,
                                 StackModel.getMBBEntryStack(FBB)));

      // Generate unconditional jump if needed.
      if (BrInsts.size() == 2)
        Emitter.emitUncondJump(BrInsts[0], FBB);
      WorkList.push_back(TBB);
      WorkList.push_back(FBB);
    }
    }
  }
  Emitter.finalize();
}
