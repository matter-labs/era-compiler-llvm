//===---------- EVMStackify.cpp - Replace phys regs with virt regs --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The pass works right before the AsmPrinter and performs code stackification,
// so it can be executed on the Ethereum virtual stack machine. Physically,
// this means adding stack manipulation instructions (POP, PUSH, DUP or SWAP)
// to the Machine IR and removing stack operands of instructions. Logically,
// this corresponds to allocation of stack slots and mapping them on virtual
// registers.
//
// Current implementation is based on the article M. Shannon, C. Bailey
//   "Global Stack Allocation – Register Allocation for Stack Machines",
// but in quite simplified form.
//
// than that required for the stackification itself. It's expected that
// previous passes perform required optimizations:
//  - splitting live intervals that contain unrelated register values.
//  - a code scheduling to form single-expression-use.
//  - coloring of virtual registers. This is a kind of register coalescing.
//
// Main conception borrowed from the article is a logical view of the physical
// stack in which stack consists of 3 main regions. Starting from the top,
// these are:
//   - The evaluation region (E-stack)
//   - The local region (L-stack)
//   - The transfer region (X-stack)
//
// E-stack is used for holding instruction arguments/results right before and
// after its execution. In all other cases it should be empty.
//
// The L-stack is always empty at the beginning and end of any basic block,
// but may contain values between instructions. The values in L-stack are live
// inside one basic block.
//
// The X-stack is used to store values both during basic blocks and on edges in
// the flow graph. The x-stack need only be empty at procedure exit. Unlike the
// article, in current implementation X-stack is statically formed and fixed
// for the entire function. This makes an implementation easier, but leads
// to non-optimal stack usage, which, in turn, breaks the stack limits.
// FIXME: try to implement "Global Stack Allocation" approach.
//
// The algorithm maintains programmatic model of the stack to know, at any
// given point of the code: a) height of physical stack, b) content of both
// the L and X-stacks.
//
// The algorithm runs, in outline, as follows (on a function level):
//
// 1. Determine content of the X-stack. Here we include all the virtual
// registers, whose live intervals cross a BB boundary. Its content is fixed
// during the whole function. See, allocateXStack()
//
// 2. Perform handling of each instruction. See handleInstrUses().
//    As an example let's see how
//
//     %vreg7 = ADD %vreg1 kill, %vreg2 kill
//
//   is handled. Suppose the stack layout before the instruction is
//
//        |vreg2| - depth 1
//     L  |vreg3|
//        |vreg9|
//        |vreg1| - depth 4
//       ---------
//        |vreg13|
//     X  |vreg7| - depth 6
//        |vreg11|
//       ---------
//        |raddr| - function return address
//        |farg1| - first function argument
//        |farg2|
//        .......
//        |fargN|
//
//   In order to execute the instruction its arguments should be placed
//   on top of the stack for which we can use (DUP1 - DUP16) instructions.
//   In terms of the stack model we load them to E-stack. So, to load
//   arguments and execute the instructions:
//
//     DUP4  // load %vreg1; No need to load %vreg2, as it's already located
//           // in the right place (depth 1) and it's killed at ADD.
//     ADD
//
//   Stack layout rigth after loading arguments on top of the stack:
//
//
//        |vreg1| - depth 1
//     E  |vreg2| - depth 2 // It was logically migrated from L -> E stack
//       ---------
//     L  |vreg3|
//        |vreg9|
//        |vreg1| - depth 5
//       ---------
//        |vreg13|
//     X  |vreg7| - depth 7
//        |vreg11|
//       ---------
//        |raddr| - function return address
//        |farg1| - first function argument
//        |farg2|
//        .......
//        |fargN|
//
//   Stack layout right after instruction execution:
//
//     E  |vreg7|
//       ---------
//     L  |vreg3|
//        |vreg9|
//        |vreg1| - depth 4, is dead
//       ---------
//        |vreg13|
//     X  |vreg7| - depth 6
//        |vreg11|
//       ---------
//        |raddr| - function return address
//        |farg1| - first function argument
//        |farg2|
//        .......
//        |fargN|
//
//   The resulting register %vreg7 should be placed to its final location in
//   X-stack. To do this we can use SWAP + POP pair, so the final "stackified"
//   code is:
//
//     DUP4   // Duplicated the 4th stack item
//     ADD
//     SWAP5  // Swap top item with 6th (SWAP opcodes have shifted names)
//     POP    // Remove top item from the stack
//
//   Note:
//
//     - resulted code has actually 25% of payload instructions. One the
//       goals of stackification is to minimize the stack manipulation
//       instructions.
//
//     - %vreg1 is dead after the instruction, so it theoretically should be
//       removed from the stack. Unfortunately, EVM has no efficient
//       instructions fot this, so we need emulate it via a sequence of
//       SWAP + DUP + POP instructions which may lead to quite inefficient
//       code. As an experiment, this is implemented in handleInstrDefs().
//       A better strategy would be to wait when a dead register is near
//       the stack top.
//
//     - Several instructions requires special handling (FCALL, RET, so on).
//
// 3. Once all the instructions are handled perform some clean up:
//    replace, remove dead instructions.
//
//
// The main drawback of this algorithm is that X-stack is fixed during the
// whole function, which, for example, means we cannot remove dead registers
// from it. On more-or-less complicated programs this may lead to an attempt
// to access too far from stack top (> 16 element), which triggers an assertion
// in the code.
// More sophisticated algorithm is required, as described in the article.
//
// TODO: CPR-1562. Current implementation is a draft stackification version
// which needs to be refactored, cleaned and documented.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "EVMMachineFunctionInfo.h"
#include "EVMSubtarget.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "evm-stackify"

