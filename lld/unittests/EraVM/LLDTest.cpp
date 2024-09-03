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
  if (LLVMLinkEraVM(MDObjMemBuffer, &BinMemBuffer, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  StringRef MDVal(MD.data(), MD.size());
  StringRef Binary(LLVMGetBufferStart(BinMemBuffer),
                   LLVMGetBufferSize(BinMemBuffer));
  EXPECT_TRUE(Binary.take_back(MD.size()) == MDVal);

  LLVMDisposeMemoryBuffer(ObjMemBuffer);
  LLVMDisposeMemoryBuffer(MDObjMemBuffer);
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
  EXPECT_TRUE(LLVMLinkEraVM(ObjMemBuffer, &BinMemBuffer, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("undefined symbol: foo"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}
