//===---------- llvm/unittests/EVM/StackSlotBuilder.cpp -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMStackModel.h"
#include "EVMTargetMachine.h"
#include "MCTargetDesc/EVMMCTargetDesc.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/CodeGenCWrappers.h"
#include "llvm/Target/TargetMachine.h"
#include "gtest/gtest.h"

using namespace llvm;

class EVMStackModelTest : public testing::Test {
protected:
  static void SetUpTestCase() {
    InitializeAllTargetMCs();
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllAsmPrinters();
  }

  void SetUp() override {
    // Get target triple for EVM.
    Triple TargetTriple("evm");
    std::string Error;
    const Target *T = TargetRegistry::lookupTarget("", TargetTriple, Error);
    // Skip the test if target is not built.
    if (!T)
      GTEST_SKIP();

    // Get TargetMachine.
    TargetOptions Options;
    TM = std::unique_ptr<TargetMachine>(
        T->createTargetMachine(TargetTriple, "", "", Options, std::nullopt,
                               std::nullopt, CodeGenOptLevel::Default));
    if (!TM)
      GTEST_SKIP();

    Context = std::make_unique<LLVMContext>();
    Mod = std::make_unique<Module>("TestModule", *Context);
    Mod->setDataLayout(TM->createDataLayout());
    MachineModuleInfoWrapperPass *MMIWP =
        new MachineModuleInfoWrapperPass(TM.get());

    Type *const ReturnType = Type::getVoidTy(Mod->getContext());
    FunctionType *FunctionType = FunctionType::get(ReturnType, false);
    Function *const F = Function::Create(
        FunctionType, GlobalValue::InternalLinkage, "TestFunction", Mod.get());
    MF = &MMIWP->getMMI().getOrCreateMachineFunction(*F);

    LISWP = std::make_unique<LiveIntervalsWrapperPass>();
    StackModel = std::make_unique<EVMStackModel>(
        *MF, LISWP->getLIS(),
        MF->getSubtarget<EVMSubtarget>().stackDepthLimit());
  }

public:
  std::unique_ptr<TargetMachine> TM;
  std::unique_ptr<LLVMContext> Context;
  std::unique_ptr<LiveIntervalsWrapperPass> LISWP;
  std::unique_ptr<Module> Mod;
  std::unique_ptr<EVMStackModel> StackModel;
  MachineFunction *MF = nullptr;
};

TEST_F(EVMStackModelTest, LiteralSlot) {
  APInt Int0 = APInt(32, 0);
  APInt Int42 = APInt(32, 42);

  auto *LiteralSlot0 = StackModel->getLiteralSlot(Int0);
  auto *LiteralSlot0Copy = StackModel->getLiteralSlot(Int0);
  EXPECT_TRUE(LiteralSlot0 == LiteralSlot0Copy);

  auto *LiteralSlot42 = StackModel->getLiteralSlot(Int42);
  EXPECT_TRUE(LiteralSlot0 != LiteralSlot42);
  EXPECT_TRUE(LiteralSlot0->getValue() != LiteralSlot42->getValue());
}

TEST_F(EVMStackModelTest, VariableSlot) {
  MachineRegisterInfo &MRI = MF->getRegInfo();
  Register Reg1 = MRI.createVirtualRegister(&EVM::GPRRegClass);
  Register Reg2 = MRI.createVirtualRegister(&EVM::GPRRegClass);

  auto *RegSlot1 = StackModel->getRegisterSlot(Reg1);
  auto *RegSlot1Copy = StackModel->getRegisterSlot(Reg1);
  EXPECT_TRUE(RegSlot1 == RegSlot1Copy);

  auto *RegSlot2 = StackModel->getRegisterSlot(Reg2);
  EXPECT_TRUE(RegSlot1 != RegSlot2);
  EXPECT_TRUE(RegSlot1->getReg() != RegSlot2->getReg());
}

TEST_F(EVMStackModelTest, SymbolSlot) {
  MCInstrDesc MCID = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  auto *MI = MF->CreateMachineInstr(MCID, DebugLoc());
  auto MAI = MCAsmInfo();
  auto MC = std::make_unique<MCContext>(Triple("evm"), &MAI, nullptr, nullptr,
                                        nullptr, nullptr, false);
  MCSymbol *Sym1 = MC->createTempSymbol("sym1", false);
  MCSymbol *Sym2 = MC->createTempSymbol("sym2", false);

  auto *SymSlot1 = StackModel->getSymbolSlot(Sym1, MI);
  auto *SymSlot1Copy = StackModel->getSymbolSlot(Sym1, MI);
  EXPECT_TRUE(SymSlot1 == SymSlot1Copy);

  auto *SymSlot2 = StackModel->getSymbolSlot(Sym2, MI);
  EXPECT_TRUE(SymSlot1 != SymSlot2);
  EXPECT_TRUE(SymSlot1->getSymbol() != SymSlot2->getSymbol());
}

TEST_F(EVMStackModelTest, CallerReturnSlot) {
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();
  auto *Call = MF->CreateMachineInstr(TII->get(EVM::FCALL), DebugLoc());
  auto *Call2 = MF->CreateMachineInstr(TII->get(EVM::FCALL), DebugLoc());

  auto *RetSlot1 = StackModel->getCallerReturnSlot(Call);
  auto *RetSlot1Copy = StackModel->getCallerReturnSlot(Call);
  EXPECT_TRUE(RetSlot1 == RetSlot1Copy);

  auto *RetSlot2 = StackModel->getCallerReturnSlot(Call2);
  EXPECT_TRUE(RetSlot1 != RetSlot2);
  EXPECT_TRUE(RetSlot1->getCall() != RetSlot2->getCall());
}

TEST_F(EVMStackModelTest, CalleeReturnSlot) {
  // Be sure the slot for callee return (the MF) is a single one.
  EXPECT_TRUE(StackModel->getCalleeReturnSlot(MF) ==
              StackModel->getCalleeReturnSlot(MF));
}

TEST_F(EVMStackModelTest, UnusedSlot) {
  // Be sure the UnusedSlot is a single one.
  EXPECT_TRUE(EVMStackModel::getUnusedSlot() == EVMStackModel::getUnusedSlot());
}
