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
#include "llvm-c/Disassembler.h"
#include "llvm-c/Object.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Target/TargetMachine.h"

#include <algorithm>
#include <limits>
#include <set>

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
  // The buffer we pass to AsmParser needs to be null-terminated, which is
  // ensured by the getMemBufferCopy implementation.
  std::unique_ptr<MemoryBuffer> BufferPtr = MemoryBuffer::getMemBufferCopy(
      InMemBuf->getBuffer(), InMemBuf->getBufferIdentifier());

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

static const char *symbolLookupCallback(void *DisInfo, uint64_t ReferenceValue,
                                        uint64_t *ReferenceType,
                                        uint64_t ReferencePC,
                                        const char **ReferenceName) {
  *ReferenceType = LLVMDisassembler_ReferenceType_InOut_None;
  return nullptr;
}

LLVMBool LLVMDisassembleEraVM(LLVMTargetMachineRef T,
                              LLVMMemoryBufferRef InBuffer, uint64_t PC,
                              uint64_t Options, LLVMMemoryBufferRef *OutBuffer,
                              char **ErrorMessage) {
  TargetMachine *TM = unwrap(T);
  const Triple &TheTriple = TM->getTargetTriple();
  constexpr size_t InstrSize = 8;
  constexpr size_t WordSize = 32;
  constexpr size_t OutStringSize = 1024;
  MemoryBuffer *InMemBuf = unwrap(InBuffer);
  const auto *Bytes =
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<const uint8_t *>(InMemBuf->getBuffer().data());
  const size_t BytesNum = InMemBuf->getBufferSize();

  if (PC > BytesNum) {
    *ErrorMessage = strdup("Starting address exceeds the bytecode size");
    return true;
  }

  if (PC % InstrSize) {
    *ErrorMessage =
        strdup("Starting address isn't multiple of 8 (instruction size)");
    return true;
  }

  if (BytesNum % WordSize) {
    *ErrorMessage = strdup("Bytecode size isn't multiple of 32 (word size)");
    return true;
  }

  LLVMDisasmContextRef DCR = LLVMCreateDisasm(
      TheTriple.getTriple().c_str(), nullptr, 0, nullptr, symbolLookupCallback);
  assert(DCR && "Unable to create disassembler");

  std::string Disassembly;
  raw_string_ostream OS(Disassembly);
  formatted_raw_ostream FOS(OS);
  bool ShoulOutputEncoding =
      Options & LLVMDisassemblerEraVM_Option_OutputEncoding;

  auto PrintEncoding = [&FOS](uint64_t PC, ArrayRef<uint8_t> InstrBytes) {
    FOS << format("%8" PRIx64 ":", PC);
    FOS << ' ';
    dumpBytes(ArrayRef<uint8_t>(InstrBytes), FOS);
  };

  // First, parse the section with instructions. Stop at the beginning
  // of the section with constants (if any).
  uint64_t ConstantSectionStart = std::numeric_limits<uint64_t>::max();
  Regex CodeRegex(R"(code\[(r[0-9]+\+)?([0-9]+)\])");
  bool FoundMetadata = false;
  while (PC < BytesNum) {
    std::array<uint8_t, InstrSize> InstrBytes{};
    std::memcpy(InstrBytes.data(), Bytes + PC, InstrSize);

    if (ShoulOutputEncoding)
      PrintEncoding(PC, InstrBytes);

    std::array<char, OutStringSize> OutString{};
    size_t NumRead =
        LLVMDisasmInstruction(DCR, InstrBytes.data(), InstrBytes.size(),
                              /*PC=*/0, OutString.data(), OutString.size());

    // We are inside the instructions section, i.e before the constants.
    // Figure out if the current octet is the real instruction, or a
    // zero-filled padding.
    if (!NumRead) {
      if (std::all_of(InstrBytes.begin(), InstrBytes.end(),
                      [](uint8_t Byte) { return Byte == 0; })) {
        FOS << (FoundMetadata ? "\t<unknown>" : "\t<padding>");
      } else {
        FoundMetadata = true;
        FOS << "\t<metadata>";
      }
    } else {
      FOS << OutString.data();
      // Check if the instruction contains a code reference. If so,
      // extract the word number and add it to the WordRefs set.
      SmallVector<StringRef, 3> Matches;
      if (CodeRegex.match(OutString.data(), &Matches)) {
        uint64_t WordNum = 0;
        // Match Idx = 0 corresponds to whole pattern, Idx = 1
        // to an optional register and Idx = 2 to the displacement.
        to_integer<uint64_t>(Matches[2], WordNum, /*Base=*/10);
        ConstantSectionStart = std::min(ConstantSectionStart, WordNum);
      }
    }
    FOS << '\n';

    PC += InstrSize;
    // If we are at the word boundary and the word is being referenced,
    // this is a beginning of the constant section, so break the cycle.
    if (!(PC % WordSize) && ConstantSectionStart == PC / WordSize)
      break;
  }

#ifndef NDEBUG
  if (ConstantSectionStart != std::numeric_limits<uint64_t>::max())
    assert(PC == ConstantSectionStart * WordSize);
#endif

  while (PC + WordSize <= BytesNum) {
    uint64_t Word = PC / WordSize;
    assert(PC % WordSize == 0);

    // Emit the numeric label and the .cell directive.
    FOS << std::to_string(Word) << ":\n";
    FOS << "\t.cell ";

    // Collect four octets constituting the word value.
    SmallVector<uint8_t, 32> CellBytes(
        llvm::make_range(Bytes + PC, Bytes + PC + WordSize));

    // Emit the cell value as a signed integer.
    llvm::SmallString<WordSize> CellHexStr;
    llvm::toHex(llvm::ArrayRef<uint8_t>(CellBytes.data(), CellBytes.size()),
                /*LowerCase=*/false, CellHexStr);
    APInt CellInt(WordSize * 8, CellHexStr.str(), /*radix=*/16);
    CellInt.print(OS, /*isSigned=*/true);
    FOS << '\n';
    PC += WordSize;
  }
  assert(PC == BytesNum);

  *OutBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      Disassembly.data(), Disassembly.size(), "result");

  LLVMDisasmDispose(DCR);
  return false;
}
