#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string.h>
#include <string>

using namespace llvm;
using namespace object;

LLD_HAS_DRIVER_MEM_BUF(elf)

namespace llvm {
namespace EraVM {
// The following two functions are defined in EraVMMCTargetDesc.cpp.
std::string getLinkerSymbolHash(StringRef SymName);
std::string getLinkerIndexedName(StringRef Name, unsigned SubIdx);
std::string getLinkerSymbolSectionName(StringRef Name);
std::string stripLinkerSymbolNameIndex(StringRef Name);
} // namespace EraVM
} // namespace llvm

constexpr static unsigned linkerSubSymbolRelocSize = sizeof(uint32_t);

static std::mutex lldMutex;

/// Returns size of the section \p SectionName in the object file \p file.
/// Returns zero, if there is no such section.
static uint64_t getSectionSize(const ObjectFile &file, StringRef sectionName) {
  section_iterator si = std::find_if(file.section_begin(), file.section_end(),
                                     [&sectionName](const SectionRef &sec) {
                                       StringRef curSecName =
                                           cantFail(sec.getName());
                                       return curSecName == sectionName;
                                     });
  return si != file.section_end() ? si->getSize() : 0;
}

/// Returns reference to the section content. The section is expected
/// the be present in the file.
static StringRef getSectionContent(const ObjectFile &file,
                                   StringRef sectionName) {
  section_iterator si = std::find_if(file.section_begin(), file.section_end(),
                                     [&sectionName](const SectionRef &sec) {
                                       StringRef curSecName =
                                           cantFail(sec.getName());
                                       return curSecName == sectionName;
                                     });
  if (si == file.section_end())
    llvm_unreachable("No section in the file");

  return cantFail(si->getContents());
}

/// Returns true if the object file \p file contains any other undefined
/// linker symbols besides those passed in \p linkerSymbolNames.
static bool hasUndefLinkerSymbols(ObjectFile &file,
                                  const char *const *linkerSymbolNames,
                                  uint64_t numLinkerSymbols) {
  StringSet<> symbolsToBeDefined;
  // Create a set of possible linker symbols from the 'linkerSymbolNames' array.
  for (unsigned symIdx = 0; symIdx < numLinkerSymbols; ++symIdx) {
    for (unsigned subSymIdx = 0;
         subSymIdx < LINKER_SYMBOL_SIZE / linkerSubSymbolRelocSize;
         ++subSymIdx) {
      std::string subSymName = EraVM::getLinkerIndexedName(
          EraVM::getLinkerSymbolHash(linkerSymbolNames[symIdx]), subSymIdx);
      if (!symbolsToBeDefined.insert(subSymName).second)
        llvm_unreachable("Duplicating linker symbols");
    }
  }

  for (const SymbolRef &sym : file.symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other == ELF::STO_ERAVM_LINKER_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      StringRef symName = cantFail(sym.getName());
      if (!symbolsToBeDefined.contains(symName))
        return true;
    }
  }
  return false;
}

/// Returns a string with the linker symbol definitions passed in
/// \p linkerSymbolValues. For each name from the \p linkerSymbolNames array
/// it creates five symbol definitions. For example, if the linkerSymbolNames[0]
/// points to a string 'symbol_id', it takes the linkerSymbolValues[0] value
/// (which is 20 byte array: 0xAAAAAAAABB.....EEEEEEEE) and creates five symbol
/// definitions:
///
///   "__linker_symbol_id_0" = 0xAAAAAAAA
///   "__linker_symbol_id_1" = 0xBBBBBBBB
///   "__linker_symbol_id_2" = 0xCCCCCCCC
///   "__linker_symbol_id_3" = 0xDDDDDDDD
///   "__linker_symbol_id_4" = 0xEEEEEEEE
///
static std::string createLinkerSymbolDefinitions(
    const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols) {
  std::string symbolsStr;
  for (uint64_t symNum = 0; symNum < numLinkerSymbols; ++symNum) {
    StringRef symbolStr(linkerSymbolNames[symNum]);
    SmallString<LINKER_SYMBOL_SIZE * 2> hexStrSymbolVal;
    toHex(ArrayRef<uint8_t>(
              reinterpret_cast<const uint8_t *>(linkerSymbolValues[symNum]),
              LINKER_SYMBOL_SIZE),
          /*LowerCase*/ false, hexStrSymbolVal);
    for (unsigned idx = 0; idx < LINKER_SYMBOL_SIZE / linkerSubSymbolRelocSize;
         ++idx) {
      symbolsStr += "\"";
      symbolsStr += EraVM::getLinkerIndexedName(
          EraVM::getLinkerSymbolHash(symbolStr), idx);
      symbolsStr += "\"";
      symbolsStr += " = 0x";
      symbolsStr += hexStrSymbolVal
                        .substr(2 * linkerSubSymbolRelocSize * idx,
                                2 * linkerSubSymbolRelocSize)
                        .str();
      symbolsStr += ";\n";
    }
  }
  return symbolsStr;
}

