//===---------- llvm/unittests/MC/AssemblerTest.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lld-c/LLDAsLibraryC.h"
#include "llvm-c/Core.h"
#include "llvm-c/IRReader.h"
#include "llvm-c/ObjCopy.h"
#include "llvm-c/TargetMachine.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/KECCAK.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <array>

using namespace llvm;

class LLDCTest : public testing::Test {
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

    Context = LLVMContextCreate();
  }

  void TearDown() override {
    LLVMDisposeTargetMachine(TM);
    LLVMContextDispose(Context);
  }

public:
  LLVMTargetMachineRef TM;
  LLVMContextRef Context;
};

TEST_F(LLDCTest, Basic) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
@glob = global i256 113                                           \n\
@glob2 = global i256 116                                          \n\
@glob.arr.as4 = addrspace(4) global [4 x i256] zeroinitializer    \n\
@glob_ptr_as3 = global i256* zeroinitializer                      \n\
@glob.arr = global [4 x i256] [i256 0, i256 29, i256 0, i256 4]   \n\
                                                                  \n\
define i256 @get_glob() nounwind {                                \n\
  %res = load i256, i256* @glob                                   \n\
  %res2 = add i256 %res, 3                                        \n\
  ret i256 %res2                                                  \n\
}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef IrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "test", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef M;
  if (LLVMParseIRInContext(Context, IrMemBuffer, &M, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffer.
  LLVMMemoryBufferRef ObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &ErrMsg,
                                          &ObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(M);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeModule(M);

  std::array<char, 32> MD = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

  LLVMMemoryBufferRef MDObjMemBuffer;
  if (LLVMAddMetadataEraVM(ObjMemBuffer, MD.data(), MD.size(), &MDObjMemBuffer,
                           &ErrMsg)) {
    errs() << "Failed to add metadata:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  LLVMMemoryBufferRef BinMemBuffer;
  if (LLVMLinkEraVM(MDObjMemBuffer, &BinMemBuffer, nullptr, nullptr, 0,
                    &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  StringRef MDVal(MD.data(), MD.size());
  StringRef Binary(LLVMGetBufferStart(BinMemBuffer),
                   LLVMGetBufferSize(BinMemBuffer));
  EXPECT_TRUE(Binary.take_back(MD.size()) == MDVal);
  EXPECT_TRUE(Binary.size() % 64 == 32);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
  LLVMDisposeMemoryBuffer(MDObjMemBuffer);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}

TEST_F(LLDCTest, MetadataIsNotMultiple32) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
@glob = global i256 113                                           \n\
                                                                  \n\
define i256 @get_glob() nounwind {                                \n\
  %res = load i256, i256* @glob                                   \n\
  %res2 = add i256 %res, 3                                        \n\
  ret i256 %res2                                                  \n\
}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef IrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "test", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef M;
  if (LLVMParseIRInContext(Context, IrMemBuffer, &M, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffer.
  LLVMMemoryBufferRef ObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &ErrMsg,
                                          &ObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(M);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeModule(M);

  std::array<char, 33> MD = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21};

  LLVMMemoryBufferRef MDObjMemBuffer;
  if (LLVMAddMetadataEraVM(ObjMemBuffer, MD.data(), MD.size(), &MDObjMemBuffer,
                           &ErrMsg)) {
    errs() << "Failed to add metadata:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  LLVMMemoryBufferRef BinMemBuffer;
  if (LLVMLinkEraVM(MDObjMemBuffer, &BinMemBuffer, nullptr, nullptr, 0,
                    &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  StringRef MDVal(MD.data(), MD.size());
  StringRef Binary(LLVMGetBufferStart(BinMemBuffer),
                   LLVMGetBufferSize(BinMemBuffer));
  EXPECT_TRUE(Binary.take_back(MD.size()) == MDVal);
  EXPECT_TRUE(Binary.size() % 64 == 32);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
  LLVMDisposeMemoryBuffer(MDObjMemBuffer);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}

TEST_F(LLDCTest, LinkerSymbol) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
target triple = \"eravm\"                                         \n\
declare i256 @llvm.eravm.linkersymbol(metadata)                   \n\
                                                                  \n\
define i256 @test() {                                             \n\
  %sym = call i256 @llvm.eravm.linkersymbol(metadata !1)          \n\
  %sym2 = call i256 @llvm.eravm.linkersymbol(metadata !2)         \n\
  %sym3 = call i256 @llvm.eravm.linkersymbol(metadata !3)         \n\
  %sym4 = call i256 @llvm.eravm.linkersymbol(metadata !4)         \n\
  %sym5 = call i256 @llvm.eravm.linkersymbol(metadata !5)         \n\
  %sym6 = call i256 @llvm.eravm.linkersymbol(metadata !6)         \n\
  %sym7 = call i256 @llvm.eravm.linkersymbol(metadata !7)         \n\
  %sym8 = call i256 @llvm.eravm.linkersymbol(metadata !8)         \n\
  %sym9 = call i256 @llvm.eravm.linkersymbol(metadata !9)         \n\
  %sym10 = call i256 @llvm.eravm.linkersymbol(metadata !10)       \n\
  %sym11 = call i256 @llvm.eravm.linkersymbol(metadata !11)       \n\
  %sym12 = call i256 @llvm.eravm.linkersymbol(metadata !12)       \n\
  %sym13 = call i256 @llvm.eravm.linkersymbol(metadata !13)       \n\
  %sym14 = call i256 @llvm.eravm.linkersymbol(metadata !14)       \n\
  %sym15 = call i256 @llvm.eravm.linkersymbol(metadata !15)       \n\
  %sym16 = call i256 @llvm.eravm.linkersymbol(metadata !16)       \n\
  %sym17 = call i256 @llvm.eravm.linkersymbol(metadata !17)       \n\
  %sym18 = call i256 @llvm.eravm.linkersymbol(metadata !18)       \n\
  %sym19 = call i256 @llvm.eravm.linkersymbol(metadata !19)       \n\
  %sym20 = call i256 @llvm.eravm.linkersymbol(metadata !20)       \n\
  %sym21 = call i256 @llvm.eravm.linkersymbol(metadata !21)       \n\
  %sym22 = call i256 @llvm.eravm.linkersymbol(metadata !22)       \n\
  %sym23 = call i256 @llvm.eravm.linkersymbol(metadata !23)       \n\
  %sym24 = call i256 @llvm.eravm.linkersymbol(metadata !24)       \n\
  %sym25 = call i256 @llvm.eravm.linkersymbol(metadata !25)       \n\
  %sym26 = call i256 @llvm.eravm.linkersymbol(metadata !26)       \n\
  %sym27 = call i256 @llvm.eravm.linkersymbol(metadata !27)       \n\
  %sym28 = call i256 @llvm.eravm.linkersymbol(metadata !28)       \n\
  %sym29 = call i256 @llvm.eravm.linkersymbol(metadata !29)       \n\
  %sym30 = call i256 @llvm.eravm.linkersymbol(metadata !30)       \n\
  %sym31 = call i256 @llvm.eravm.linkersymbol(metadata !31)       \n\
  %sym32 = call i256 @llvm.eravm.linkersymbol(metadata !32)       \n\
  %sym33 = call i256 @llvm.eravm.linkersymbol(metadata !33)       \n\
  %sym34 = call i256 @llvm.eravm.linkersymbol(metadata !34)       \n\
  %sym35 = call i256 @llvm.eravm.linkersymbol(metadata !35)       \n\
  %res = add i256 %sym, %sym2                                     \n\
  %res2 = add i256 %res, %sym3                                    \n\
  %res3 = add i256 %res2, %sym4                                   \n\
  %res4 = add i256 %res3, %sym5                                   \n\
  %res5 = add i256 %res4, %sym6                                   \n\
  %res6 = add i256 %res5, %sym7                                   \n\
  %res7 = add i256 %res6, %sym8                                   \n\
  %res8 = add i256 %res7, %sym9                                   \n\
  %res9 = add i256 %res8, %sym10                                  \n\
  %res10 = add i256 %res9, %sym11                                 \n\
  %res11 = add i256 %res10, %sym12                                \n\
  %res12 = add i256 %res11, %sym13                                \n\
  %res13 = add i256 %res12, %sym14                                \n\
  %res14 = add i256 %res13, %sym15                                \n\
  %res15 = add i256 %res14, %sym16                                \n\
  %res16 = add i256 %res15, %sym17                                \n\
  %res17 = add i256 %res16, %sym18                                \n\
  %res18 = add i256 %res17, %sym19                                \n\
  %res19 = add i256 %res18, %sym20                                \n\
  %res20 = add i256 %res19, %sym21                                \n\
  %res21 = add i256 %res20, %sym22                                \n\
  %res22 = add i256 %res21, %sym23                                \n\
  %res23 = add i256 %res22, %sym24                                \n\
  %res24 = add i256 %res23, %sym25                                \n\
  %res25 = add i256 %res24, %sym26                                \n\
  %res26 = add i256 %res25, %sym27                                \n\
  %res27 = add i256 %res26, %sym28                                \n\
  %res28 = add i256 %res27, %sym29                                \n\
  %res29 = add i256 %res28, %sym30                                \n\
  %res30 = add i256 %res29, %sym31                                \n\
  %res31 = add i256 %res30, %sym32                                \n\
  %res32 = add i256 %res31, %sym33                                \n\
  %res33 = add i256 %res32, %sym34                                \n\
  %res34 = add i256 %res33, %sym35                                \n\
  ret i256 %res34                                                 \n\
}                                                                 \n\
                                                                  \n\
!1 = !{!\"/file/path()`~!@#$%^&*-+=/library:id\"}                 \n\
!2 = !{!\"C:\\file\\path()`~!@#$%^&*-+=\\library:id2\"}           \n\
!3 = !{!\"~/file/path()`~!@#$%^&*-+=/library:id3\"}               \n\
!4 = !{!\"/()`~!@#$%^&*-+=|\\{}[ ]:;'<>,?/_library:id4\"}         \n\
!5 = !{!\".()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                    \n\
!6 = !{!\"()`~!@#$%^&*-+=|\\{}[]:;'<>,  .?/_\"}                   \n\
!7 = !{!\"!()`~!@#$%^&* - +=|\\{}[]:;'<>,.?/_\"}                  \n\
!8 = !{!\"`()`~!@#$%^& * -+=|\\{}[]:;'<>,.?/_\"}                  \n\
!9 = !{!\"!()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                    \n\
!10 = !{!\"@()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!11 = !{!\"#()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!12 = !{!\"$()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!13 = !{!\"%()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!14 = !{!\"^()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!15 = !{!\"&()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!16 = !{!\"*()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!17 = !{!\"-()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!18 = !{!\"+()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!19 = !{!\"=()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!20 = !{!\" =()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                  \n\
!21 = !{!\"|()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!22 = !{!\"\\()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                  \n\
!23 = !{!\"{()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!24 = !{!\"}()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!25 = !{!\"[()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!26 = !{!\"]()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!27 = !{!\":()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!28 = !{!\";()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!29 = !{!\"'()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!30 = !{!\"<()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!31 = !{!\">()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!32 = !{!\",()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!33 = !{!\"?()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!34 = !{!\"/()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}                   \n\
!35 = !{!\"_()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_\"}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef IrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "test", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef M;
  if (LLVMParseIRInContext(Context, IrMemBuffer, &M, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffer.
  LLVMMemoryBufferRef ObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &ErrMsg,
                                          &ObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(M);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeModule(M);

  LLVMMemoryBufferRef BinMemBuffer;
  const char *LinkerSymbol[35] = {
      "/file/path()`~!@#$%^&*-+=/library:id",
      "C:\\file\\path()`~!@#$%^&*-+=\\library:id2",
      "~/file/path()`~!@#$%^&*-+=/library:id3",
      "/()`~!@#$%^&*-+=|\\{}[ ]:;'<>,?/_library:id4",
      ".()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "()`~!@#$%^&*-+=|\\{}[]:;'<>,  .?/_",
      "!()`~!@#$%^&* - +=|\\{}[]:;'<>,.?/_",
      "`()`~!@#$%^& * -+=|\\{}[]:;'<>,.?/_",
      "!()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "@()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "#()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "$()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "%()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "^()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "&()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "*()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "-()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "+()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "=()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      " =()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "|()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "\\()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "{()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "}()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "[()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "]()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      ":()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      ";()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "'()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "<()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      ">()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      ",()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "?()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "/()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
      "_()`~!@#$%^&*-+=|\\{}[]:;'<>,.?/_",
  };
  const char LinkerSymbolVal[35][20] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
      {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
      {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
      {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
      {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
      {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
      {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
      {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9},
      {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
       10, 10, 10, 10, 10, 10, 10, 10, 10, 10},
      {11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
       11, 11, 11, 11, 11, 11, 11, 11, 11, 11},
      {12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
       12, 12, 12, 12, 12, 12, 12, 12, 12, 12},
      {13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
       13, 13, 13, 13, 13, 13, 13, 13, 13, 13},
      {14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
       14, 14, 14, 14, 14, 14, 14, 14, 14, 14},
      {15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
       15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
      {16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
       16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
      {17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
       17, 17, 17, 17, 17, 17, 17, 17, 17, 17},
      {18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
       18, 18, 18, 18, 18, 18, 18, 18, 18, 18},
      {19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
       19, 19, 19, 19, 19, 19, 19, 19, 19, 19},
      {20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
       20, 20, 20, 20, 20, 20, 20, 20, 20, 20},
      {21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
       21, 21, 21, 21, 21, 21, 21, 21, 21, 21},
      {22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
       22, 22, 22, 22, 22, 22, 22, 22, 22, 22},
      {23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
       23, 23, 23, 23, 23, 23, 23, 23, 23, 23},
      {24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
       24, 24, 24, 24, 24, 24, 24, 24, 24, 24},
      {25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
       25, 25, 25, 25, 25, 25, 25, 25, 25, 25},
      {26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
       26, 26, 26, 26, 26, 26, 26, 26, 26, 26},
      {27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
       27, 27, 27, 27, 27, 27, 27, 27, 27, 27},
      {28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
       28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
      {29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
       29, 29, 29, 29, 29, 29, 29, 29, 29, 29},
      {30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
       30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
      {31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
       31, 31, 31, 31, 31, 31, 31, 31, 31, 31},
      {32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
       32, 32, 32, 32, 32, 32, 32, 32, 32, 32},
      {33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
       33, 33, 33, 33, 33, 33, 33, 33, 33, 33},
      {34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
       34, 34, 34, 34, 34, 34, 34, 34, 34, 34},
      {35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
       35, 35, 35, 35, 35, 35, 35, 35, 35, 35},
  };
  if (LLVMLinkEraVM(ObjMemBuffer, &BinMemBuffer, LinkerSymbol, LinkerSymbolVal,
                    35, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  uint64_t NumUndefLinkerSymbols = 0;
  EXPECT_FALSE(LLVMIsELFEraVM(BinMemBuffer));
  char **UndefLinkerSymbols =
      LLVMGetUndefinedLinkerSymbolsEraVM(BinMemBuffer, &NumUndefLinkerSymbols);
  EXPECT_TRUE(NumUndefLinkerSymbols == 0);
  LLVMDisposeUndefinedLinkerSymbolsEraVM(UndefLinkerSymbols,
                                         NumUndefLinkerSymbols);

  StringRef Binary(LLVMGetBufferStart(BinMemBuffer),
                   LLVMGetBufferSize(BinMemBuffer));
  for (unsigned I = 0; I < 35; ++I) {
    StringRef Val(LinkerSymbolVal[I], 20);
    EXPECT_TRUE(Binary.find(Val) != StringRef::npos);
  }
  EXPECT_TRUE(LLVMGetBufferSize(BinMemBuffer) % 64 == 32);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}

TEST_F(LLDCTest, IterativeLinkage) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
target triple = \"eravm\"                                         \n\
declare i256 @llvm.eravm.linkersymbol(metadata)                   \n\
                                                                  \n\
define i256 @test() {                                             \n\
  %res = call i256 @llvm.eravm.linkersymbol(metadata !1)          \n\
  %res2 = call i256 @llvm.eravm.linkersymbol(metadata !2)         \n\
  %res3 = add i256 %res, %res2                                    \n\
  ret i256 %res3                                                  \n\
}                                                                 \n\
                                                                  \n\
!1 = !{!\"library_id\"}                                           \n\
!2 = !{!\"library_id2\"}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef IrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "test", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef M;
  if (LLVMParseIRInContext(Context, IrMemBuffer, &M, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffer.
  LLVMMemoryBufferRef ObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &ErrMsg,
                                          &ObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(M);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeModule(M);

  EXPECT_TRUE(LLVMIsELFEraVM(ObjMemBuffer));

  uint64_t NumUndefLinkerSymbols = 0;
  const char *LinkerSymbols[2] = {"library_id", "library_id2"};
  const char LinkerSymbolVals[2][20] = {
      {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5},
      {6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 11, 12, 13}};

  char **UndefLinkerSymbols =
      LLVMGetUndefinedLinkerSymbolsEraVM(ObjMemBuffer, &NumUndefLinkerSymbols);
  EXPECT_TRUE(NumUndefLinkerSymbols == 2);
  EXPECT_TRUE((std::strcmp(UndefLinkerSymbols[0], LinkerSymbols[0]) == 0) ||
              (std::strcmp(UndefLinkerSymbols[0], LinkerSymbols[1]) == 0));
  EXPECT_TRUE((std::strcmp(UndefLinkerSymbols[1], LinkerSymbols[0]) == 0) ||
              (std::strcmp(UndefLinkerSymbols[1], LinkerSymbols[1]) == 0));

  LLVMDisposeUndefinedLinkerSymbolsEraVM(UndefLinkerSymbols,
                                         NumUndefLinkerSymbols);

  // Pass only the first linker symbol.
  LLVMMemoryBufferRef Obj2MemBuffer;
  if (LLVMLinkEraVM(ObjMemBuffer, &Obj2MemBuffer, LinkerSymbols,
                    LinkerSymbolVals, 1, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(LLVMIsELFEraVM(Obj2MemBuffer));
  UndefLinkerSymbols =
      LLVMGetUndefinedLinkerSymbolsEraVM(Obj2MemBuffer, &NumUndefLinkerSymbols);
  EXPECT_TRUE(NumUndefLinkerSymbols == 1);
  EXPECT_TRUE(std::strcmp(UndefLinkerSymbols[0], LinkerSymbols[1]) == 0);

  LLVMDisposeUndefinedLinkerSymbolsEraVM(UndefLinkerSymbols,
                                         NumUndefLinkerSymbols);

  // Pass only the second linker symbol. This time
  // the linker should emit the final bytecode, as all the
  // symbols are resolved.
  LLVMMemoryBufferRef BinMemBuffer;
  if (LLVMLinkEraVM(Obj2MemBuffer, &BinMemBuffer, &LinkerSymbols[1],
                    &LinkerSymbolVals[1], 1, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  {
    LLVMMemoryBufferRef Bin2MemBuffer;
    EXPECT_TRUE(LLVMLinkEraVM(BinMemBuffer, &Bin2MemBuffer, nullptr, nullptr, 0,
                              &ErrMsg));
    EXPECT_TRUE(
        StringRef(ErrMsg).contains("Input binary is not an EraVM ELF file"));
    LLVMDisposeMessage(ErrMsg);
  }

  EXPECT_FALSE(LLVMIsELFEraVM(BinMemBuffer));
  UndefLinkerSymbols =
      LLVMGetUndefinedLinkerSymbolsEraVM(BinMemBuffer, &NumUndefLinkerSymbols);
  EXPECT_TRUE(NumUndefLinkerSymbols == 0);

  StringRef Val1(LinkerSymbolVals[0], 20);
  StringRef Val2(LinkerSymbolVals[1], 20);
  StringRef Binary(LLVMGetBufferStart(BinMemBuffer),
                   LLVMGetBufferSize(BinMemBuffer));
  EXPECT_TRUE(Binary.find(Val1) != StringRef::npos);
  EXPECT_TRUE(Binary.find(Val2) != StringRef::npos);
  EXPECT_TRUE(LLVMGetBufferSize(BinMemBuffer) % 64 == 32);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
  LLVMDisposeMemoryBuffer(Obj2MemBuffer);
  LLVMDisposeMemoryBuffer(BinMemBuffer);
}

TEST_F(LLDCTest, LinkError) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
declare void @foo()                                               \n\
define void @glob() nounwind {                                    \n\
  call void @foo()                                                \n\
  ret void                                                        \n\
}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef IrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "test", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef M;
  if (LLVMParseIRInContext(Context, IrMemBuffer, &M, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffer.
  LLVMMemoryBufferRef ObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &ErrMsg,
                                          &ObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(M);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeModule(M);

  LLVMMemoryBufferRef BinMemBuffer;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(
      LLVMLinkEraVM(ObjMemBuffer, &BinMemBuffer, nullptr, nullptr, 0, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("undefined symbol: foo"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}

TEST_F(LLDCTest, LinkErrorWithDefinedLibrarySymbols) {
  StringRef LLVMIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S32-a:256:256\"    \n\
declare i256 @foo()                                               \n\
declare i256 @llvm.eravm.linkersymbol(metadata)                   \n\
define i256 @glob() nounwind {                                    \n\
  %addr = call i256 @llvm.eravm.linkersymbol(metadata !1)         \n\
  %off = call i256 @foo()                                         \n\
  %res = add i256 %addr, %off                                     \n\
  ret i256 %res                                                   \n\
}                                                                 \n\
!1 = !{!\"library_id\"}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef IrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      LLVMIr.data(), LLVMIr.size(), "test", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef M;
  if (LLVMParseIRInContext(Context, IrMemBuffer, &M, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffer.
  LLVMMemoryBufferRef ObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &ErrMsg,
                                          &ObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(M);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeModule(M);

  const char *LinkerSymbols[1] = {"library_id"};
  const char LinkerSymbolVals[1][20] = {
      {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5}};

  LLVMMemoryBufferRef BinMemBuffer;
  // Return code 'true' denotes an error.
  EXPECT_TRUE(LLVMLinkEraVM(ObjMemBuffer, &BinMemBuffer, LinkerSymbols,
                            LinkerSymbolVals, 1, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("undefined symbol: foo"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}
