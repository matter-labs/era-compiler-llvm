//===----- EVMFinalizeStackFrames.cpp - Finalize stack frames --*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass calculates stack size for each function and replaces frame indices
// with their offsets.
//
//===----------------------------------------------------------------------===//

#include "EVM.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "TargetInfo/EVMTargetInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "evm-finalize-stack-frames"
#define PASS_NAME "EVM finalize stack frames"

static cl::opt<uint64_t>
    StackRegionSize("evm-stack-region-size", cl::Hidden, cl::init(0),
                    cl::desc("Allocated stack region size"));

static cl::opt<uint64_t>
    StackRegionOffset("evm-stack-region-offset", cl::Hidden,
                      cl::init(std::numeric_limits<uint64_t>::max()),
                      cl::desc("Offset where the stack region starts"));

namespace {
class EVMFinalizeStackFrames : public ModulePass {
public:
  static char ID;

  EVMFinalizeStackFrames() : ModulePass(ID) {
    initializeEVMFinalizeStackFramesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;

  StringRef getPassName() const override { return PASS_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }

private:
  /// Calculate the stack allocation offsets for all stack objects.
  uint64_t calculateFrameObjectOffsets(MachineFunction &MF) const;

  /// Replace frame indices with their corresponding offsets.
  void replaceFrameIndices(MachineFunction &MF,
                           uint64_t StackRegionStart) const;
};
} // end anonymous namespace

char EVMFinalizeStackFrames::ID = 0;

INITIALIZE_PASS_BEGIN(EVMFinalizeStackFrames, DEBUG_TYPE, PASS_NAME, false,
                      false)
INITIALIZE_PASS_DEPENDENCY(MachineModuleInfoWrapperPass)
INITIALIZE_PASS_END(EVMFinalizeStackFrames, DEBUG_TYPE, PASS_NAME, false, false)

ModulePass *llvm::createEVMFinalizeStackFrames() {
  return new EVMFinalizeStackFrames();
}

uint64_t
EVMFinalizeStackFrames::calculateFrameObjectOffsets(MachineFunction &MF) const {
  // Bail out if there are no stack objects.
  auto &MFI = MF.getFrameInfo();
  if (!MFI.hasStackObjects())
    return 0;

  // Set the stack offsets for each object.
  uint64_t StackSize = 0;
  for (int I = 0, E = MFI.getObjectIndexEnd(); I != E; ++I) {
    if (MFI.isDeadObjectIndex(I))
      continue;

    MFI.setObjectOffset(I, StackSize);
    StackSize += MFI.getObjectSize(I);
  }

  assert(StackSize % 32 == 0 && "Stack size must be a multiple of 32 bytes");
  return StackSize;
}

void EVMFinalizeStackFrames::replaceFrameIndices(
    MachineFunction &MF, uint64_t StackRegionStart) const {
  auto &MFI = MF.getFrameInfo();
  assert(MFI.hasStackObjects() &&
         "Cannot replace frame indices without stack objects");

  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : make_early_inc_range(MBB)) {
      if (MI.getOpcode() != EVM::PUSH_FRAME)
        continue;

      assert(MI.getNumOperands() == 1 && "PUSH_FRAME must have one operand");
      MachineOperand &FIOp = MI.getOperand(0);
      assert(FIOp.isFI() && "Expected a frame index operand");

      // Replace the frame index with the corresponding stack offset.
      APInt Offset(256,
                   StackRegionStart + MFI.getObjectOffset(FIOp.getIndex()));
      unsigned PushOpc = EVM::getPUSHOpcode(Offset);
      auto NewMI = BuildMI(MBB, MI, MI.getDebugLoc(),
                           TII->get(EVM::getStackOpcode(PushOpc)));
      if (PushOpc != EVM::PUSH0)
        NewMI.addCImm(ConstantInt::get(MF.getFunction().getContext(), Offset));

      MI.eraseFromParent();
    }
  }
}

bool EVMFinalizeStackFrames::runOnModule(Module &M) {
  LLVM_DEBUG({ dbgs() << "********** Finalize stack frames **********\n"; });

  // Check if options for stack region size and offset are set correctly.
  if (StackRegionSize.getNumOccurrences()) {
    if (!StackRegionOffset.getNumOccurrences())
      report_fatal_error("Stack region offset must be set when stack region "
                         "size is set. Use --evm-stack-region-offset to set "
                         "the offset.");

    if (StackRegionOffset % 32 != 0)
      report_fatal_error("Stack region offset must be a multiple of 32 bytes.");
  }

  uint64_t TotalStackSize = 0;
  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  SmallVector<std::pair<MachineFunction *, uint64_t>, 8> ToReplaceFI;

  // Calculate the stack size for each function.
  for (Function &F : M) {
    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;

    uint64_t StackSize = calculateFrameObjectOffsets(*MF);
    if (StackSize == 0)
      continue;

    uint64_t StackRegionStart = StackRegionOffset + TotalStackSize;
    ToReplaceFI.emplace_back(MF, StackRegionStart);
    TotalStackSize += StackSize;

    LLVM_DEBUG({
      dbgs() << "Stack size for function " << MF->getName()
             << " is: " << StackSize
             << " and starting offset is: " << StackRegionStart << "\n";
    });
  }
  LLVM_DEBUG({ dbgs() << "Total stack size: " << TotalStackSize << "\n"; });

  // Check if it is valid to replace frame indices.
  if (TotalStackSize > 0) {
    if (TotalStackSize > StackRegionSize)
      report_fatal_error("Total stack size: " + Twine(TotalStackSize) +
                         " is larger than the allocated stack region size: " +
                         Twine(StackRegionSize));

    if (StackRegionSize > TotalStackSize)
      errs() << "warning: allocated stack region size: " +
                    Twine(StackRegionSize) +
                    " is larger than the total stack size: " +
                    Twine(TotalStackSize) + "\n";
  }

  // Replace frame indices with their offsets.
  for (auto &[MF, StackRegionStart] : ToReplaceFI)
    replaceFrameIndices(*MF, StackRegionStart);

  return TotalStackSize > 0;
}
