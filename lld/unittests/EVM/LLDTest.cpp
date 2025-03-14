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
  LLVMMemoryBufferRef link(LLVMMemoryBufferRef InAssembly, const char *Name,
                           const char *LinkerSyms[],
                           const char LinkerSymVals[][20], uint64_t NumSyms) {
    char *ErrMsg = nullptr;
    LLVMMemoryBufferRef OutAssembly = nullptr;
    if (LLVMLinkEVM(InAssembly, &OutAssembly, LinkerSyms, LinkerSymVals,
                    NumSyms, &ErrMsg)) {
      LLVMDisposeMessage(ErrMsg);
      exit(1);
    }
    return OutAssembly;
  }

  LLVMMemoryBufferRef assemble(const std::vector<LLVMMemoryBufferRef> &Objs,
                               const std::vector<const char *> &IDs) {
    char *ErrMsg = nullptr;
    LLVMMemoryBufferRef OutAssembly = nullptr;
    if (LLVMAssembleEVM(Objs.data(), IDs.data(), Objs.size(), &OutAssembly,
                        &ErrMsg)) {
      LLVMDisposeMessage(ErrMsg);
      exit(1);
    }
    EXPECT_TRUE(LLVMIsELFEVM(OutAssembly));
    return OutAssembly;
  }

  LLVMMemoryBufferRef compileIR(StringRef IR) {
    // Wrap Source in a MemoryBuffer
    LLVMMemoryBufferRef IrBuf = LLVMCreateMemoryBufferWithMemoryRange(
        IR.data(), IR.size(), "deploy", 1);
    char *ErrMsg = nullptr;
    LLVMModuleRef Module;
    if (LLVMParseIRInContext(Context, IrBuf, &Module, &ErrMsg)) {
      LLVMDisposeMessage(ErrMsg);
      exit(1);
    }

    // Run CodeGen to produce the buffers.
    LLVMMemoryBufferRef Result;
    if (LLVMTargetMachineEmitToMemoryBuffer(TM, Module, LLVMObjectFile, &ErrMsg,
                                            &Result)) {
      LLVMDisposeModule(Module);
      LLVMDisposeMessage(ErrMsg);
      exit(1);
    }
    LLVMDisposeModule(Module);
    return Result;
  }

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
  ret i256 %res                                                   \n\
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
declare i256 @llvm.evm.loadimmutable(metadata)                    \n\
                                                                  \n\