namespace {

enum class StackType { X, L, E };

struct StackLocation {
  StackType Type;
  // Depth counted from the top of this stack type
  // (not a physical depth).
  unsigned Depth;
};

using MIIter = MachineBasicBlock::iterator;
using RegisterLocation = std::pair<Register, StackLocation>;

class StackModel {
  [[maybe_unused]] static constexpr unsigned StackAccessDepthLimit = 16;
  const LiveIntervals &LIS;
  const EVMInstrInfo *TII;
  MachineFunction *MF;

  // Frame objects area which is the result of
  // alloca's lowering.
  unsigned NumFrameObjs = 0;
  std::deque<Register> XStack;
  std::deque<Register> LStack;
  // E-stack is only used to properly calculate physical stack
  // height, so in the code below we usually push there '0'
  // register.
  std::deque<Register> EStack;
  SmallVector<MachineInstr *> ToErase;

public:
  StackModel(MachineFunction *MF, const LiveIntervals &LIS,
             const EVMInstrInfo *TII)
      : LIS(LIS), TII(TII), MF(MF) {}

  // Loads instruction arguments to the stack top and stores
  // results.
  void handleInstruction(MachineInstr *MI);

  // Allocate space for both X stack and allocas.
  void preProcess();

  // Special handling of some instructions.
  void postProcess();

  void enterBB(MachineBasicBlock *BB) {
    assert(EStack.empty() && LStack.empty());
  }

  void leaveBB(MachineBasicBlock *BB) {
    // Clear L-stack.
    assert(EStack.empty());
    peelPhysStack(StackType::L, LStack.size(), BB, BB->end());
  }

  void dumpState() const;

private:
  StackLocation allocateStackModelLoc(const Register &Reg);

  void allocateXStack();

  void allocateFrameObjects();

  std::optional<StackLocation> getStackLocation(const Register &Reg) const;

  bool isRegisterKill(const Register &Reg, const MachineInstr *MI) const;

  void migrateToEStack(const Register &Reg, const StackLocation &Loc);

  unsigned getPhysRegDepth(const StackLocation &Loc) const;

  unsigned getPhysRegDepth(const Register &Reg) const;

  MachineInstr *loadRegToPhysStackTop(unsigned Depth, MachineBasicBlock *MBB,
                                      MIIter Insert, const DebugLoc &DL);

  MachineInstr *loadRegToPhysStackTop(const Register &Reg,
                                      MachineBasicBlock *MBB, MIIter Insert,
                                      const DebugLoc &DL);

  MachineInstr *loadRegToPhysStackTop(const Register &Reg, MachineInstr *MI) {
    return loadRegToPhysStackTop(Reg, MI->getParent(), MI, MI->getDebugLoc());
  }

  MachineInstr *loadRegToPhysStackTop(unsigned Depth, MachineInstr *MI) {
    return loadRegToPhysStackTop(Depth, MI->getParent(), MI, MI->getDebugLoc());
  }

  MachineInstr *storeRegToPhysStack(const Register &Reg, MIIter Insert);

  MachineInstr *storeRegToPhysStackAt(unsigned Depth, MachineBasicBlock *MBB,
                                      MIIter Insert, const DebugLoc &DL);

  SmallVector<RegisterLocation>::const_iterator
  migrateTopRegsToEStack(const SmallVector<RegisterLocation> &RegLoc,
                         MachineInstr *MI);

  bool migrateTopRegToEStack(const Register &Reg, MachineInstr *MI);

  StackLocation pushRegToStackModel(StackType Type, const Register &Reg);

  void handleInstrUses(MachineInstr *MI);

  void handleInstrDefs(MachineInstr *MI);

  void handleArgument(MachineInstr *MI);

  void handleLStackAtJump(MachineBasicBlock *MBB, MachineInstr *MI,
                          const Register &Reg);

  void handleCondJump(MachineInstr *MI);

  void handleJump(MachineInstr *MI);

  void handleReturn(MachineInstr *MI);

  void handleCall(MachineInstr *MI);

  void handleStackLoad(MachineInstr *MI);

  void handleStackStore(MachineInstr *MI);

  void clearFrameObjsAtInst(MachineInstr *MI);

  void clearPhysStackAtInst(StackType TargetStackType, MachineInstr *MI,
                            const Register &Reg);

  void peelPhysStack(StackType Type, unsigned NumItems, MachineBasicBlock *BB,
                     MIIter Pos);

  void stackifyInstruction(MachineInstr *MI);

  static unsigned getDUPOpcode(unsigned Depth);

  static unsigned getSWAPOpcode(unsigned Depth);

  unsigned getStackSize() const {
    return NumFrameObjs + XStack.size() + LStack.size() + EStack.size();
  }

  std::deque<Register> &getStack(StackType Type) {
    return (Type == StackType::X) ? XStack : LStack;
  }
};

} // end anonymous namespace

// FIXME: this needs some kind of hashing.
std::optional<StackLocation>
StackModel::getStackLocation(const Register &Reg) const {
  auto It = std::find(LStack.begin(), LStack.end(), Reg);
  if (It != LStack.end()) {
    assert(std::count(LStack.begin(), LStack.end(), Reg) == 1);
    unsigned Depth = std::distance(LStack.begin(), It);
    return {{StackType::L, Depth}};
  }

  It = std::find(XStack.begin(), XStack.end(), Reg);
  if (It != XStack.end()) {
    assert(std::count(XStack.begin(), XStack.end(), Reg) == 1);
    unsigned Depth = std::distance(XStack.begin(), It);
    return {{StackType::X, Depth}};
  }

  return std::nullopt;
}