/// Creates a linker script used to generate an relocatable ELF file. The
/// script contains only the linker symbol definitions.
/// Here is an example of the resulting linker script:
///
///   "__linker_library_id_0" = 0x01010101;
///   "__linker_library_id_1" = 0x02020202;
///   "__linker_library_id_2" = 0x03030303;
///   "__linker_library_id_3" = 0x04040404;
///   "__linker_library_id_4" = 0x05050505;
///
static std::string
createEraVMRelLinkerScript(const char *const *linkerSymbolNames,
                           const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
                           uint64_t numLinkerSymbols) {
  return createLinkerSymbolDefinitions(linkerSymbolNames, linkerSymbolValues,
                                       numLinkerSymbols);
}

/// Creates a linker script used to generate an executable byte code. The
/// script describes both the byte code layout according to the
/// 'EraVM Binary Layout' specification and the linker symbol definitions.
/// Here is an example of the resulting linker script:
///
///   "__linker_library_id2_0" = 0x06060606;
///   "__linker_library_id2_1" = 0x07070707;
///   "__linker_library_id2_2" = 0x08080808;
///   "__linker_library_id2_3" = 0x09090909;
///   "__linker_library_id2_4" = 0x0A0B0C0D;
///   ENTRY(0);
///   SECTIONS {
///     .code : SUBALIGN(1) {
///       *(.text)
///
///       ASSERT(. % 8 == 0, "size of instructions isn't multiple of 8");
///
///       /* Check that the code size isn't more than 2^16 instructions. */
///       ASSERT(. <= (1 << 16) * 8, "number of instructions > 2^16")
///
///       /* Align the .rodata to 32 bytes. */
///       . = ALIGN(32);
///       *(.rodata)
///
///       ASSERT(. % 32 == 0, "size isn't multiple of 32");
///
///       /* Add padding before the metadata. Here metadata size is 32 bytes */
///       . = ((((. + 32) >> 5) | 1 ) << 5) - 32;
///       *(.eravm-metadata)
///
///       ASSERT(. % 64 == 32, "size isn't odd number of words");
///
///       /* Check the total binary size is not more than (2^16 - 2) words. */
///       ASSERT(. <= ((1 << 16) - 1) * 32, "Binary size > (2^16 - 2) words")
///     } = 0
///
///     /* .data section itself, that contains initializers of global variables,
///        is not needed. */
///     /DISCARD/ : {
///       *(.data)
///   }}
///
static std::string
createEraVMExeLinkerScript(uint64_t metadataSize,
                           const char *const *linkerSymbolNames,
                           const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
                           uint64_t numLinkerSymbols) {
  std::string linkerSymbolNamesStr = createLinkerSymbolDefinitions(
      linkerSymbolNames, linkerSymbolValues, numLinkerSymbols);

  // The final bytecode should be padded such that its size be the
  // odd number of words, i.e 2 * (N + 1).
  // Add padding before the metadata section such that the final
  // bytecode size to be the even number of words. The metadata
  // may have arbitrary size.
  uint64_t alignedMDSize = alignTo(metadataSize, Align(32), 0);
  // Create the resulting padding expression which is
  //  . = ((((. + alignedMDSize) >> 5) | 1 ) << 5) - metadataSize - .
  std::string padding =
      formatv(". = ((((. + {0}) >> 5) | 1 ) << 5) - {1};\n",
              std::to_string(alignedMDSize), std::to_string(metadataSize))
          .str();

  Twine scriptPart1 = Twine("\
ENTRY(0);                                                                   \n\
SECTIONS {                                                                  \n\
  .code : SUBALIGN(1) {                                                     \n\
    *(.text)                                                                \n\
                                                                            \n\
    ASSERT(. % 8 == 0, \"size of instructions isn't multiple of 8\");       \n\
                                                                            \n\
    /* Check that the code size isn't more than 2^16 instructions. */       \n\
    ASSERT(. <= (1 << 16) * 8, \"number of instructions > 2^16\")           \n\
                                                                            \n\
    /* Align the .rodata to 32 bytes. */                                    \n\
    . = ALIGN(32);                                                          \n\
    *(.rodata)                                                              \n\
                                                                            \n\
    ASSERT(. % 32 == 0, \"size isn't multiple of 32\");                     \n\
                                                                            \n\
    /* Add padding before the metadata */\n");

  Twine scriptPart2 = Twine("\
    *(.eravm-metadata)                                                      \n\
                                                                            \n\
    ASSERT(. % 64 == 32, \"size isn't odd number of words\");               \n\
                                                                            \n\
    /* Check the total binary size isn't more than (2^16 - 2) words. */     \n\
    ASSERT(. <= ((1 << 16) - 1) * 32, \"Binary size > (2^16 - 2) words\")   \n\
  } = 0                                                                     \n\
                                                                            \n\
  /* .data section itself, that contains initializers of global variables,  \n\
     is not needed. */                                                      \n\
  /DISCARD/ : {                                                             \n\
    *(.data)                                                                \n\
  }}");

  return (linkerSymbolNamesStr + scriptPart1 + padding + scriptPart2).str();
}