define i256 @foo() {                                              \n\
  %res = call i256 @llvm.evm.linkersymbol(metadata !1)            \n\
  %res2 = call i256 @llvm.evm.loadimmutable(metadata !2)          \n\
  %res3 = add i256 %res, %res2                                    \n\
  ret i256 %res3                                                  \n\
}                                                                 \n\
!1 = !{!\"library_id2\"}                                          \n\
!2 = !{!\"id\"}";

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

  const char *InIDs[] = {"Test_26", "Test_26_deployed"};
  std::array<LLVMMemoryBufferRef, 2> InData = {DeployObjMemBuffer,
                                               DeployedObjMemBuffer};
  LLVMMemoryBufferRef InMemBuf = nullptr;
  LLVMMemoryBufferRef OutMemBuf = nullptr;

  // Assemble deploy with deployed.
  {
    if (LLVMAssembleEVM(InData.data(), InIDs, 2, &OutMemBuf, &ErrMsg)) {
      FAIL() << "Failed to link:" << ErrMsg;
      LLVMDisposeMessage(ErrMsg);
      return;
    }
    EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf));
    std::swap(OutMemBuf, InMemBuf);
  }

  // Check load immutable references.
  {
    char **ImmutableIDs = nullptr;
    uint64_t *ImmutableOffsets = nullptr;
    uint64_t ImmCount =
        LLVMGetImmutablesEVM(InData[1], &ImmutableIDs, &ImmutableOffsets);
    EXPECT_TRUE(ImmCount == 1);
    EXPECT_TRUE(std::strcmp(ImmutableIDs[0], "id") == 0);
    LLVMDisposeImmutablesEVM(ImmutableIDs, ImmutableOffsets, ImmCount);
  }

  // No linker symbol definitions are provided, so we have to receive ELF
  // object file.
  if (LLVMLinkEVM(InMemBuf, &OutMemBuf, nullptr, nullptr, 0, &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf));

  char **UndefLinkerSyms = nullptr;
  uint64_t NumLinkerUndefs = 0;

  LLVMGetUndefinedReferencesEVM(OutMemBuf, &UndefLinkerSyms, &NumLinkerUndefs);

  EXPECT_TRUE(NumLinkerUndefs == 2);
  EXPECT_TRUE((std::strcmp(UndefLinkerSyms[0], LinkerSymbol[0]) == 0));
  EXPECT_TRUE((std::strcmp(UndefLinkerSyms[1], LinkerSymbol[1]) == 0));
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  std::swap(OutMemBuf, InMemBuf);
  LLVMDisposeMemoryBuffer(OutMemBuf);

  // The first linker symbol definitions is provided, so we still have
  // to receive an ELF object file
  if (LLVMLinkEVM(InMemBuf, &OutMemBuf, LinkerSymbol, LinkerSymbolVal, 1,
                  &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(LLVMIsELFEVM(OutMemBuf));

  LLVMGetUndefinedReferencesEVM(OutMemBuf, &UndefLinkerSyms, &NumLinkerUndefs);
  EXPECT_TRUE(NumLinkerUndefs == 1);
  EXPECT_TRUE((std::strcmp(UndefLinkerSyms[0], LinkerSymbol[1]) == 0));
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  std::swap(OutMemBuf, InMemBuf);
  LLVMDisposeMemoryBuffer(OutMemBuf);

  // Both linker symbol definitions are provided, so we have to receive
  // a bytecode.
  if (LLVMLinkEVM(InMemBuf, &OutMemBuf, LinkerSymbol, LinkerSymbolVal, 2,
                  &ErrMsg)) {
    FAIL() << "Failed to link:" << ErrMsg;
    LLVMDisposeMessage(ErrMsg);
    return;
  }

  EXPECT_TRUE(!LLVMIsELFEVM(OutMemBuf));

  LLVMGetUndefinedReferencesEVM(OutMemBuf, &UndefLinkerSyms, &NumLinkerUndefs);
  EXPECT_TRUE(NumLinkerUndefs == 0);
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  StringRef DeployBin(LLVMGetBufferStart(OutMemBuf),
                      LLVMGetBufferSize(OutMemBuf));

  EXPECT_TRUE(DeployBin.find(SymVal1) != StringRef::npos);
  EXPECT_TRUE(DeployBin.find(SymVal2) != StringRef::npos);

  LLVMDisposeMemoryBuffer(OutMemBuf);
  LLVMDisposeMemoryBuffer(InMemBuf);
}

TEST_F(LLDCTest, Assembly) {
  StringRef A_deploy = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\" \n\
target triple = \"evm\"                                         \n\
declare i256 @llvm.evm.datasize(metadata)                       \n\
declare i256 @llvm.evm.dataoffset(metadata)                     \n\
declare i256 @llvm.evm.codesize()                               \n\
                                                                \n\
define i256 @init() {                                           \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !1)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
define i256 @args_len() {                                       \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)    \n\
  %codesize = tail call i256 @llvm.evm.codesize()               \n\
  %res = sub i256 %codesize, %datasize                          \n\
  ret i256 %res                                                 \n\
}                                                               \n\
!1 = !{!\"A_38_deployed\"}                                      \n\
!2 = !{!\"A_38\"}";

  StringRef A_deployed = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\" \n\
target triple = \"evm\"                                         \n\
declare i256 @llvm.evm.linkersymbol(metadata)                   \n\
declare i256 @llvm.evm.datasize(metadata)                       \n\
                                                                \n\
define i256 @runtime() {                                        \n\
  %lib = call i256 @llvm.evm.linkersymbol(metadata !2)          \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)    \n\
  %res = add i256 %lib, %datasize                               \n\
  ret i256 %res                                                 \n\
}                                                               \n\
!1 = !{!\"A_38_deployed\"}                                      \n\
!2 = !{!\"library_id\"}";

  StringRef D_deploy = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\" \n\
target triple = \"evm\"                                         \n\
declare i256 @llvm.evm.datasize(metadata)                       \n\
declare i256 @llvm.evm.dataoffset(metadata)                     \n\
declare i256 @llvm.evm.codesize()                               \n\
                                                                \n\
define i256 @init() {                                           \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !1)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
define i256 @args_len() {                                       \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)    \n\
  %codesize = tail call i256 @llvm.evm.codesize()               \n\
  %res = sub i256 %codesize, %datasize                          \n\
  ret i256 %res                                                 \n\
}                                                               \n\
!1 = !{!\"D_51_deployed\"}                                      \n\
!2 = !{!\"D_51\"}";

  StringRef D_deployed = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\" \n\
target triple = \"evm\"                                         \n\
declare i256 @llvm.evm.loadimmutable(metadata)                  \n\
                                                                \n\
define i256 @runtime() {                                        \n\
  %res = call i256 @llvm.evm.loadimmutable(metadata !1)         \n\
  ret i256 %res                                                 \n\
}                                                               \n\
!1 = !{!\"40\"}";

  StringRef R_deploy = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\" \n\
target triple = \"evm\"                                         \n\
declare i256 @llvm.evm.datasize(metadata)                       \n\
declare i256 @llvm.evm.dataoffset(metadata)                     \n\
declare i256 @llvm.evm.codesize()                               \n\
                                                                \n\
define i256 @init() {                                           \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !1)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !1)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
define i256 @A_init() {                                         \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !4)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !4)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
define i256 @args_len() {                                       \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)    \n\
  %codesize = tail call i256 @llvm.evm.codesize()               \n\
  %res = sub i256 %codesize, %datasize                          \n\
  ret i256 %res                                                 \n\
}                                                               \n\
define i256 @A_runtime() {                                      \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !3)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !3)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
!1 = !{!\"R_107_deployed\"}                                     \n\
!2 = !{!\"R_107\"}                                              \n\
!3 = !{!\"A_38.A_38_deployed\"}                                 \n\
!4 = !{!\"A_38\"}";

  StringRef R_deployed = "\
target datalayout = \"E-p:256:256-i256:256:256-S256-a:256:256\" \n\
target triple = \"evm\"                                         \n\
declare i256 @llvm.evm.loadimmutable(metadata)                  \n\
declare i256 @llvm.evm.datasize(metadata)                       \n\
declare i256 @llvm.evm.dataoffset(metadata)                     \n\
                                                                \n\
define i256 @runtime() {                                        \n\
  %res = tail call i256 @llvm.evm.loadimmutable(metadata !1)    \n\
  ret i256 %res                                                 \n\
}                                                               \n\
                                                                \n\
define i256 @get_runtimecode() {                                \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !2)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !2)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
define i256 @get_initcode() {                                   \n\
  %datasize = tail call i256 @llvm.evm.datasize(metadata !3)    \n\
  %dataoffset = tail call i256 @llvm.evm.dataoffset(metadata !3)\n\
  %res = add i256 %datasize, %dataoffset                        \n\
  ret i256 %res                                                 \n\
}                                                               \n\
!1 = !{!\"53\"}                                                 \n\
!2 = !{!\"A_38.A_38_deployed\"}                                 \n\
!3 = !{!\"D_51\"}";

  const char *LinkerSymbol[2] = {"unused_library_id", "library_id"};
  const char LinkerSymbolVal[2][20] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9}};

  LLVMMemoryBufferRef A_deploy_obj = compileIR(A_deploy);
  LLVMMemoryBufferRef A_deployed_obj = compileIR(A_deployed);
  LLVMMemoryBufferRef D_deploy_obj = compileIR(D_deploy);
  LLVMMemoryBufferRef D_deployed_obj = compileIR(D_deployed);
  LLVMMemoryBufferRef R_deploy_obj = compileIR(R_deploy);
  LLVMMemoryBufferRef R_deployed_obj = compileIR(R_deployed);

  // A assemble.
  LLVMMemoryBufferRef A_assembly_deployed =
      assemble({A_deployed_obj}, {"A_38_deployed"});
  LLVMMemoryBufferRef A_assembly =
      assemble({A_deploy_obj, A_deployed_obj}, {"A_38", "A_38_deployed"});

  // D assemble.
  LLVMMemoryBufferRef D_assembly =
      assemble({D_deploy_obj, D_deployed_obj}, {"D_51", "D_51_deployed"});

  // R_deployed assemble.
  LLVMMemoryBufferRef R_deployed_assemble =
      assemble({R_deployed_obj, D_assembly, A_assembly, A_assembly_deployed},
               {"R_107_deployed", "D_51", "A_38", "A_38.A_38_deployed"});

  // R assemble.
  LLVMMemoryBufferRef R_assembly = assemble(
      {R_deploy_obj, R_deployed_assemble, A_assembly, A_assembly_deployed},
      {"R_107", "R_107_deployed", "A_38", "A_38.A_38_deployed"});

  // Linking with no linker symbols.
  LLVMMemoryBufferRef TmpAssembly = link(R_assembly, "R", nullptr, nullptr, 0);
  EXPECT_TRUE(LLVMIsELFEVM(TmpAssembly));

  // Linking with unused linker symbol.
  LLVMMemoryBufferRef TmpAssembly2 =
      link(TmpAssembly, "R", LinkerSymbol, LinkerSymbolVal, 1);
  EXPECT_TRUE(LLVMIsELFEVM(TmpAssembly2));

  // Linking with both linker symbols.
  LLVMMemoryBufferRef Bytecode =
      link(TmpAssembly2, "R", LinkerSymbol, LinkerSymbolVal, 2);
  EXPECT_TRUE(!LLVMIsELFEVM(Bytecode));

  char **UndefLinkerSyms = nullptr;
  uint64_t NumLinkerUndefs = 0;

  LLVMGetUndefinedReferencesEVM(Bytecode, &UndefLinkerSyms, &NumLinkerUndefs);
  EXPECT_TRUE(NumLinkerUndefs == 0);
  LLVMDisposeUndefinedReferences(UndefLinkerSyms, NumLinkerUndefs);

  StringRef Binary(LLVMGetBufferStart(Bytecode), LLVMGetBufferSize(Bytecode));

  StringRef LibAddr(LinkerSymbolVal[1], 20);
  EXPECT_TRUE(Binary.count(LibAddr) == 3);

  LLVMDisposeMemoryBuffer(A_deploy_obj);
  LLVMDisposeMemoryBuffer(A_deployed_obj);
  LLVMDisposeMemoryBuffer(D_deploy_obj);
  LLVMDisposeMemoryBuffer(D_deployed_obj);
  LLVMDisposeMemoryBuffer(R_deploy_obj);
  LLVMDisposeMemoryBuffer(R_deployed_obj);
  LLVMDisposeMemoryBuffer(TmpAssembly);
  LLVMDisposeMemoryBuffer(TmpAssembly2);
  LLVMDisposeMemoryBuffer(Bytecode);
}