void StackModel::handleInstruction(MachineInstr *MI) {
  // Handle special cases.
  switch (MI->getOpcode()) {
  default:
    break;
  case EVM::JUMP:
    handleJump(MI);
    return;
  case EVM::JUMPI:
    handleCondJump(MI);
    return;
  case EVM::ARGUMENT:
    handleArgument(MI);
    return;
  case EVM::RET:
    // TODO: handle RETURN, REVERT, STOP
    handleReturn(MI);
    return;
  }

  handleInstrUses(MI);

  assert(EStack.empty());

  if (MI->getOpcode() == EVM::STACK_LOAD)
    handleStackLoad(MI);

  handleInstrDefs(MI);
}

void StackModel::handleInstrUses(MachineInstr *MI) {
  // Handle a general case. Collect use registers.
  SmallVector<std::pair<Register, StackLocation>> RegLocs;
  for (const auto &MO : MI->explicit_uses()) {
    if (!MO.isReg())
      continue;

    const auto Reg = MO.getReg();
    // SP is not used anyhow.
    if (Reg == EVM::SP)
      continue;

    const auto &Loc = getStackLocation(Reg);
    assert(Loc);
    RegLocs.push_back({Reg, *Loc});
    LLVM_DEBUG(dbgs() << "Use: " << Reg << ", Depth: " << getPhysRegDepth(Reg)
                      << '\n');
  }

  // If MI has two args, both are on stack top and at least one of them is
  // killed, try to swap them to minimize the number of used DUP instructions.
  bool NeedsSwap = false;
  if (RegLocs.size() == 2) {
    const Register &Use1 = RegLocs[0].first;
    const Register &Use2 = RegLocs[1].first;
    const unsigned Depth1 = getPhysRegDepth(Use1);
    const unsigned Depth2 = getPhysRegDepth(Use2);
    if (Depth1 == 1 && Depth2 == 0 && isRegisterKill(Use1, MI) &&
        isRegisterKill(Use2, MI))
      NeedsSwap = true;

    if (Depth1 == 0 && isRegisterKill(Use1, MI) &&
        (Depth2 > 1 || !isRegisterKill(Use2, MI)))
      NeedsSwap = true;

    if (NeedsSwap) {
      std::swap(RegLocs[0], RegLocs[1]);
      LLVM_DEBUG(dbgs() << "Swap top stack items\n");
    }

    // If we can swap instruction operands then no need to swap them in the
    // physical stack.
    if (MI->isCommutable())
      NeedsSwap = false;
  }

  SmallVector<RegisterLocation>::const_iterator B = RegLocs.begin(),
                                                StartUse =
                                                    migrateTopRegsToEStack(
                                                        RegLocs, MI);
  for (const auto &Loc : reverse(make_range(B, StartUse)))
    loadRegToPhysStackTop(Loc.first, MI);

  if (NeedsSwap)
    BuildMI(*MI->getParent(), MI, MI->getDebugLoc(), TII->get(EVM::SWAP1));

  if (MI->getOpcode() == EVM::STACK_STORE)
    handleStackStore(MI);

  // Pop from the E-stack the registers consumed by the MI.
  EStack.erase(EStack.begin(), EStack.begin() + RegLocs.size());
}

void StackModel::handleInstrDefs(MachineInstr *MI) {
  unsigned NumXLStackRegs = 0;
  MachineBasicBlock *MBB = MI->getParent();
  const DebugLoc &DL = MI->getDebugLoc();
  for (const auto &MO : reverse(MI->defs())) {
    assert(MO.isReg());

    LLVM_DEBUG(dbgs() << "Def(rev): " << MO.getReg() << '\n');
    // Push instruction's result into E-stack
    EStack.push_front(MO.getReg());
    if (getStackLocation(MO.getReg()))
      ++NumXLStackRegs;
  }

  if (EStack.empty())
    return;

  // The MI pushed on the EStack several registers. Some of them need to be
  // placed onto existing X/L stack locations. For others we need to allocate
  // new slots on X/L stack.
  auto RevIt = std::prev(EStack.end());
  MachineInstr *MII = MI;
  for (unsigned Iter = 0; Iter < NumXLStackRegs; ++Iter) {
    const Register TopReg = EStack.front();
    // If there exists a slot for the register, just move it there.
    if (getStackLocation(TopReg)) {
      MII = storeRegToPhysStack(TopReg, MII);
      continue;
    }
    // Try to find a register for which there is s slot starting from the bottom
    // of the ESatck.
    const auto Begin = EStack.begin();
    while (RevIt != Begin && !getStackLocation(*RevIt))
      --RevIt;

    // Swap just found register with the one on the stack top and move it
    // to existing location.
    unsigned Depth = std::distance(Begin, RevIt);
    const unsigned SWAPOpc = getSWAPOpcode(Depth);
    MII = BuildMI(*MBB, std::next(MIIter(MII)), DL, TII->get(SWAPOpc));
    std::swap(*Begin, *RevIt);
    MII = storeRegToPhysStack(EStack.front(), MII);
  }

  // Handle the registers for which we need to allocate X/L stack, performing
  // migration E -> L/X stack.
  for (const auto Reg : reverse(EStack)) {
    assert(!getStackLocation(Reg));
    allocateStackModelLoc(Reg);
    EStack.pop_back();
  }

  const unsigned NumDeadRegs = std::count_if(
      LStack.begin(), LStack.end(),
      [this, MI](const Register &Reg) { return isRegisterKill(Reg, MI); });

  const auto E = LStack.rend();
  for (unsigned I = 0; I < NumDeadRegs; ++I) {
    auto DeadIt =
        std::find_if(LStack.rbegin(), E, [this, MI](const Register &Reg) {
          return isRegisterKill(Reg, MI);
        });
    for (; DeadIt < E;) {
      auto LiveIt =
          std::find_if(std::next(DeadIt), E, [this, MI](const Register &Reg) {
            return !isRegisterKill(Reg, MI);
          });
      if (LiveIt == E)
        break;

      // Swap live and dead stack locations.
      const unsigned LiveDepth = getPhysRegDepth(*LiveIt);
      MII = loadRegToPhysStackTop(LiveDepth, MBB, std::next(MIIter(MII)), DL);
      const unsigned DeadDepth = getPhysRegDepth(*DeadIt);
      MII = storeRegToPhysStackAt(DeadDepth, MBB, std::next(MIIter(MII)), DL);

      LLVM_DEBUG(dbgs() << "Swapping regs: dead: " << *DeadIt << " "
                        << DeadDepth << ", live: " << *LiveIt << " "
                        << LiveDepth << '\n');

      std::swap(*DeadIt, *LiveIt);
      DeadIt = LiveIt;
    }
  }

  LLVM_DEBUG(dbgs() << "Dumping stack before removing dead regs\n");
  dumpState();

  for (unsigned I = 0; I < NumDeadRegs; ++I) {
    assert(isRegisterKill(LStack.front(), MI));
    LStack.pop_front();
    MII = BuildMI(*MBB, std::next(MIIter(MII)), DL, TII->get(EVM::POP));
  }
}