/// Performs linkage of the ELF object file passed in \p inBuffer, as
/// described in the header. It works by creating a linker script
/// depending on the binary type to be produced (ELF relocatable vs byte code
/// with stripped ELF format) and passing it with the input file to the LLD.
LLVMBool LLVMLinkEraVM(LLVMMemoryBufferRef inBuffer,
                       LLVMMemoryBufferRef *outBuffer,
                       const char *const *linkerSymbolNames,
                       const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
                       uint64_t numLinkerSymbols, char **errorMessage) {
  // The first array element is the input memory buffer.
  // The second one is a buffer with the linker script.
  SmallVector<MemoryBufferRef> localInMemBufRefs(2);
  localInMemBufRefs[0] = *unwrap(inBuffer);

  *outBuffer = nullptr;
  if (!LLVMIsELFEraVM(inBuffer)) {
    *errorMessage = strdup("Input binary is not an EraVM ELF file");
    return true;
  }

  std::unique_ptr<Binary> InBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  assert(InBinary->isObject());

  bool shouldEmitRelocatable =
      hasUndefLinkerSymbols(*static_cast<ObjectFile *>(InBinary.get()),
                            linkerSymbolNames, numLinkerSymbols);

  // Input ELF has undefined linker symbols, but no one definition was
  // provided, which means the linker has nothing to do with the ELF file,
  // so just copy it to the out buffer.
  if (shouldEmitRelocatable && !numLinkerSymbols) {
    StringRef inData = localInMemBufRefs[0].getBuffer();
    *outBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(
        inData.data(), inData.size(), "result");
    return false;
  }

  // Get the size of the metadata (if any) contained in the '.eravm-metadata'
  // section of the input ELF file.
  uint64_t MDSize = getSectionSize(*static_cast<ObjectFile *>(InBinary.get()),
                                   ".eravm-metadata");

  // The input ELF file can have undefined symbols. There can be the following
  // cases:
  //   - there are undefined linker symbols for which the 'linkerSymbolValues'
  //     arguments do not provide definitions. In this case we emit a trivial
  //     linker script that provides definitions for the passed linker symbols.
  //   - there are undefined symbols other that the liker ones. This situation
  //     is treated as unreachable. If that happened, it means there is a bug
  //     in the FE/LLVM codegen/Linker. Technically, such an error will be
  //     handled by the LLD itself by returning 'undefined symbol' error.
  //   - all the linker symbols are either defined, or 'linkerSymbolValues'
  //     provides their definitions. In this case we create a linker script
  //     for the executable byte code generation.
  std::string linkerScript =
      shouldEmitRelocatable
          ? createEraVMRelLinkerScript(linkerSymbolNames, linkerSymbolValues,
                                       numLinkerSymbols)
          : createEraVMExeLinkerScript(MDSize, linkerSymbolNames,
                                       linkerSymbolValues, numLinkerSymbols);

  std::unique_ptr<MemoryBuffer> linkerScriptBuf =
      MemoryBuffer::getMemBuffer(linkerScript, "1");

  localInMemBufRefs[1] = linkerScriptBuf->getMemBufferRef();

  SmallVector<const char *, 16> lldArgs;
  lldArgs.push_back("ld.lld");

  // Push the name of the linker script - '1'.
  lldArgs.push_back("-T");
  lldArgs.push_back("1");

  // Push the name of the input object file - '0'.
  lldArgs.push_back("0");

  // If all the symbols are supposed to be resolved, strip out the ELF format
  // end emit the final bytecode. Otherwise emit an ELF relocatable file.
  if (shouldEmitRelocatable)
    lldArgs.push_back("--relocatable");
  else
    lldArgs.push_back("--oformat=binary");

  SmallString<0> codeString;
  raw_svector_ostream ostream(codeString);
  SmallString<0> errorString;
  raw_svector_ostream errorOstream(errorString);

  // Lld-as-a-library is not thread safe, as it has a global state,
  // so we need to protect lld from simultaneous access from different threads.
  std::unique_lock<std::mutex> lock(lldMutex);
  const lld::Result s =
      lld::lldMainMemBuf(localInMemBufRefs, &ostream, lldArgs, outs(),
                         errorOstream, {{lld::Gnu, &lld::elf::linkMemBuf}});
  lock.unlock();

  bool Ret = !s.retCode && s.canRunAgain;
  // For unification with other LLVM C-API functions, return 'true' in case of
  // an error.
  if (!Ret) {
    *errorMessage = strdup(errorString.c_str());
    return true;
  }

  StringRef data = ostream.str();
  *outBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(),
                                                         data.size(), "result");

  return false;
}

