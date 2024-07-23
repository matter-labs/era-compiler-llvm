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
// NOLINTNEXTLINE(misc-header-include-cycle)
#include "gtest/gtest.h"

using namespace llvm;

class AssemblerCTest : public testing::Test {
  void SetUp() override {
    LLVMInitializeEraVMTargetInfo();
    LLVMInitializeEraVMTarget();
    LLVMInitializeEraVMTargetMC();
    LLVMInitializeEraVMAsmParser();
    LLVMInitializeEraVMAsmPrinter();

    LLVMTargetRef Target = nullptr;
    const char *Triple = "eravm";
    char *ErrMsg = nullptr;
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
  LLVMTargetMachineRef TM = nullptr;
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, Basic) {
  StringRef Asm = "                                    \
    .text                                            \n\
    incsp 1                                          \n\
    add  code[@glob_initializer_0], r0, stack[@glob] \n\
                                                     \n\
    get_glob:                                        \n\
        add  3, r0, r1                               \n\
        add  stack[@glob], r1, r1                    \n\
        ret                                          \n\
                                                     \n\
    .data                                            \n\
    glob:                                            \n\
        .cell  113                                   \n\
                                                     \n\
    .rodata                                          \n\
    glob_initializer_0:                              \n\
        .cell  113                                   \n\
                                                     \n\
    .text                                            \n\
    DEFAULT_UNWIND:                                  \n\
        pncl @DEFAULT_UNWIND                         \n\
    DEFAULT_FAR_RETURN:                              \n\
        retl r1, @DEFAULT_FAR_RETURN                 \n\
    DEFAULT_FAR_REVERT:                              \n\
        revl r1, @DEFAULT_FAR_REVERT";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer = nullptr;
  char *ErrMsg = nullptr;
  if (LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to assembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  LLVMDisposeMemoryBuffer(AsmMemBuffer);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, NonNullTerminatedInput) {
  StringRef Asm = ".text\nxxxxx777";
  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), 5, "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer = nullptr;
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

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, AsmParserError) {
  StringRef Asm = "                                  \
    .text                                          \n\
    get_glob:                                      \n\
        add  3, r0, r44                            \n\
        add  stack[@glob], r1, r1                  \n\
        ret";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer = nullptr;
  char *ErrMsg = nullptr;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("cannot parse operand"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(AsmMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, ToManyInstructionsError) {
  std::string Asm = ".text";
  // Max number of instructions is 2^16.
  for (unsigned Num = 0; Num < (1 << 16) + 1; ++Num)
    Asm += "\nadd r0, r0, r0";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer = nullptr;
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

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, BytecodeIsTooBigError) {
  std::string Asm = ".rodata\nglob_initializer:";
  // The bytecode size limit is (2^16 - 1) * 32 bytes.
  // Each .cell dierective gives 32 bytes.
  for (unsigned Num = 0; Num < (1 << 16) - 1; ++Num)
    Asm += "\n.cell 7";

  LLVMMemoryBufferRef AsmMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(Asm.data(), Asm.size(), "test", 0);
  LLVMMemoryBufferRef ObjMemBuffer = nullptr;
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
