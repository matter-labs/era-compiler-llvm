//===- BytecodeSize.cpp --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EVMCalculateModuleSize.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

std::unique_ptr<LLVMTargetMachine> createTargetMachine() {
  auto TT(Triple::normalize("evm--"));
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(TT, Error);
  return std::unique_ptr<LLVMTargetMachine>(static_cast<LLVMTargetMachine *>(
      TheTarget->createTargetMachine(TT, "", "", TargetOptions(), std::nullopt,
                                     std::nullopt, CodeGenOptLevel::Default)));
}

class BytecodeSizeTest : public testing::Test {
protected:
  static const char *MIRString;
  static const char *MIRStringReadOnlyData;
  LLVMContext Context;
  std::unique_ptr<LLVMTargetMachine> TM;
  std::unique_ptr<MachineModuleInfo> MMI;

  static void SetUpTestCase() {
    LLVMInitializeEVMTargetInfo();
    LLVMInitializeEVMTarget();
    LLVMInitializeEVMTargetMC();
  }

  std::unique_ptr<Module> parseMIR(StringRef MIR) {
    std::unique_ptr<MemoryBuffer> MBuffer = MemoryBuffer::getMemBuffer(MIR);
    std::unique_ptr<MIRParser> Parser =
        createMIRParser(std::move(MBuffer), Context);
    if (!Parser)
      report_fatal_error("null MIRParser");
    std::unique_ptr<Module> M = Parser->parseIRModule();
    if (!M)
      report_fatal_error("parseIRModule failed");
    M->setTargetTriple(TM->getTargetTriple().getTriple());
    M->setDataLayout(TM->createDataLayout());
    if (Parser->parseMachineFunctions(*M, *MMI))
      report_fatal_error("parseMachineFunctions failed");

    return M;
  }

  void SetUp() override {
    TM = createTargetMachine();
    MMI = std::make_unique<MachineModuleInfo>(TM.get());
  }
};

TEST_F(BytecodeSizeTest, Test) {
  std::unique_ptr<Module> M(parseMIR(MIRString));
  ASSERT_TRUE(EVM::calculateModuleCodeSize(*M, *MMI) == 131);

  std::unique_ptr<Module> M2(parseMIR(MIRStringReadOnlyData));
  // 23 = 21 (global variable initializers) + JUMPDEST + INVALID
  ASSERT_TRUE(EVM::calculateModuleCodeSize(*M2, *MMI) == 23);
}

const char *BytecodeSizeTest::MIRString = R"MIR(
--- |
  target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
  target triple = "evm-unknown-unknown"

  ; Function Attrs: nocallback noduplicate nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
  declare i256 @llvm.evm.pushdeployaddress() #0

  ; Function Attrs: noreturn
  define void @egcd() #1 {
  bb:
    %deploy_addr = tail call i256 @llvm.evm.pushdeployaddress()
    %addr_arg = inttoptr i256 0 to ptr addrspace(2)
    %arg = call i256 @llvm.evm.calldataload(ptr addrspace(2) %addr_arg)
    %addr_arg1 = inttoptr i256 32 to ptr addrspace(2)
    %arg1 = call i256 @llvm.evm.calldataload(ptr addrspace(2) %addr_arg1)
    %addr1 = inttoptr i256 32 to ptr addrspace(1)
    store i256 %deploy_addr, ptr addrspace(1) %addr1, align 4
    %addr2 = inttoptr i256 64 to ptr addrspace(1)
    store i256 0, ptr addrspace(1) %addr2, align 4
    %i = icmp eq i256 %arg1, 0
    br i1 %i, label %bb20, label %bb4.preheader

  bb4.preheader:                                    ; preds = %bb
    br label %bb4

  bb4:                                              ; preds = %bb4.preheader, %bb4
    %i5 = phi i256 [ %i9, %bb4 ], [ 1, %bb4.preheader ]
    %i6 = phi i256 [ %i7, %bb4 ], [ %arg, %bb4.preheader ]
    %i7 = phi i256 [ %i13, %bb4 ], [ %arg1, %bb4.preheader ]
    %i8 = phi i256 [ %i17, %bb4 ], [ 1, %bb4.preheader ]
    %i9 = phi i256 [ %i15, %bb4 ], [ 0, %bb4.preheader ]
    %i10 = phi i256 [ %i8, %bb4 ], [ 0, %bb4.preheader ]
    %i11 = sdiv i256 %i6, %i7
    %i12 = mul nsw i256 %i11, %i7
    %i13 = sub nsw i256 %i6, %i12
    %i14 = mul nsw i256 %i11, %i9
    %i15 = sub nsw i256 %i5, %i14
    %i16 = mul nsw i256 %i11, %i8
    %i17 = sub nsw i256 %i10, %i16
    %i18 = icmp eq i256 %i13, 0
    br i1 %i18, label %bb19, label %bb4

  bb19:                                             ; preds = %bb4
    store i256 %i9, ptr addrspace(1) %addr1, align 4
    store i256 %i8, ptr addrspace(1) %addr2, align 4
    br label %bb20

  bb20:                                             ; preds = %bb19, %bb
    %i21 = phi i256 [ %i7, %bb19 ], [ %arg, %bb ]
    store i256 %i21, ptr addrspace(1) null, align 4
    call void @llvm.evm.return(ptr addrspace(1) null, i256 96)
    unreachable
  }

  ; Function Attrs: noreturn nounwind
  declare void @llvm.evm.return(ptr addrspace(1), i256) #2

  ; Function Attrs: nounwind memory(argmem: read)
  declare i256 @llvm.evm.calldataload(ptr addrspace(2)) #3

  attributes #0 = { nocallback noduplicate nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
  attributes #1 = { noreturn }
  attributes #2 = { noreturn nounwind }
  attributes #3 = { nounwind memory(argmem: read) }

