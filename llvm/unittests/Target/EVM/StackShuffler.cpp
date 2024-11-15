//===---------- llvm/unittests/MC/AssemblerTest.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMControlFlowGraph.h"
#include "EVMRegisterInfo.h"
#include "EVMStackDebug.h"
#include "EVMStackShuffler.h"
#include "EVMTargetMachine.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm-c/Core.h"
#include "llvm-c/IRReader.h"
#include "llvm-c/TargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Target/CodeGenCWrappers.h"
#include "llvm/Target/TargetMachine.h"
#include "gtest/gtest.h"
#include <memory>
#include <string.h>

using namespace llvm;

#include <iostream>
static TargetMachine *unwrap(LLVMTargetMachineRef P) {
  return reinterpret_cast<TargetMachine *>(P);
}

class LLDCTest : public testing::Test {
  void SetUp() override {
    LLVMInitializeEVMTargetInfo();
    LLVMInitializeEVMTarget();
    LLVMInitializeEVMTargetMC();

    LLVMTargetRef Target = 0;
    const char *Triple = "evm";
    char *ErrMsg = 0;
    if (LLVMGetTargetFromTriple(Triple, &Target, &ErrMsg)) {
      FAIL() << "Failed to create target from the triple (" << Triple
             << "): " << ErrMsg;
      return;
    }
    ASSERT_TRUE(Target);

    // Construct a TargetMachine.
    TM =
        LLVMCreateTargetMachine(Target, Triple, "", "", LLVMCodeGenLevelDefault,
                                LLVMRelocDefault, LLVMCodeModelDefault);
    Context = std::make_unique<LLVMContext>();
    Mod = std::make_unique<Module>("TestModule", *Context);
    Mod->setDataLayout(unwrap(TM)->createDataLayout());
    const LLVMTargetMachine &LLVMTM =
        static_cast<const LLVMTargetMachine &>(*unwrap(TM));
    MMIWP = std::make_unique<MachineModuleInfoWrapperPass>(&LLVMTM);

    Type *const ReturnType = Type::getVoidTy(Mod->getContext());
    FunctionType *FunctionType = FunctionType::get(ReturnType, false);
    Function *const F = Function::Create(
        FunctionType, GlobalValue::InternalLinkage, "TestFunction", Mod.get());
    MF = &MMIWP->getMMI().getOrCreateMachineFunction(*F);
  }

  void TearDown() override { LLVMDisposeTargetMachine(TM); }

public:
  LLVMTargetMachineRef TM;
  MachineFunction *MF = nullptr;
  std::unique_ptr<LLVMContext> Context;
  std::unique_ptr<Module> Mod;
  std::unique_ptr<MachineModuleInfoWrapperPass> MMIWP;
};