bool StackModel::isRegisterKill(const Register &Reg,
                                const MachineInstr *MI) const {
  const LiveInterval *LI = &LIS.getInterval(Reg);
  const SlotIndex SI = LIS.getInstructionIndex(*MI).getRegSlot();
  bool IsEnd = LI->expiredAt(SI);
  LLVM_DEBUG(dbgs() << "LI:" << *LI << ", reg: " << Reg << '\n'
                    << (IsEnd ? "ends at: " : "doesn't end at: ") << SI << ",\n"
                    << *MI << '\n');
  return IsEnd;
}

void StackModel::migrateToEStack(const Register &Reg,
                                 const StackLocation &Loc) {
  auto &Stack = getStack(Loc.Type);
  assert(Reg == Stack.front());
  Stack.pop_front();
  // Put the register at the bottom of the E-stack.
  EStack.push_back(Reg);
}

unsigned StackModel::getPhysRegDepth(const StackLocation &Loc) const {
  unsigned Depth = EStack.size();
  switch (Loc.Type) {
  case StackType::X:
    Depth += (Loc.Depth + LStack.size());
    break;
  case StackType::L:
    Depth += Loc.Depth;
    break;
  default:
    llvm_unreachable("Unexpected stack type");
    break;
  }
  return Depth;
}

unsigned StackModel::getPhysRegDepth(const Register &Reg) const {
  const auto &Loc = getStackLocation(Reg);
  assert(Loc);
  return getPhysRegDepth(*Loc);
}

MachineInstr *StackModel::loadRegToPhysStackTop(unsigned Depth,
                                                MachineBasicBlock *MBB,
                                                MIIter Insert,
                                                const DebugLoc &DL) {
  EStack.emplace_front(0);
  return BuildMI(*MBB, Insert, DL, TII->get(getDUPOpcode(Depth)));
}

MachineInstr *StackModel::loadRegToPhysStackTop(const Register &Reg,
                                                MachineBasicBlock *MBB,
                                                MIIter Insert,
                                                const DebugLoc &DL) {
  if (!migrateTopRegToEStack(Reg, &*Insert))
    return loadRegToPhysStackTop(getPhysRegDepth(Reg), MBB, Insert, DL);

  return &*Insert;
}

SmallVector<RegisterLocation>::const_iterator
StackModel::migrateTopRegsToEStack(const SmallVector<RegisterLocation> &RegLoc,
                                   MachineInstr *MI) {
  assert(EStack.empty());

  const auto B = RegLoc.begin(), E = RegLoc.end();
  if (RegLoc.empty())
    return E;

  const auto *const StartUse = std::find_if(B, E, [this](const auto &Loc) {
    return getPhysRegDepth(Loc.first) == 0;
  });
  for (const auto *It = StartUse; It != E; ++It) {
    const Register &Reg = It->first;
    const unsigned Depth = getPhysRegDepth(Reg);
    if (It->second.Type == StackType::X ||
        Depth != std::distance(StartUse, It) || !isRegisterKill(Reg, MI))
      return E;
  }

  for (const auto *It = StartUse; It != E; ++It)
    migrateToEStack(It->first, It->second);

  return StartUse;
}

bool StackModel::migrateTopRegToEStack(const Register &Reg, MachineInstr *MI) {
  const auto Uses = MI->explicit_uses();
  const unsigned NumUses =
      std::count_if(Uses.begin(), Uses.end(), [Reg](const MachineOperand &MO) {
        if (MO.isReg())
          return Reg == MO.getReg();
        return false;
      });

  if (NumUses > 1)
    return false;

  const auto &Loc = getStackLocation(Reg);
  assert(Loc);

  if (Loc->Type == StackType::X || getPhysRegDepth(Reg) != 0 ||
      !isRegisterKill(Reg, MI))
    return false;

  migrateToEStack(Reg, *Loc);
  return true;
}

MachineInstr *StackModel::storeRegToPhysStack(const Register &Reg,
                                              MIIter Insert) {
  // Check if the reg is already located in either X or L stack.
  // If so, just return its location.
  const auto LocOpt = getStackLocation(Reg);

  const StackLocation &Loc = LocOpt ? *LocOpt : allocateStackModelLoc(Reg);
  unsigned Depth = getPhysRegDepth(Loc);
  assert(Depth > 0 && Loc.Type != StackType::E);

  // Perform migration E -> L/X stack.
  // If the reg should be put on a newly created L/X stack top location,
  // just do nothing.
  if (Loc.Depth == 0 && !LocOpt) {
    assert(EStack.size() == 1);
    EStack.pop_back();
    return &*Insert;
  }
  return storeRegToPhysStackAt(Depth, Insert->getParent(), std::next(Insert),
                               Insert->getDebugLoc());
}

