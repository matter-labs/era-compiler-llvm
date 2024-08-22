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
    LLVMInitializeEraVMDisassembler();

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
TEST_F(AssemblerCTest, NoBufferNullTerminator) {
  StringRef Asm = "                                  \
    .text                                          \n\
    get_glob:                                      \n\
        add  r1, r1, r1                            \n\
        ret";

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays)
  auto Buffer = std::make_unique<char[]>(Asm.size());
  std::memcpy(Buffer.get(), Asm.data(), Asm.size());
  StringRef StrBuffer(Buffer.get(), Asm.size());
  std::unique_ptr<MemoryBuffer> MemBuffer =
      MemoryBuffer::getMemBuffer(StrBuffer, "",
                                 /*RequiresNullTerminator=*/false);

  LLVMMemoryBufferRef AsmMemBuffer = llvm::wrap(MemBuffer.get());
  LLVMMemoryBufferRef ObjMemBuffer = nullptr;
  char *ErrMsg = nullptr;
  // Return code 'true' denotes an error.
  if (LLVMAssembleEraVM(TM, AsmMemBuffer, &ObjMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to assembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
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

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, Disassembler) {
  std::array<char, 12 * 8> Code = {
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x2d, 0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x04, 0x32, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x04, 0x2e,
      0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x04, 0x30, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x71};

  LLVMMemoryBufferRef BinMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      Code.data(), Code.size(), "test", 0);
  char *ErrMsg = nullptr;
  LLVMMemoryBufferRef DisasmMemBuffer = nullptr;
  // Return code 'true' denotes an error.
  if (LLVMDisassembleEraVM(TM, BinMemBuffer, 0,
                           LLVMDisassemblerEraVM_Option_OutputEncoding,
                           &DisasmMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to disassembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  StringRef Result(LLVMGetBufferStart(DisasmMemBuffer),
                   LLVMGetBufferSize(DisasmMemBuffer));
  EXPECT_TRUE(Result.contains("0: 00 01 00 00 00 00 00 02	incsp	1"));
  EXPECT_TRUE(Result.contains(
      "8: 00 00 00 02 00 00 00 47	add	code[2], r0, stack[r0]"));
  EXPECT_TRUE(Result.contains(
      "10: 00 00 00 00 01 00 00 31	add	stack[r0], r0, r1"));
  EXPECT_TRUE(Result.contains("18: 00 00 00 00 00 01 04 2d	ret"));
  EXPECT_TRUE(Result.contains("20: 00 00 00 04 00 00 04 32	pncl	4"));
  EXPECT_TRUE(Result.contains("28: 00 00 00 05 00 01 04 2e	retl	5"));
  EXPECT_TRUE(Result.contains("30: 00 00 00 06 00 01 04 30	revl	6"));
  EXPECT_TRUE(Result.contains("2:"));
  EXPECT_TRUE(Result.contains("	.cell 113"));

  LLVMDisposeMemoryBuffer(BinMemBuffer);
  LLVMDisposeMemoryBuffer(DisasmMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, DisassemblerNoConstants) {
  std::array<uint8_t, 12 * 8> Code = {
      0x00, 0x00, 0x00, 0x01, 0x01, 0x10, 0x00, 0x39, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x04, 0x2d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x04, 0x32,
      0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x04, 0x2e, 0x00, 0x00, 0x00, 0x04,
      0x00, 0x01, 0x04, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x3a, 0xc2, 0x25, 0x16, 0x8d, 0xf5, 0x42, 0x12,
      0xa2, 0x5c, 0x1c, 0x01, 0xfd, 0x35, 0xbe, 0xbf, 0xea, 0x40, 0x8f, 0xda,
      0xc2, 0xe3, 0x1d, 0xdd, 0x6f, 0x80, 0xa4, 0xbb, 0xf9, 0xa5, 0xf1, 0xcb};

  LLVMMemoryBufferRef BinMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<const char *>(Code.data()), Code.size(), "test", 0);
  char *ErrMsg = nullptr;
  LLVMMemoryBufferRef DisasmMemBuffer = nullptr;
  // Return code 'true' denotes an error.
  if (LLVMDisassembleEraVM(TM, BinMemBuffer, 0,
                           LLVMDisassemblerEraVM_Option_OutputEncoding,
                           &DisasmMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to disassembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  StringRef Result(LLVMGetBufferStart(DisasmMemBuffer),
                   LLVMGetBufferSize(DisasmMemBuffer));
  EXPECT_TRUE(
      Result.contains("0: 00 00 00 01 01 10 00 39	add	1, r1, r1"));
  EXPECT_TRUE(Result.contains("8: 00 00 00 00 00 01 04 2d	ret"));
  EXPECT_TRUE(Result.contains("10: 00 00 00 02 00 00 04 32	pncl	2"));
  EXPECT_TRUE(Result.contains("18: 00 00 00 03 00 01 04 2e	retl	3"));
  EXPECT_TRUE(Result.contains("20: 00 00 00 04 00 01 04 30	revl	4"));
  EXPECT_TRUE(Result.contains("28: 00 00 00 00 00 00 00 00	<padding>"));
  EXPECT_TRUE(Result.contains("30: 00 00 00 00 00 00 00 00	<padding>"));
  EXPECT_TRUE(Result.contains("38: 00 00 00 00 00 00 00 00	<padding>"));
  EXPECT_TRUE(Result.contains("40: 3a c2 25 16 8d f5 42 12	<metadata>"));
  EXPECT_TRUE(Result.contains("48: a2 5c 1c 01 fd 35 be bf	<metadata>"));
  EXPECT_TRUE(Result.contains("50: ea 40 8f da c2 e3 1d dd	<metadata>"));
  EXPECT_TRUE(Result.contains("58: 6f 80 a4 bb f9 a5 f1 cb	<metadata>"));

  LLVMDisposeMemoryBuffer(BinMemBuffer);
  LLVMDisposeMemoryBuffer(DisasmMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, DisassemblerOffset8) {
  std::array<char, 8 * 4> Code = {
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  LLVMMemoryBufferRef BinMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      Code.data(), Code.size(), "test", 0);
  char *ErrMsg = nullptr;
  LLVMMemoryBufferRef DisasmMemBuffer = nullptr;
  // Return code 'true' denotes an error.
  if (LLVMDisassembleEraVM(TM, BinMemBuffer, 8, 0, &DisasmMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to disassembly:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  StringRef Result(LLVMGetBufferStart(DisasmMemBuffer),
                   LLVMGetBufferSize(DisasmMemBuffer));
  EXPECT_TRUE(Result.contains("add	stack[r0], r0, r1"));

  LLVMDisposeMemoryBuffer(BinMemBuffer);
  LLVMDisposeMemoryBuffer(DisasmMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, DisassemblerErrPCIsNotMultiple8) {
  std::array<char, 8 * 4> Code = {
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  LLVMMemoryBufferRef BinMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      Code.data(), Code.size(), "test", 0);
  char *ErrMsg = nullptr;
  LLVMMemoryBufferRef DisasmMemBuffer = nullptr;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(
      LLVMDisassembleEraVM(TM, BinMemBuffer, 1, 0, &DisasmMemBuffer, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains(
      "Starting address isn't multiple of 8 (instruction size)"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, DisassemblerErrPCExceedsCodeSize) {
  std::array<char, 8 * 4> Code = {
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  LLVMMemoryBufferRef BinMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      Code.data(), Code.size(), "test", 0);
  char *ErrMsg = nullptr;
  LLVMMemoryBufferRef DisasmMemBuffer = nullptr;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(
      LLVMDisassembleEraVM(TM, BinMemBuffer, 64, 0, &DisasmMemBuffer, &ErrMsg));
  EXPECT_TRUE(
      StringRef(ErrMsg).contains("Starting address exceeds the bytecode size"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST_F(AssemblerCTest, DisassemblerErrCodeSizeIsNotMultiple32) {
  std::array<char, 9> Code = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

  LLVMMemoryBufferRef BinMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      Code.data(), Code.size(), "test", 0);
  char *ErrMsg = nullptr;
  LLVMMemoryBufferRef DisasmMemBuffer = nullptr;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(
      LLVMDisassembleEraVM(TM, BinMemBuffer, 0, 0, &DisasmMemBuffer, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains(
      "Bytecode size isn't multiple of 32 (word size)"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}