TEST_F(LLDCTest, Basic) {
  Stack SourceStack;
  Stack TargetStack;

  MachineBasicBlock *MBB = MF->CreateMachineBasicBlock(nullptr);
  MF->push_back(MBB);

  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  const MCInstrDesc &MCID = TII->get(EVM::SELFBALANCE);
  MachineRegisterInfo &MRI = MF->getRegInfo();

  auto CreateInstr = [&]() {
    Register Reg = MRI.createVirtualRegister(&EVM::GPRRegClass);
    MachineInstr *MI = BuildMI(MBB, DebugLoc(), MCID, Reg);
    return std::pair(MI, Reg);
  };
  SmallVector<std::pair<MachineInstr *, Register>> Instrs;
  for (unsigned I = 0; I < 17; ++I)
    Instrs.emplace_back(CreateInstr());

  // Create the source stack layout:
  //   [ %0 %1 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET RET %5 ]
  SourceStack.emplace_back(VariableSlot{Instrs[0].second});
  SourceStack.emplace_back(VariableSlot{Instrs[1].second});
  SourceStack.emplace_back(VariableSlot{Instrs[2].second});
  SourceStack.emplace_back(VariableSlot{Instrs[3].second});
  SourceStack.emplace_back(VariableSlot{Instrs[4].second});
  SourceStack.emplace_back(VariableSlot{Instrs[5].second});
  SourceStack.emplace_back(VariableSlot{Instrs[6].second});
  SourceStack.emplace_back(VariableSlot{Instrs[7].second});
  SourceStack.emplace_back(VariableSlot{Instrs[9].second});
  SourceStack.emplace_back(VariableSlot{Instrs[10].second});
  SourceStack.emplace_back(VariableSlot{Instrs[11].second});
  SourceStack.emplace_back(VariableSlot{Instrs[12].second});
  SourceStack.emplace_back(VariableSlot{Instrs[13].second});
  SourceStack.emplace_back(VariableSlot{Instrs[14].second});
  SourceStack.emplace_back(VariableSlot{Instrs[15].second});
  SourceStack.emplace_back(VariableSlot{Instrs[16].second});
  SourceStack.emplace_back(FunctionReturnLabelSlot{MBB->getParent()});
  SourceStack.emplace_back(FunctionReturnLabelSlot{MBB->getParent()});
  SourceStack.emplace_back(VariableSlot{Instrs[5].second});

  // [ %1 %0 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET JUNK JUNK ]
  TargetStack.emplace_back(VariableSlot{Instrs[1].second});
  TargetStack.emplace_back(VariableSlot{Instrs[0].second});
  TargetStack.emplace_back(VariableSlot{Instrs[2].second});
  TargetStack.emplace_back(VariableSlot{Instrs[3].second});
  TargetStack.emplace_back(VariableSlot{Instrs[4].second});
  TargetStack.emplace_back(VariableSlot{Instrs[5].second});
  TargetStack.emplace_back(VariableSlot{Instrs[6].second});
  TargetStack.emplace_back(VariableSlot{Instrs[7].second});
  TargetStack.emplace_back(VariableSlot{Instrs[9].second});
  TargetStack.emplace_back(VariableSlot{Instrs[10].second});
  TargetStack.emplace_back(VariableSlot{Instrs[11].second});
  TargetStack.emplace_back(VariableSlot{Instrs[12].second});
  TargetStack.emplace_back(VariableSlot{Instrs[13].second});
  TargetStack.emplace_back(VariableSlot{Instrs[14].second});
  TargetStack.emplace_back(VariableSlot{Instrs[15].second});
  TargetStack.emplace_back(VariableSlot{Instrs[16].second});
  TargetStack.emplace_back(FunctionReturnLabelSlot{MBB->getParent()});
  TargetStack.emplace_back(JunkSlot{});
  TargetStack.emplace_back(JunkSlot{});

  StringRef Reference("\
[ %0 %1 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET RET %5 ]\n\
POP\n\
[ %0 %1 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET RET ]\n\
SWAP16\n\
[ %0 RET %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET %1 ]\n\
SWAP16\n\
[ %0 %1 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET RET ]\n\
POP\n\
[ %0 %1 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET ]\n\
SWAP15\n\
[ %0 RET %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 %1 ]\n\
SWAP16\n\
[ %1 RET %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 %0 ]\n\
SWAP15\n\
[ %1 %0 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET ]\n\
PUSH JUNK\n\
[ %1 %0 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET JUNK ]\n\
PUSH JUNK\n\
[ %1 %0 %2 %3 %4 %5 %6 %7 %9 %10 %11 %12 %13 %14 %15 %16 RET JUNK JUNK ]\n");

  std::ostringstream Output;
  createStackLayout(
      SourceStack, TargetStack,
      [&](unsigned SwapDepth) { // swap
        Output << stackToString(SourceStack) << '\n';
        Output << "SWAP" << SwapDepth << '\n';
      },
      [&](StackSlot const &Slot) { // dupOrPush
        Output << stackToString(SourceStack) << '\n';
        if (canBeFreelyGenerated(Slot))
          Output << "PUSH " << stackSlotToString(Slot) << '\n';
        else {
          Stack TmpStack = SourceStack;
          std::reverse(TmpStack.begin(), TmpStack.end());
          auto It = std::find(TmpStack.begin(), TmpStack.end(), Slot);
          if (It == TmpStack.end())
            FAIL() << "Invalid DUP operation.";

          auto Depth = std::distance(TmpStack.begin(), It);
          Output << "DUP" << Depth + 1 << '\n';
        }
      },
      [&]() { // pop
        Output << stackToString(SourceStack) << '\n';
        Output << "POP" << '\n';
      });

  Output << stackToString(SourceStack) << '\n';
  std::cerr << Output.str();
  EXPECT_TRUE(Reference == Output.str());
}