MachineInstr *StackModel::storeRegToPhysStackAt(unsigned Depth,
                                                MachineBasicBlock *MBB,
                                                MIIter Insert,
                                                const DebugLoc &DL) {
  // Use the SWAP + POP instructions pair to store the register
  // in the physical stack on the given depth.
  const unsigned SWAPOpc = getSWAPOpcode(Depth);
  Insert = BuildMI(*MBB, Insert, DL, TII->get(SWAPOpc));
  Insert = BuildMI(*MBB, std::next(Insert), DL, TII->get(EVM::POP));
  EStack.pop_front();
  return &*Insert;
}

StackLocation StackModel::pushRegToStackModel(StackType Type,
                                              const Register &Reg) {
  StackLocation Loc{Type, 0};
  auto &Stack = getStack(Type);
  assert(std::find(Stack.begin(), Stack.end(), Reg) == std::end(Stack));
  Stack.push_front(Reg);
  return Loc;
}

StackLocation StackModel::allocateStackModelLoc(const Register &Reg) {
  // We need to add the register to either X or L-stack, depending
  // on reg's live interval. The register goes to X stack if its live interval
  // spawns across a BB boundary, otherwise put it to L-stack.
  const auto *LI = &LIS.getInterval(Reg);
  if (const auto *BB = LIS.intervalIsInOneMBB(*LI))
    return pushRegToStackModel(StackType::L, Reg);

  return pushRegToStackModel(StackType::X, Reg);
}

void StackModel::handleArgument(MachineInstr *MI) {
  const auto ArgIdx = MI->getOperand(1).getImm();
  // 1 - because of the return address.
  unsigned Depth = 1 + static_cast<unsigned>(ArgIdx) + getStackSize();
  loadRegToPhysStackTop(Depth, MI);
  storeRegToPhysStack(MI->getOperand(0).getReg(), MI);
  ToErase.push_back(MI);
}

void StackModel::handleLStackAtJump(MachineBasicBlock *MBB, MachineInstr *MI,
                                    const Register &Reg) {
  // If the condition register is in the L-stack, we need to move it to
  // the bottom of the L-stack. After that we should clean clean the L-stack.
  // In case of an unconditional jump, the Reg value should be
  // EVM::NoRegister.
  clearPhysStackAtInst(StackType::L, MI, Reg);

  // Insert "PUSH_LABEL %bb" instruction that should be be replaced with
  // the actual PUSH* one in the MC layer to contain actual jump target
  // offset.
  BuildMI(*MI->getParent(), MI, DebugLoc(), TII->get(EVM::PUSH_LABEL))
      .addMBB(MBB);

  // Add JUMPDEST at the beginning of the target MBB.
  if (MBB->empty() || MBB->begin()->getOpcode() != EVM::JUMPDEST)
    BuildMI(*MBB, MBB->begin(), DebugLoc(), TII->get(EVM::JUMPDEST));
}

void StackModel::handleCondJump(MachineInstr *MI) {
  MachineBasicBlock *TargetMBB = MI->getOperand(0).getMBB();
  const Register &CondReg = MI->getOperand(1).getReg();
  loadRegToPhysStackTop(CondReg, MI);
  handleLStackAtJump(TargetMBB, MI, CondReg);
}

void StackModel::handleJump(MachineInstr *MI) {
  MachineBasicBlock *TargetMBB = MI->getOperand(0).getMBB();
  handleLStackAtJump(TargetMBB, MI, EVM::NoRegister);
}

