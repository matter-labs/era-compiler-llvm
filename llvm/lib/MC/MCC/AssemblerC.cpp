//===------ AssemblerC.cpp - Assembler Public C Interface -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the C interface for the llvm-mc assembler.
//
//===----------------------------------------------------------------------===//

#include "llvm-c/Assembler.h"
#include "llvm-c/Core.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

static TargetMachine *unwrap(LLVMTargetMachineRef P) {
  return reinterpret_cast<TargetMachine *>(P);
}

static int AssembleInput(const Target *TheTarget, SourceMgr &SrcMgr,
                         MCContext &Ctx, MCStreamer &Str, const MCAsmInfo &MAI,
                         const MCSubtargetInfo &STI, const MCInstrInfo &MCII,
                         const MCTargetOptions &MCOptions) {
  std::unique_ptr<MCAsmParser> Parser(createMCAsmParser(SrcMgr, Ctx, Str, MAI));
  std::unique_ptr<MCTargetAsmParser> TAP(
      TheTarget->createMCAsmParser(STI, *Parser, MCII, MCOptions));

  if (!TAP)
    return 1;

  Parser->setTargetParser(*TAP);
  return Parser->Run(/*NoInitialTextSection=*/false);
}

LLVMBool LLVMAssembleEraVM(LLVMTargetMachineRef T,
                           LLVMMemoryBufferRef inLLVMMemBufRef,
                           LLVMMemoryBufferRef *outLLVMMemBufRef) {
  TargetMachine *TM = unwrap(T);
  const Target *TheTarget = &TM->getTarget();
  const MCTargetOptions &MCOptions = TM->Options.MCOptions;
  const Triple &TheTriple = TM->getTargetTriple();

  MemoryBuffer *InMemBuf = unwrap(inLLVMMemBufRef);
  // Create a copy of the input buffer because SourceMgs will take
  // ownership of the memory buffer.
  std::unique_ptr<MemoryBuffer> BufferPtr =
      MemoryBuffer::getMemBuffer(InMemBuf->getMemBufferRef());

  SourceMgr SrcMgr;

  // Tell SrcMgr about this buffer, which is what the parser will pick up.
  SrcMgr.AddNewSourceBuffer(std::move(BufferPtr), SMLoc());

  const MCRegisterInfo *MRI = TM->getMCRegisterInfo();
  assert(MRI && "Unable to get target register info!");

  const MCAsmInfo *MAI = TM->getMCAsmInfo();
  assert(MAI && "Unable to get target asm info!");

  const MCSubtargetInfo *STI = TM->getMCSubtargetInfo();
  assert(STI && "Unable to get subtarget info!");

  MCContext Ctx(TheTriple, MAI, MRI, STI, &SrcMgr, &MCOptions);
  std::unique_ptr<MCObjectFileInfo> MOFI(
      TheTarget->createMCObjectFileInfo(Ctx, /*PIC=*/false));
  Ctx.setObjectFileInfo(MOFI.get());

  if (MCOptions.MCSaveTempLabels)
    Ctx.setAllowTemporaryLabels(false);

  // TODO: control this via an option.
  Ctx.setGenDwarfForAssembly(false);

  const MCInstrInfo *MCII = TM->getMCInstrInfo();
  assert(MCII && "Unable to get instruction info!");

  MCCodeEmitter *CE = TheTarget->createMCCodeEmitter(*MCII, Ctx);
  MCAsmBackend *MAB = TheTarget->createMCAsmBackend(*STI, *MRI, MCOptions);

  // Define the local buffer where the resulting object code will be put.
  llvm::SmallString<0> BufferString;
  llvm::raw_svector_ostream OS(BufferString);

  std::unique_ptr<MCStreamer> Str(TheTarget->createMCObjectStreamer(
      TheTriple, Ctx, std::unique_ptr<MCAsmBackend>(MAB),
      MAB->createObjectWriter(OS), std::unique_ptr<MCCodeEmitter>(CE), *STI,
      MCOptions.MCRelaxAll, MCOptions.MCIncrementalLinkerCompatible,
      /*DWARFMustBeAtTheEnd*/ false));

  Str->initSections(true, *STI);

  // Use Assembler information for parsing.
  Str->setUseAssemblerInfoForParsing(true);

  int Res =
      AssembleInput(TheTarget, SrcMgr, Ctx, *Str, *MAI, *STI, *MCII, MCOptions);
  if (Res)
    return false;

  // Create output buffer and copy there the object code.
  llvm::StringRef data = BufferString.str();
  *outLLVMMemBufRef = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data(), data.size(), "result");

  return true;
}
