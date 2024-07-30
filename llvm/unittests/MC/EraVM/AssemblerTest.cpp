//===---------- llvm/unittests/MC/AssemblerTest.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm-c/Assembler.h"
#include "llvm-c/Core.h"
#include "llvm/Support/MemoryBuffer.h"
#include "gtest/gtest.h"
#include <string.h>

using namespace llvm;

class AssemblerCTest : public testing::Test {
  void SetUp() override {
    LLVMInitializeEraVMTargetInfo();
    LLVMInitializeEraVMTarget();
    LLVMInitializeEraVMTargetMC();
    LLVMInitializeEraVMAsmParser();
    LLVMInitializeEraVMAsmPrinter();

    LLVMTargetRef Target = 0;
    const char *Triple = "eravm";
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
  }

  void TearDown() override { LLVMDisposeTargetMachine(TM); }

public:
  LLVMTargetMachineRef TM;
};

TEST_F(AssemblerCTest, Basic) {
  StringRef Asm = "                                  \
    .text                                          \n\
    nop  stack+=[1 + r0]                           \n\
    add  @glob_initializer_0[0], r0, stack[@glob]  \n\
                                                   \n\
    get_glob:                                      \n\
        add  3, r0, r1                             \n\
        add  stack[@glob], r1, r1                  \n\
        ret                                        \n\
                                                   \n\
    .data                                          \n\
    glob:                                          \n\
        .cell  113                                 \n\
                                                   \n\
    .rodata                                        \n\
    glob_initializer_0:                            \n\
        .cell  113                                 \n\
                                                   \n\
    .text                                          \n\
    DEFAULT_UNWIND:                                \n\
        ret.panic.to_label r0, @DEFAULT_UNWIND     \n\
    DEFAULT_FAR_RETURN:                            \n\
        ret.ok.to_label r1, @DEFAULT_FAR_RETURN    \n\
    DEFAULT_FAR_REVERT:                            \n\
        ret.revert.to_label r1, @DEFAULT_FAR_REVERT";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer;
  char *ErrMsg = nullptr;
  if (LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to assembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  LLVMDisposeMemoryBuffer(AsmMemBuffer);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}

TEST_F(AssemblerCTest, NonNullTerminatedInput) {
  StringRef Asm = ".text\nxxxxx777";
  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), 5, "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer;
  char *ErrMsg = nullptr;
  // Return code 'true' denotes an error.
  if (LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to assembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeMemoryBuffer(AsmMemBuffer);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}

TEST_F(AssemblerCTest, AsmParserError) {
  StringRef Asm = "                                  \
    .text                                          \n\
    get_glob:                                      \n\
        add  3, r0, r44                            \n\
        add  stack[@glob], r1, r1                  \n\
        ret";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer;
  char *ErrMsg = nullptr;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("cannot parse operand"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(AsmMemBuffer);
}

TEST_F(AssemblerCTest, ToManyInstructionsError) {
  std::string Asm = ".text";
  // Max number of instructions is 2^16.
  for (unsigned Num = 0; Num < (1 << 16) + 1; ++Num)
    Asm += "\nadd r0, r0, r0";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer;
  char *ErrMsg = nullptr;
  if (LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to assembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Return code 'true' denotes an error.
  EXPECT_TRUE(LLVMExceedsSizeLimitEraVM(ObjMemBuffer, /*metadataSize=*/0));

  LLVMDisposeMemoryBuffer(AsmMemBuffer);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}

TEST_F(AssemblerCTest, BytecodeIsTooBigError) {
  std::string Asm = ".rodata\nglob_initializer:";
  // The bytecode size limit is (2^16 - 1) * 32 bytes.
  // Each .cell dierective gives 32 bytes.
  for (unsigned Num = 0; Num < (1 << 16) - 1; ++Num)
    Asm += "\n.cell 7";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer;
  char *ErrMsg = nullptr;
  if (LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to assembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Return code 'true' denotes an error.
  EXPECT_FALSE(LLVMExceedsSizeLimitEraVM(ObjMemBuffer, /*metadataSize=*/0));
  // With the metadata word we exceed the maximal size.
  EXPECT_TRUE(LLVMExceedsSizeLimitEraVM(ObjMemBuffer, /*metadataSize=*/32));

  LLVMDisposeMemoryBuffer(AsmMemBuffer);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}