void StackModel::handleReturn(MachineInstr *MI) {
  ToErase.push_back(MI);
  BuildMI(*MI->getParent(), std::next(MIIter(MI)), DebugLoc(),
          TII->get(EVM::JUMP));

  // Collect the use registers of the RET instruction.
  SmallVector<Register> ReturnRegs;
  for (const auto &MO : MI->explicit_uses()) {
    assert(MO.isReg());
    ReturnRegs.push_back(MO.getReg());
  }

  auto *MFI = MF->getInfo<EVMMachineFunctionInfo>();
  if (MFI->getNumParams() >= ReturnRegs.size()) {
    // Move the return registers to the stack location where
    // arguments were resided.
    unsigned RegCount = 0;
    for (const auto &Reg : reverse(ReturnRegs)) {
      loadRegToPhysStackTop(Reg, MI);
      unsigned Depth = getStackSize() + MFI->getNumParams() - RegCount;
      storeRegToPhysStackAt(Depth, MI->getParent(), MI, MI->getDebugLoc());
      ++RegCount;
    }

    if (MFI->getNumParams() != ReturnRegs.size()) {
      // Now move the return address to the final location.
      loadRegToPhysStackTop(getStackSize(), MI);
      storeRegToPhysStackAt(getStackSize() + MFI->getNumParams() - RegCount,
                            MI->getParent(), MI, MI->getDebugLoc());
    }

    // Clear the phys stack.
    // Save both NumFrameObjs and X-tack model to restore after clearing the
    // stack, because this BB may not be the last in the CFG layout.
    std::deque<Register> CopyXStack = XStack;
    unsigned CopyNumFrameObjs = NumFrameObjs;

    // First clear the L and X  areas.
    clearPhysStackAtInst(StackType::X, MI, EVM::NoRegister);
    clearFrameObjsAtInst(MI);
    // Then clear a stack part corresponding to the arguments area.
    unsigned NumSlotsToPop = MFI->getNumParams() - RegCount;
    while (NumSlotsToPop--)
      BuildMI(*MI->getParent(), MI, MI->getDebugLoc(), TII->get(EVM::POP));

    XStack = CopyXStack;
    NumFrameObjs = CopyNumFrameObjs;
  } else {
    // Load return address to the top, as its old location will be overwritten.
    loadRegToPhysStackTop(getStackSize(), MI);

    // Move the last 'MFI->getNumParams() + 1' return registers to the final
    // locations.
    // (1 - because we also overwrite the return address stack location)
    for (unsigned RegCount = 0; RegCount < MFI->getNumParams() + 1;
         ++RegCount) {
      loadRegToPhysStackTop(ReturnRegs.pop_back_val(), MI);
      unsigned Depth = getStackSize() + MFI->getNumParams() - RegCount;
      storeRegToPhysStackAt(Depth, MI->getParent(), MI, MI->getDebugLoc());
    }

    // Save X-tack model and restore it later, because this BB may not be the
    // last in the CFG layout.
    std::deque<Register> CopyXStack = XStack;
    unsigned CopyNumFrameObjs = NumFrameObjs;

    // Copy all the remaining return registers to the top
    // locations of the stack.
    for (const auto &Reg : ReturnRegs)
      loadRegToPhysStackTop(Reg, MI);

    // Now copy them and the return address to the final locations.
    for (unsigned RegCount = 0; RegCount < ReturnRegs.size() + 1; ++RegCount) {
      unsigned Depth = getStackSize() - RegCount - 1;
      storeRegToPhysStackAt(Depth, MI->getParent(), MI, MI->getDebugLoc());
    }

    // Clear the phys stack.
    assert(getStackSize() >= ReturnRegs.size() + 1);
    unsigned NumSlotsToPop = getStackSize() - ReturnRegs.size() - 1;
    while (NumSlotsToPop--)
      BuildMI(*MI->getParent(), MI, MI->getDebugLoc(), TII->get(EVM::POP));

    LStack.clear();
    XStack = CopyXStack;
    NumFrameObjs = CopyNumFrameObjs;
  }
}

void StackModel::handleStackLoad(MachineInstr *MI) {
  APInt Offset = MI->getOperand(2).getCImm()->getValue();
  assert((Offset.getZExtValue() % 32) == 0);
  unsigned Depth =
      getStackSize() - static_cast<unsigned>(Offset.getZExtValue() / 32) - 1;
  BuildMI(*MI->getParent(), *MI, MI->getDebugLoc(),
          TII->get(getDUPOpcode(Depth)));
  ToErase.push_back(MI);
}

void StackModel::handleStackStore(MachineInstr *MI) {
  APInt Offset = MI->getOperand(1).getCImm()->getValue();
  assert((Offset.getZExtValue() % 32) == 0);
  unsigned Depth =
      getStackSize() - static_cast<unsigned>(Offset.getZExtValue() / 32) - 1;
  const unsigned SWAPOpc = getSWAPOpcode(Depth);
  BuildMI(*MI->getParent(), MI, MI->getDebugLoc(), TII->get(SWAPOpc));
  BuildMI(*MI->getParent(), MI, MI->getDebugLoc(), TII->get(EVM::POP));
  ToErase.push_back(MI);
}

void StackModel::handleCall(MachineInstr *MI) {
  // Calling convention: caller first pushes arguments then the return address.
  // Callee removes them form the stack and pushes return values.

  MachineBasicBlock &MBB = *MI->getParent();
  // Create return destination.
  MIIter It = BuildMI(MBB, MI, MI->getDebugLoc(), TII->get(EVM::JUMPDEST));

  // Add symbol just after the jump that will be used as the return
  // address from the function.
  MCSymbol *RetSym = MF->getContext().createTempSymbol("FUNC_RET", true);

  // Create jump to the callee.
  It = BuildMI(MBB, It, MI->getDebugLoc(), TII->get(EVM::JUMP));
  It->setPostInstrSymbol(*MF, RetSym);

  // Create push of the return address.
  BuildMI(MBB, It, MI->getDebugLoc(), TII->get(EVM::PUSH_LABEL)).addSym(RetSym);

  // Create push of the callee's address.
  const MachineOperand *CalleeOp = MI->explicit_uses().begin();
  assert(CalleeOp->isGlobal());
  BuildMI(MBB, It, MI->getDebugLoc(), TII->get(EVM::PUSH_LABEL))
      .addGlobalAddress(CalleeOp->getGlobal());
}

void StackModel::clearFrameObjsAtInst(MachineInstr *MI) {
  while (NumFrameObjs--) {
    BuildMI(*MI->getParent(), MI, DebugLoc(), TII->get(EVM::POP));
  }
}

void StackModel::clearPhysStackAtInst(StackType TargetStackType,
                                      MachineInstr *MI, const Register &Reg) {
  auto &Stack = getStack(TargetStackType);
  // Completely clear the phys stack till the target stack.
  if (Reg == EVM::NoRegister) {
    if (TargetStackType == StackType::X)
      peelPhysStack(StackType::L, LStack.size(), MI->getParent(), MI);

    peelPhysStack(TargetStackType, Stack.size(), MI->getParent(), MI);
    return;
  }

  // If the X/L-stack is empty, that means the reg is located in the E-stack,
  // that means no need to store it deeper, as it can be directly consumed by
  // the MI.
  if (Stack.empty()) {
    assert(EStack.size() == 1);
    EStack.pop_front();
    return;
  }

  // Assign the Reg (in terms of the stack model) to the bottom of the
  // L or X stack.
  StackLocation NewLoc = {TargetStackType,
                          static_cast<unsigned>(Stack.size() - 1)};

  // Store the Reg to the physical stack at the new location.
  unsigned Depth = getPhysRegDepth(NewLoc);
  storeRegToPhysStackAt(Depth, MI->getParent(), MI, MI->getDebugLoc());
  assert(EStack.empty());

  // If the Reg is placed at the bottom of the X stack, first we need to
  // completely clear L stack.
  if (TargetStackType == StackType::X)
    peelPhysStack(StackType::L, LStack.size(), MI->getParent(), MI);

  // Remove all stack items, but the last one from the target stack.
  peelPhysStack(TargetStackType, NewLoc.Depth, MI->getParent(), MI);

  // Pop CondReg from the L-stack, as JUMPI consumes CondReg.
  Stack.pop_front();
}

