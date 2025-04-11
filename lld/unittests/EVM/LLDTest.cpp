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
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/KECCAK.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"
#include <array>

using namespace llvm;

#include <iostream>
static std::string expand(const char *Path) {
  llvm::SmallString<256> ThisPath;
  ThisPath.append(getenv("LLD_SRC_DIR"));
  llvm::sys::path::append(ThisPath, "unittests", "EVM", "Inputs", Path);
  std::cerr << "Full path: " << ThisPath.str().str() << '\n';
  return ThisPath.str().str();
}

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

  LLVMMemoryBufferRef addmd(LLVMMemoryBufferRef InObj) {
    char *ErrMsg = nullptr;
    LLVMMemoryBufferRef Out;
    if (LLVMAddMetadata(InObj, MD.data(), MD.size(), &Out, &ErrMsg)) {
      LLVMDisposeMessage(ErrMsg);
      exit(1);
    }

    return Out;
  }

  LLVMMemoryBufferRef assemble(uint64_t codeSegment,
                               const std::vector<LLVMMemoryBufferRef> &Objs,
                               const std::vector<const char *> &IDs) {
    char *ErrMsg = nullptr;
    LLVMMemoryBufferRef OutAssembly = nullptr;
    if (LLVMAssembleEVM(codeSegment, Objs.data(), IDs.data(), Objs.size(),
                        &OutAssembly, &ErrMsg)) {
      LLVMDisposeMessage(ErrMsg);
      exit(1);
    }
    EXPECT_TRUE(LLVMIsELFEVM(OutAssembly));
    return OutAssembly;
  }

  LLVMMemoryBufferRef compileIR(const char *FileName) {
    auto Buf = MemoryBuffer::getFile(expand(FileName), /*IsText=*/false,
                                     /*RequiresNullTerminator=*/false);
    if (auto EC = Buf.getError())
      exit(1);

    LLVMMemoryBufferRef IrBuf = llvm::wrap(Buf.get().release());
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
  std::array<char, 32> MD = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
};

TEST_F(LLDCTest, IterativeLinkage) {
  LLVMMemoryBufferRef DeployObjMemBuffer = compileIR("deployIr.ll");
  LLVMMemoryBufferRef DeployedObjMemBuffer = compileIR("deployedIr.ll");

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
  char *ErrMsg = nullptr;

  // Assemble deploy with deployed.
  {
    if (LLVMAssembleEVM(/*codeSegment=*/0, InData.data(), InIDs, 2, &OutMemBuf,
                        &ErrMsg)) {
      FAIL() << "Failed to assemble:" << ErrMsg;
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
  const char *LinkerSymbol[2] = {"unused_library_id", "library_id"};
  const char LinkerSymbolVal[2][20] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9}};

  LLVMMemoryBufferRef A_deploy_obj = compileIR("A_deploy.ll");
  LLVMMemoryBufferRef A_deployed_obj = compileIR("A_deployed.ll");
  LLVMMemoryBufferRef D_deploy_obj = compileIR("D_deploy.ll");
  LLVMMemoryBufferRef D_deployed_obj = compileIR("D_deployed.ll");
  LLVMMemoryBufferRef R_deploy_obj = compileIR("R_deploy.ll");
  LLVMMemoryBufferRef R_deployed_obj = compileIR("R_deployed.ll");

  // A assemble.
  LLVMMemoryBufferRef A_deployed_obj_md = addmd(A_deployed_obj);
  LLVMMemoryBufferRef A_assembly_deployed =
      assemble(/*codeSegment=*/1, {A_deployed_obj_md}, {"A_38_deployed"});
  LLVMMemoryBufferRef A_assembly =
      assemble(/*codeSegment=*/0, {A_deploy_obj, A_assembly_deployed},
               {"A_38", "A_38_deployed"});

  // D assemble.
  LLVMMemoryBufferRef D_deployed_obj_md = addmd(D_deployed_obj);
  LLVMMemoryBufferRef D_assembly_deployed =
      assemble(/*codeSegment=*/1, {D_deployed_obj_md}, {"D_38_deployed"});
  LLVMMemoryBufferRef D_assembly =
      assemble(/*codeSegment=*/0, {D_deploy_obj, D_assembly_deployed},
               {"D_51", "D_51_deployed"});

  // R_deployed assemble.
  // A_assembly is not required here, but we add it intentionaly to check
  // that it will be ignored (the total number of library reference is 3).
  LLVMMemoryBufferRef R_deployed_obj_md = addmd(R_deployed_obj);
  LLVMMemoryBufferRef R_deployed_assemble =
      assemble(/*codeSegment=*/1,
               {R_deployed_obj_md, D_assembly, A_assembly, A_assembly_deployed},
               {"R_107_deployed", "D_51", "A_38", "A_38.A_38_deployed"});

  // R assemble.
  LLVMMemoryBufferRef R_assembly = assemble(
      /*codeSegment=*/0,
      {R_deploy_obj, R_deployed_assemble, A_assembly, A_assembly_deployed},
      {"R_107", "R_107_deployed", "A_38", "A_38.A_38_deployed"});

  // Linking with no linker symbols.
  LLVMMemoryBufferRef TmpAssembly = link(R_assembly, "R", nullptr, nullptr, 0);
  EXPECT_TRUE(LLVMIsELFEVM(TmpAssembly));

  // Linking with unused linker symbol. It has no effect.
  LLVMMemoryBufferRef TmpAssembly2 =
      link(TmpAssembly, "R", LinkerSymbol, LinkerSymbolVal, 1);
  EXPECT_TRUE(LLVMIsELFEVM(TmpAssembly2));

  // Linking with both linker symbols. The library reference should be resolved
  // and resulting object is a final bytecode.
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
  EXPECT_TRUE(Binary.count(StringRef(MD.data(), MD.size())) == 5);

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

TEST_F(LLDCTest, UndefNonRefSymbols) {
  LLVMMemoryBufferRef DeployObj = compileIR("undefDeployIr.ll");
  LLVMMemoryBufferRef DeployedObj = compileIR("undefDeployedIr.ll");

  EXPECT_TRUE(LLVMIsELFEVM(DeployObj));
  EXPECT_TRUE(LLVMIsELFEVM(DeployedObj));

  const std::array<LLVMMemoryBufferRef, 2> InObjs = {DeployObj, DeployedObj};
  const std::array<const char *, 2> IDs = {"Test_26", "Test_26_deployed"};
  LLVMMemoryBufferRef OutObj = nullptr;
  char *ErrMsg = nullptr;
  EXPECT_TRUE(LLVMAssembleEVM(/*codeSegment=*/0, InObjs.data(), IDs.data(),
                              IDs.size(), &OutObj, &ErrMsg));
  EXPECT_TRUE(StringRef(ErrMsg).contains("non-ref undefined symbol:"));
  LLVMDisposeMessage(ErrMsg);

  LLVMDisposeMemoryBuffer(DeployObj);
  LLVMDisposeMemoryBuffer(DeployedObj);
}
