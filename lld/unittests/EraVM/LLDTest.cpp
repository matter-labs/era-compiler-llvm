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
#include "llvm-c/TargetMachine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "gtest/gtest.h"
#include <string.h>

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

  LLVMMemoryBufferRef BinMemBuffer;
  if (LLVMLinkEraVM(ObjMemBuffer, &BinMemBuffer,
                    /*metadataPtr*/ nullptr, 0, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
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
  EXPECT_TRUE(LLVMLinkEraVM(ObjMemBuffer, &BinMemBuffer,
                            /*metadataPtr*/ nullptr, 0, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("undefined symbol: foo"));

  LLVMDisposeMessage(ErrMsg);
  LLVMDisposeMemoryBuffer(ObjMemBuffer);
}