void StackModel::peelPhysStack(StackType Type, unsigned NumItems,
                               MachineBasicBlock *BB, MIIter Pos) {
  auto &Stack = getStack(Type);
  while (NumItems--) {
    BuildMI(*BB, Pos, DebugLoc(), TII->get(EVM::POP));
    Stack.pop_front();
  }
}

void StackModel::allocateXStack() {
  MachineRegisterInfo &MRI = MF->getRegInfo();
  MachineBasicBlock &Entry = MF->front();
  for (unsigned I = 0; I < MRI.getNumVirtRegs(); ++I) {
    const Register &Reg = Register::index2VirtReg(I);
    const LiveInterval *LI = &LIS.getInterval(Reg);
    if (LI->empty())
      continue;

    if (!LIS.intervalIsInOneMBB(*LI)) {
      LLVM_DEBUG(dbgs() << "\tallocation X-stack for: " << *LI << '\n');
      BuildMI(Entry, Entry.begin(), DebugLoc(), TII->get(EVM::PUSH0));
      XStack.push_front(Reg);
    }
  }
}

void StackModel::allocateFrameObjects() {
  MachineBasicBlock &Entry = MF->front();
  const MachineFrameInfo &MFI = MF->getFrameInfo();
  unsigned FrameSize = 0;
  for (unsigned I = 0, E = MFI.getObjectIndexEnd(); I != E; ++I) {
    // We don't have variable sized objects that have size 0.
    assert(MFI.getObjectSize(I));
    FrameSize += static_cast<unsigned>(MFI.getObjectSize(I));
  }
  assert((FrameSize % 32) == 0);

  NumFrameObjs = FrameSize / 32;
  for (unsigned I = 0; I < NumFrameObjs; ++I) {
    LLVM_DEBUG(dbgs() << "\tallocating frame object\n");
    BuildMI(Entry, Entry.begin(), DebugLoc(), TII->get(EVM::PUSH0));
  }
}

void StackModel::preProcess() {
  assert(!MF->empty());
  allocateFrameObjects();
  allocateXStack();
  // Add JUMPDEST at the beginning of the first MBB,
  // so this function can be jumped to.
  MachineBasicBlock &MBB = MF->front();
  BuildMI(MBB, MBB.begin(), DebugLoc(), TII->get(EVM::JUMPDEST));
}

// Remove all registers operands of the \p MI and repaces the opcode with
// the stack variant variant.
void StackModel::stackifyInstruction(MachineInstr *MI) {
  if (MI->isDebugInstr() || MI->isLabel() || MI->isInlineAsm())
    return;

  unsigned RegOpcode = MI->getOpcode();
  if (RegOpcode == EVM::PUSH_LABEL)
    return;

  // Remove register operands.
  for (unsigned I = MI->getNumOperands(); I > 0; --I) {
    auto &MO = MI->getOperand(I - 1);
    if (MO.isReg()) {
      MI->removeOperand(I - 1);
    }
  }

  // Transform 'register' instruction to the 'stack' one.
  unsigned StackOpcode = EVM::getStackOpcode(RegOpcode);
  MI->setDesc(TII->get(StackOpcode));
}

void StackModel::postProcess() {
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == EVM::CONST_I256) {
        // Replace with PUSH* opcode and remove register operands
        const APInt Imm = MI.getOperand(1).getCImm()->getValue();
        unsigned Opc = EVM::getPUSHOpcode(Imm);
        MI.setDesc(TII->get(Opc));
        MI.removeOperand(0);
        if (Opc == EVM::PUSH0)
          MI.removeOperand(0);
      } else if (MI.getOpcode() == EVM::COPY_I256 ||
                 MI.getOpcode() == EVM::POP_KILL) {
        // Just remove the copy instruction, as the code that
        // loads the argument and stores the result performs copying.
        // TODO: POP_KILL should be handled before stackification pass.
        ToErase.push_back(&MI);
      } else if (MI.getOpcode() == EVM::FCALL) {
        handleCall(&MI);
        ToErase.push_back(&MI);
      }
    }
  }

  for (auto *MI : ToErase)
    MI->eraseFromParent();

  for (MachineBasicBlock &MBB : *MF)
    for (MachineInstr &MI : MBB)
      stackifyInstruction(&MI);

  auto *MFI = MF->getInfo<EVMMachineFunctionInfo>();
  MFI->setIsStackified();

  // In a stackified code register liveness has no meaning.
  MachineRegisterInfo &MRI = MF->getRegInfo();
  MRI.invalidateLiveness();

  // In EVM architecture jump target is set up using one of PUSH* instructions
  // that come right before the jump instruction.
  // For example:

  //   PUSH_LABEL %bb.10
  //   JUMPI_S
  //   PUSH_LABEL %bb.9
  //   JUMP_S
  //
  // The problem here is that such MIR is not valid. There should not be
  // non-terminator (PUSH) instructions between terminator (JUMP) ones.
  // To overcome this issue, we bundle adjacent <PUSH_LABEL, JUMP> instructions
  // together and unbundle them in the AsmPrinter.
  for (MachineBasicBlock &MBB : *MF) {
    MachineBasicBlock::instr_iterator I = MBB.instr_begin(),
                                      E = MBB.instr_end();
    for (; I != E; ++I) {
      if (I->isBranch()) {
        auto P = std::next(I);
        if (P != E && P->getOpcode() == EVM::PUSH_LABEL)
          I->bundleWithPred();
      }
    }
  }
}

