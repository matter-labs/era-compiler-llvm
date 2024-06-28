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
#include "llvm-c/Object.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;
using namespace object;

static TargetMachine *unwrap(LLVMTargetMachineRef P) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<TargetMachine *>(P);
}

static bool AssembleInput(const Target *TheTarget, SourceMgr &SrcMgr,
                          MCContext &Ctx, MCStreamer &Str, const MCAsmInfo &MAI,
                          const MCSubtargetInfo &STI, const MCInstrInfo &MCII,
                          const MCTargetOptions &MCOptions) {
  std::unique_ptr<MCAsmParser> Parser(createMCAsmParser(SrcMgr, Ctx, Str, MAI));
  std::unique_ptr<MCTargetAsmParser> TAP(
      TheTarget->createMCAsmParser(STI, *Parser, MCII, MCOptions));

  if (!TAP)
    return true;

  Parser->setTargetParser(*TAP);
  return Parser->Run(/*NoInitialTextSection=*/false);
}

static void DiagHandler(const SMDiagnostic &Diag, void *Context) {
  auto *OS = static_cast<raw_ostream *>(Context);
  Diag.print(/*ProgName=*/nullptr, *OS, /*ShowColors*/ false,
             /*ShowKindLabels*/ true);
  OS->flush();
}

LLVMBool LLVMAssembleEraVM(LLVMTargetMachineRef T, LLVMMemoryBufferRef InBuffer,
                           LLVMMemoryBufferRef *OutBuffer,
                           char **ErrorMessage) {
  TargetMachine *TM = unwrap(T);
  const Target *TheTarget = &TM->getTarget();
  const MCTargetOptions &MCOptions = TM->Options.MCOptions;
  const Triple &TheTriple = TM->getTargetTriple();

  MemoryBuffer *InMemBuf = unwrap(InBuffer);
  // Create a copy of the input buffer because SourceMgr will take
  // ownership of the memory buffer.
  // The buffer we pass to AsmParser doesn't need to be null-terminated.
  std::unique_ptr<MemoryBuffer> BufferPtr =
      MemoryBuffer::getMemBuffer(InMemBuf->getMemBufferRef(),
                                 /*RequiresNullTerminator=*/false);

  llvm::SmallString<0> ErrorMsgBuffer;
  llvm::raw_svector_ostream ErrorMsgOS(ErrorMsgBuffer);

  SourceMgr SrcMgr;
  // Set up a diagnostic handler to avoid errors being printed out to
  // stderr.
  SrcMgr.setDiagHandler(DiagHandler, &ErrorMsgOS);

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

  // TODO: control this in the future via an option.
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

  // We are not using .note.GNU-stack section, so no need to request
  // for it in initSections.
  Str->initSections(/*NoExecStack*/ false, *STI);

  // Use Assembler information for parsing.
  Str->setUseAssemblerInfoForParsing(true);

  bool Res =
      AssembleInput(TheTarget, SrcMgr, Ctx, *Str, *MAI, *STI, *MCII, MCOptions);
  if (Res) {
    *ErrorMessage = strdup(ErrorMsgBuffer.c_str());
    return true;
  }

  // Create output buffer and copy there the object code.
  llvm::StringRef Data = BufferString.str();
  *OutBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(Data.data(),
                                                         Data.size(), "result");
  return false;
}

LLVMBool LLVMExceedsSizeLimitEraVM(LLVMMemoryBufferRef MemBuf,
                                   uint64_t MetadataSize) {
  Expected<std::unique_ptr<Binary>> ObjOrErr(
      createBinary(unwrap(MemBuf)->getMemBufferRef(), nullptr));
  if (!ObjOrErr)
    llvm_unreachable("Cannot create Binary object from the memory buffer");

  uint64_t TextSize = 0, RodataSize = 0;
  const auto *OF = cast<ObjectFile>(ObjOrErr->get());
  for (const SectionRef &SecRef : OF->sections()) {
    Expected<StringRef> NameOrErr = SecRef.getName();
    if (!NameOrErr)
      llvm_unreachable("Cannot get a section name");

    if (*NameOrErr == ".text")
      TextSize = SecRef.getSize();
    else if (*NameOrErr == ".rodata")
      RodataSize = SecRef.getSize();
  }

  // The instructions code size shouldn't be more than 2^16 * 8 bytes.
  if (TextSize > (uint64_t(1) << 16) * 8)
    return true;

  // Add paddings, so the section size is equal 0 modulo 32.
  TextSize = alignTo(TextSize, Align(32), 0);

  uint64_t BinarySize = TextSize + RodataSize;

  // Add padding such that the total binary size to be the odd number of
  // words.
  uint64_t AlignedMDSize = llvm::alignTo(MetadataSize, llvm::Align(32), 0);
  BinarySize = ((((BinarySize + AlignedMDSize) >> 5) | 1) << 5) - MetadataSize;

  // Add metadata
  BinarySize += MetadataSize;
  // Check the number of words is odd.
  assert(BinarySize % 64 == 32);

  // The total binary size shouldn't be more than (2^16 - 1) * 32 bytes.
  if (BinarySize > ((uint64_t(1) << 16) - 1) * 32)
    return true;

  return false;
}