...
---
name:            egcd
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
callsEHReturn:   false
callsUnwindInit: false
hasEHCatchret:   false
hasEHScopes:     false
hasEHFunclets:   false
isOutlined:      false
debugInstrRef:   false
failsVerification: false
tracksDebugUserValues: false
registers:
  - { id: 0, class: gpr, preferred-register: '' }
  - { id: 1, class: gpr, preferred-register: '' }
  - { id: 2, class: gpr, preferred-register: '' }
  - { id: 3, class: gpr, preferred-register: '' }
  - { id: 4, class: gpr, preferred-register: '' }
  - { id: 5, class: gpr, preferred-register: '' }
  - { id: 6, class: gpr, preferred-register: '' }
  - { id: 7, class: gpr, preferred-register: '' }
  - { id: 8, class: gpr, preferred-register: '' }
  - { id: 9, class: gpr, preferred-register: '' }
  - { id: 10, class: gpr, preferred-register: '' }
  - { id: 11, class: gpr, preferred-register: '' }
  - { id: 12, class: gpr, preferred-register: '' }
  - { id: 13, class: gpr, preferred-register: '' }
  - { id: 14, class: gpr, preferred-register: '' }
  - { id: 15, class: gpr, preferred-register: '' }
  - { id: 16, class: gpr, preferred-register: '' }
  - { id: 17, class: gpr, preferred-register: '' }
  - { id: 18, class: gpr, preferred-register: '' }
  - { id: 19, class: gpr, preferred-register: '' }
  - { id: 20, class: gpr, preferred-register: '' }
  - { id: 21, class: gpr, preferred-register: '' }
  - { id: 22, class: gpr, preferred-register: '' }
  - { id: 23, class: gpr, preferred-register: '' }
  - { id: 24, class: gpr, preferred-register: '' }
  - { id: 25, class: gpr, preferred-register: '' }
  - { id: 26, class: gpr, preferred-register: '' }
  - { id: 27, class: gpr, preferred-register: '' }
  - { id: 28, class: gpr, preferred-register: '' }
  - { id: 29, class: gpr, preferred-register: '' }
  - { id: 30, class: gpr, preferred-register: '' }
  - { id: 31, class: gpr, preferred-register: '' }
  - { id: 32, class: gpr, preferred-register: '' }
  - { id: 33, class: gpr, preferred-register: '' }
  - { id: 34, class: gpr, preferred-register: '' }
  - { id: 35, class: gpr, preferred-register: '' }
  - { id: 36, class: gpr, preferred-register: '' }
  - { id: 37, class: gpr, preferred-register: '' }
  - { id: 38, class: gpr, preferred-register: '' }
  - { id: 39, class: gpr, preferred-register: '' }
  - { id: 40, class: gpr, preferred-register: '' }
  - { id: 41, class: gpr, preferred-register: '' }
  - { id: 42, class: gpr, preferred-register: '' }
liveins:
  - { reg: '$arguments', virtual-reg: '' }
  - { reg: '$value_stack', virtual-reg: '' }
