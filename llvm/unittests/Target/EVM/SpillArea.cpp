//===---------- llvm/unittests/MC/AssemblerTest.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm-c/Core.h"
#include "llvm-c/IRReader.h"
#include "llvm-c/TargetMachine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Target/TargetMachine.h"
#include "gtest/gtest.h"
#include <memory>

using namespace llvm;

class SpillAreaTest : public testing::Test {
  void SetUp() override {
    LLVMInitializeEVMTargetInfo();
    LLVMInitializeEVMTarget();
    LLVMInitializeEVMTargetMC();
    LLVMInitializeEVMAsmPrinter();

    LLVMTargetRef Target = nullptr;
    const char *Triple = "evm";
    char *ErrMsg = nullptr;
    if (LLVMGetTargetFromTriple(Triple, &Target, &ErrMsg)) {
      FAIL() << "Failed to create target from the triple (" << Triple
             << "): " << ErrMsg;
      return;
    }
    ASSERT_TRUE(Target);

    TM =
        LLVMCreateTargetMachine(Target, Triple, "", "", LLVMCodeGenLevelDefault,
                                LLVMRelocDefault, LLVMCodeModelDefault);
    Context = LLVMContextCreate();
  }

  void TearDown() override { LLVMDisposeTargetMachine(TM); }

public:
  LLVMTargetMachineRef TM = nullptr;
  LLVMContextRef Context = nullptr;
};

TEST_F(SpillAreaTest, Basic) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
                                                                  \n\
define i256 @foo() nounwind {                                     \n\
  ret i256 0                                                      \n\
}";

  LLVMMemoryBufferRef IrBuf = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "src", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef Module = nullptr;
  if (LLVMParseIRInContext(Context, IrBuf, &Module, &ErrMsg)) {
    LLVMDisposeMessage(ErrMsg);
    exit(1);
  }

  LLVMMemoryBufferRef Result = nullptr;
  unwrap(Context)->setSpillAreaSize(32);
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, Module, LLVMObjectFile, &ErrMsg,
                                          &Result)) {
    EXPECT_TRUE(LLVMGetSpillAreaSizeEVM(Context) == 32);
    EXPECT_TRUE(!StringRef(ErrMsg).compare(
        "Stackification requires a pre-allocated spill area"));
    LLVMDisposeMessage(ErrMsg);
  } else {
    EXPECT_TRUE(false);
  }
  LLVMDisposeModule(Module);
}
