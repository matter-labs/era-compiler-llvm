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
    LLVMInitializeEVMTargetInfo();
    LLVMInitializeEVMTarget();
    LLVMInitializeEVMTargetMC();
    LLVMInitializeEVMAsmPrinter();

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

TEST_F(LLDCTest, IterativeLinkage) {
  StringRef DeployIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\"   \n\
target triple = \"evm\"                                           \n\
declare i256 @llvm.evm.datasize(metadata)                         \n\
declare i256 @llvm.evm.dataoffset(metadata)                       \n\
declare i256 @llvm.evm.linkersymbol(metadata)                     \n\
                                                                  \n\
define i256 @foo() {                                              \n\
  %res = call i256 @llvm.evm.linkersymbol(metadata !1)            \n\
  ret i256 %res                                                  \n\
}                                                                 \n\
                                                                  \n\
define i256 @bar() {                                              \n\
  %linkersym = call i256 @llvm.evm.linkersymbol(metadata !1)      \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)      \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !2)  \n\
  %tmp = add i256 %datasize, %dataoffset                          \n\
  %res = add i256 %tmp, %linkersym                                \n\
  ret i256 %res                                                   \n\
}                                                                 \n\
!1 = !{!\"library_id\"}                                           \n\
!2 = !{!\"Test_26_deployed\"}";

  StringRef DeployedIr = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\"   \n\
target triple = \"evm\"                                           \n\
declare i256 @llvm.evm.linkersymbol(metadata)                     \n\
                                                                  \n\
define i256 @foo() {                                              \n\
  %res = call i256 @llvm.evm.linkersymbol(metadata !1)            \n\
  ret i256 %res                                                   \n\
}                                                                 \n\
!1 = !{!\"library_id2\"}";

  // Wrap Source in a MemoryBuffer
  LLVMMemoryBufferRef DeployIrMemBuffer = LLVMCreateMemoryBufferWithMemoryRange(
      DeployIr.data(), DeployIr.size(), "deploy", 1);
  char *ErrMsg = nullptr;
  LLVMModuleRef DeployMod;
  if (LLVMParseIRInContext(Context, DeployIrMemBuffer, &DeployMod, &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  LLVMMemoryBufferRef DeployedIrMemBuffer =
      LLVMCreateMemoryBufferWithMemoryRange(DeployedIr.data(),
                                            DeployedIr.size(), "deploy", 1);
  LLVMModuleRef DeployedMod;
  if (LLVMParseIRInContext(Context, DeployedIrMemBuffer, &DeployedMod,
                           &ErrMsg)) {
    FAIL() << "Failed to parse llvm ir:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  // Run CodeGen to produce the buffers.
  LLVMMemoryBufferRef DeployObjMemBuffer;
  LLVMMemoryBufferRef DeployedObjMemBuffer;
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, DeployMod, LLVMObjectFile,
                                          &ErrMsg, &DeployObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(DeployMod);
    LLVMDisposeMessage(ErrMsg);
    return;
  }
  if (LLVMTargetMachineEmitToMemoryBuffer(TM, DeployedMod, LLVMObjectFile,
                                          &ErrMsg, &DeployedObjMemBuffer)) {
    FAIL() << "Failed to compile llvm ir:" << ErrMsg;
    LLVMDisposeModule(DeployedMod);
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(LLVMIsELFEVM(DeployObjMemBuffer));
  EXPECT_TRUE(LLVMIsELFEVM(DeployedObjMemBuffer));

  const char *LinkerSymbol[2] = {"library_id", "library_id2"};
  const char LinkerSymbolVal[2][20] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {'\x9f', 0x1E, '\xBB', '\xF1', 7, 7, 7, 7, 7, 7,
       7,      7,    7,      7,      7, 7, 7, 7, 7, 7}};
  StringRef SymVal1(LinkerSymbolVal[0], 20);
  StringRef SymVal2(LinkerSymbolVal[1], 20);

  std::array<LLVMMemoryBufferRef, 2> InMemBuf = {DeployObjMemBuffer,
                                                 DeployedObjMemBuffer};
  std::array<LLVMMemoryBufferRef, 2> OutMemBuf = {nullptr, nullptr};
  const char *InIDs[] = {"Test_26", "Test_26_deployed"};

  // No linker symbol definitions are provided, so we have to receive two ELF
  // object files.
  if (LLVMLinkEVM(InMemBuf.data(), InIDs, 2, OutMemBuf.data(), nullptr, nullptr,
                  0, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf[0]));
  EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf[1]));

  char **UndefLinkerSyms = nullptr;
  uint64_t NumLinkerUndefs = 0;

  LLVMGetUndefinedReferencesEVM(OutMemBuf[0], &UndefLinkerSyms,
                                &NumLinkerUndefs);
  EXPECT_TRUE((std::strcmp(UndefLinkerSyms[0], LinkerSymbol[0]) == 0));
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  LLVMGetUndefinedReferencesEVM(OutMemBuf[1], &UndefLinkerSyms,
                                &NumLinkerUndefs);
  EXPECT_TRUE((std::strcmp(UndefLinkerSyms[0], LinkerSymbol[1]) == 0));
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  InMemBuf.swap(OutMemBuf);
  LLVMDisposeMemoryBuffer(OutMemBuf[0]);
  LLVMDisposeMemoryBuffer(OutMemBuf[1]);

  // The first linker symbol definitions is provided, so we still have to
  // receive two ELF object files, because of the undefined second reference.
  if (LLVMLinkEVM(InMemBuf.data(), InIDs, 2, OutMemBuf.data(), LinkerSymbol,
                  LinkerSymbolVal, 1, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf[0]));
  EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf[1]));

  LLVMGetUndefinedReferencesEVM(OutMemBuf[0], &UndefLinkerSyms,
                                &NumLinkerUndefs);
  EXPECT_TRUE(NumLinkerUndefs == 0);
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  LLVMGetUndefinedReferencesEVM(OutMemBuf[1], &UndefLinkerSyms,
                                &NumLinkerUndefs);
  EXPECT_TRUE((std::strcmp(UndefLinkerSyms[0], LinkerSymbol[1]) == 0));
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  InMemBuf.swap(OutMemBuf);
  LLVMDisposeMemoryBuffer(OutMemBuf[0]);
  LLVMDisposeMemoryBuffer(OutMemBuf[1]);

  // Both linker symbol definitions are provided, so we have to receive
  // bytecodes files.
  if (LLVMLinkEVM(InMemBuf.data(), InIDs, 2, OutMemBuf.data(), LinkerSymbol,
                  LinkerSymbolVal, 2, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(!LLVMIsELFEVM(OutMemBuf[0]));
  EXPECT_TRUE(!LLVMIsELFEVM(OutMemBuf[1]));

  LLVMGetUndefinedReferencesEVM(OutMemBuf[0], &UndefLinkerSyms,
                                &NumLinkerUndefs);
  EXPECT_TRUE(NumLinkerUndefs == 0);
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  LLVMGetUndefinedReferencesEVM(OutMemBuf[1], &UndefLinkerSyms,
                                &NumLinkerUndefs);
  EXPECT_TRUE(NumLinkerUndefs == 0);
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  StringRef DeployBin(LLVMGetBufferStart(OutMemBuf[0]),
                      LLVMGetBufferSize(OutMemBuf[0]));
  StringRef DeployedBin(LLVMGetBufferStart(OutMemBuf[1]),
                        LLVMGetBufferSize(OutMemBuf[1]));

  EXPECT_TRUE(DeployBin.find(SymVal1) != StringRef::npos);
  EXPECT_TRUE(DeployedBin.find(SymVal2) != StringRef::npos);

  for (unsigned I = 0; I < 2; ++I) {
    LLVMDisposeMemoryBuffer(OutMemBuf[I]);
    LLVMDisposeMemoryBuffer(InMemBuf[I]);
  }
}