frameInfo:
  isFrameAddressTaken: false
  isReturnAddressTaken: false
  hasStackMap:     false
  hasPatchPoint:   false
  stackSize:       0
  offsetAdjustment: 0
  maxAlignment:    1
  adjustsStack:    false
  hasCalls:        false
  stackProtector:  ''
  functionContext: ''
  maxCallFrameSize: 0
  cvBytesOfCalleeSavedRegisters: 0
  hasOpaqueSPAdjustment: false
  hasVAStart:      false
  hasMustTailInVarArgFunc: false
  hasTailCall:     false
  isCalleeSavedInfoValid: false
  localFrameSize:  0
  savePoint:       ''
  restorePoint:    ''
fixedStack:      []
stack:           []
entry_values:    []
callSites:       []
debugValueSubstitutions: []
constants:       []
machineFunctionInfo:
  isStackified:    true
  numberOfParameters: 0
  hasPushDeployAddress: true
body:             |
  bb.0.bb:
    successors: %bb.1(0x40000000), %bb.2(0x40000000)
    liveins: $arguments, $value_stack

    PUSH0_S
    SWAP1_S
    PUSH0_S
    CALLDATALOAD_S
    SWAP1_S
    PUSH1_S i256 32
    CALLDATALOAD_S
    SWAP1_S
    PUSH0_S
    PUSH1_S i256 64
    MSTORE_S
    PUSH1_S i256 32
    MSTORE_S
    PUSH1_S i256 32
    CALLDATALOAD_S
    PseudoJUMPI %bb.2

  bb.1:
    successors: %bb.5(0x80000000)
    liveins: $value_stack

    POP_S
    POP_S
    POP_S
    PUSH0_S
    CALLDATALOAD_S
    PseudoJUMP %bb.5

  bb.2.bb4.preheader:
    successors: %bb.3(0x80000000)
    liveins: $value_stack

    PUSH1_S i256 1
    SWAP3_S
    SWAP2_S
    SWAP3_S
    SWAP2_S
    PUSH1_S i256 1
    SWAP3_S
    PUSH0_S
    SWAP2_S

  bb.3.bb4:
    successors: %bb.4(0x00000800), %bb.6(0x7ffff800)
    liveins: $value_stack

    SWAP4_S
    SWAP3_S
    SWAP5_S
    DUP6_S
    DUP1_S
    DUP3_S
    SDIV_S
    SWAP4_S
    DUP6_S
    DUP6_S
    MUL_S
    SWAP1_S
    SUB_S
    SWAP3_S
    DUP7_S
    DUP6_S
    MUL_S
    SWAP1_S
    SUB_S
    SWAP4_S
    MUL_S
    SWAP1_S
    SUB_S
    DUP1_S
    SWAP3_S
    DUP6_S
    DUP8_S
    SWAP2_S
    DUP7_S
    SWAP6_S
    PseudoJUMP_UNLESS %bb.4

  bb.6:
    successors: %bb.3(0x80000000)
    liveins: $value_stack

    SWAP3_S
    SWAP6_S
    POP_S
    SWAP3_S
    SWAP6_S
    POP_S
    SWAP6_S
    POP_S
    PseudoJUMP %bb.3

  bb.4.bb19:
    successors: %bb.5(0x80000000)
    liveins: $value_stack

    POP_S
    POP_S
    POP_S
    POP_S
    POP_S
    POP_S
    PUSH1_S i256 64
    MSTORE_S
    PUSH1_S i256 32
    MSTORE_S

  bb.5.bb20:
    liveins: $value_stack

    PUSH0_S
    MSTORE_S
    PUSH1_S i256 96
    PUSH0_S
    RETURN_S

...
)MIR";

const char *BytecodeSizeTest::MIRStringReadOnlyData = R"MIR(
--- |
  target datalayout = "E-p:256:256-i256:256:256-S256-a:256:256"
  target triple = "evm"

  @code_const.1 = private unnamed_addr addrspace(4) constant [7 x i8] c"global1"
  @code_const.2 = private unnamed_addr addrspace(4) constant [7 x i8] c"global2"
  @code_const.3 = private unnamed_addr addrspace(4) constant [7 x i8] c"global3"

  ; The following globals are never generated and should not be taken into account.
  @heap_const = private unnamed_addr addrspace(1) constant [7 x i8] c"invalid"
  @code_global = addrspace(4) global i256 0
  @code_global_ext = external addrspace(4) global i256

  define void @test() noreturn {
    unreachable
  }

...
---
name:            test
alignment:       1
tracksRegLiveness: true
machineFunctionInfo:
  isStackified:    true
  numberOfParameters: 0
  hasPushDeployAddress: false
body:             |
  bb.0 (%ir-block.0):
    liveins: $arguments, $value_stack

...
)MIR";

} // anonymous namespace