/// Returns true if the \p inBuffer contains an ELF object file.
LLVMBool LLVMIsELFEraVM(LLVMMemoryBufferRef inBuffer) {
  Expected<ELFObjectFile<ELF32LE>> inBinaryOrErr =
      ELFObjectFile<ELF32LE>::create(unwrap(inBuffer)->getMemBufferRef());
  if (!inBinaryOrErr) {
    handleAllErrors(inBinaryOrErr.takeError(), [](const ErrorInfoBase &EI) {});
    return false;
  }
  return inBinaryOrErr.get().getArch() == Triple::eravm;
}

/// Returns an array of undefined linker symbol names of the ELF object passed
/// in \p inBuffer.
char **LLVMGetUndefinedLinkerSymbolsEraVM(LLVMMemoryBufferRef inBuffer,
                                          uint64_t *numLinkerSymbols) {
  if (!LLVMIsELFEraVM(inBuffer)) {
    *numLinkerSymbols = 0;
    return nullptr;
  }

  StringSet<> undefSymbols;
  StringSet<> undefSubSymbols;
  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  const auto *oFile = static_cast<ObjectFile *>(inBinary.get());
  for (const SymbolRef &sym : oFile->symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other == ELF::STO_ERAVM_LINKER_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      StringRef subName = cantFail(sym.getName());
      undefSubSymbols.insert(subName);
      std::string symName = EraVM::stripLinkerSymbolNameIndex(subName);
      undefSymbols.insert(symName);
    }
  }

  *numLinkerSymbols = undefSymbols.size();
  if (!undefSymbols.size())
    return nullptr;

  char **linkerSymbolNames = reinterpret_cast<char **>(
      std::malloc(undefSymbols.size() * sizeof(char *)));
  unsigned idx = 0;
  for (const StringSet<>::value_type &entry : undefSymbols) {
    StringRef symName = entry.first();
    // Check that 'undefSubSymbols' forms a set of groups each consisting of
    // five sub-symbols.
    for (unsigned idx = 0; idx < LINKER_SYMBOL_SIZE / linkerSubSymbolRelocSize;
         idx++) {
      std::string subSymName = EraVM::getLinkerIndexedName(symName, idx);
      if (!undefSubSymbols.contains(subSymName))
        llvm_unreachable("missing a library sub-symbol");
    }
    std::string secName = EraVM::getLinkerSymbolSectionName(symName);
    linkerSymbolNames[idx++] =
        strdup(getSectionContent(*oFile, secName).str().c_str());
  }

  return linkerSymbolNames;
}

/// Disposes an array with linker symbols returned by the
/// LLVMGetUndefinedLinkerSymbolsEraVM().
void LLVMDisposeUndefinedLinkerSymbolsEraVM(char *linkerSymbolNames[],
                                            uint64_t numLinkerSymbols) {
  for (unsigned idx = 0; idx < numLinkerSymbols; ++idx)
    std::free(linkerSymbolNames[idx]);
  std::free(linkerSymbolNames);
}