void StackModel::dumpState() const {
#ifndef NDEBUG
  LLVM_DEBUG(dbgs() << "X: " << XStack.size() << ", L: " << LStack.size()
                    << ", E: " << EStack.size() << '\n');
  for (const auto &Reg : LStack) {
    const auto Loc = *getStackLocation(Reg);
    LLVM_DEBUG(dbgs() << "reg: " << Reg
                      << ", Type: " << static_cast<unsigned>(Loc.Type)
                      << ", Depth: " << getPhysRegDepth(Reg) << '\n');
  }
  for (const auto &Reg : XStack) {
    const auto Loc = *getStackLocation(Reg);
    LLVM_DEBUG(dbgs() << "reg: " << Reg
                      << ", Type: " << static_cast<unsigned>(Loc.Type)
                      << ", Depth: " << getPhysRegDepth(Reg) << '\n');
  }
  LLVM_DEBUG(dbgs() << '\n');
#endif
}

unsigned StackModel::getDUPOpcode(unsigned Depth) {
  assert(Depth < StackAccessDepthLimit);
  switch (Depth) {
  case 0:
    return EVM::DUP1;
  case 1:
    return EVM::DUP2;
  case 2:
    return EVM::DUP3;
  case 3:
    return EVM::DUP4;
  case 4:
    return EVM::DUP5;
  case 5:
    return EVM::DUP6;
  case 6:
    return EVM::DUP7;
  case 7:
    return EVM::DUP8;
  case 8:
    return EVM::DUP9;
  case 9:
    return EVM::DUP10;
  case 10:
    return EVM::DUP11;
  case 11:
    return EVM::DUP12;
  case 12:
    return EVM::DUP13;
  case 13:
    return EVM::DUP14;
  case 14:
    return EVM::DUP15;
  case 15:
    return EVM::DUP16;
  default:
    llvm_unreachable("Unexpected stack depth");
  }
}

unsigned StackModel::getSWAPOpcode(unsigned Depth) {
  assert(Depth < StackAccessDepthLimit);
  switch (Depth) {
  case 1:
    return EVM::SWAP1;
  case 2:
    return EVM::SWAP2;
  case 3:
    return EVM::SWAP3;
  case 4:
    return EVM::SWAP4;
  case 5:
    return EVM::SWAP5;
  case 6:
    return EVM::SWAP6;
  case 7:
    return EVM::SWAP7;
  case 8:
    return EVM::SWAP8;
  case 9:
    return EVM::SWAP9;
  case 10:
    return EVM::SWAP10;
  case 11:
    return EVM::SWAP11;
  case 12:
    return EVM::SWAP12;
  case 13:
    return EVM::SWAP13;
  case 14:
    return EVM::SWAP14;
  case 15:
    return EVM::SWAP15;
  case 16:
    return EVM::SWAP16;
  default:
    llvm_unreachable("Unexpected stack depth");
  }
}

namespace {
class EVMStackify final : public MachineFunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  EVMStackify() : MachineFunctionPass(ID) {}

private:
  StringRef getPassName() const override { return "EVM stackification"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<MachineDominatorTree>();
    AU.addRequired<LiveIntervals>();
    AU.addPreserved<MachineBlockFrequencyInfo>();
    AU.addPreserved<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::TracksLiveness);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void setStackOpcodes(MachineFunction &MF);
};
} // end anonymous namespace

char EVMStackify::ID = 0;
INITIALIZE_PASS(EVMStackify, DEBUG_TYPE,
                "Insert stack manipulation instructions to execute the code in "
                "stack environment",
                false, false)

FunctionPass *llvm::createEVMStackify() { return new EVMStackify(); }

bool EVMStackify::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG({
    dbgs() << "********** Perform EVM stackification **********\n"
           << "********** Function: " << MF.getName() << '\n';
  });

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto *TII = MF.getSubtarget<EVMSubtarget>().getInstrInfo();
  auto &LIS = getAnalysis<LiveIntervals>();

  // We don't preserve SSA form.
  MRI.leaveSSA();

  assert(MRI.tracksLiveness() && "Stackify expects liveness");

  LLVM_DEBUG(dbgs() << "ALL register intervals:\n");
  for (unsigned I = 0; I < MRI.getNumVirtRegs(); ++I) {
    const Register &VReg = Register::index2VirtReg(I);
    LiveInterval *LI = &LIS.getInterval(VReg);
    if (LI->empty())
      continue;

    LIS.shrinkToUses(LI);
    LLVM_DEBUG(LI->dump());
    if (const auto *BB = LIS.intervalIsInOneMBB(*LI)) {
      LLVM_DEBUG(dbgs() << "\tIs live in: (" << BB->getName() << ")\n");
    }
  }
  LLVM_DEBUG(dbgs() << '\n');

  StackModel Model(&MF, LIS, TII);
  Model.preProcess();
  for (MachineBasicBlock &MBB : MF) {
    Model.enterBB(&MBB);
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      LLVM_DEBUG(dbgs() << MI);
      Model.handleInstruction(&MI);
      Model.dumpState();
    }
    Model.leaveBB(&MBB);
  }
  Model.postProcess();

  return true;
}
