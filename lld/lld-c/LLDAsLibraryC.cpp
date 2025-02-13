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

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

using namespace llvm;
using namespace object;

LLD_HAS_DRIVER_MEM_BUF(elf)

namespace llvm {
namespace EraVM {
// The following two functions are defined in EraVMMCTargetDesc.cpp.
std::string getSymbolHash(StringRef Name);
std::string getSymbolIndexedName(StringRef Name, unsigned SubIdx);
std::string getSymbolSectionName(StringRef Name);
std::string getNonIndexedSymbolName(StringRef Name);
std::string getLinkerSymbolName(StringRef Name);
std::string getFactoryDependencySymbolName(StringRef BaseName);
bool isLinkerSymbolName(StringRef Name);
bool isFactoryDependencySymbolName(StringRef Name);
} // namespace EraVM

namespace EVM {
std::string getLinkerSymbolHash(StringRef SymName);
std::string getLinkerSymbolSectionName(StringRef Name);
std::string getDataSizeSymbol(StringRef SymbolName);
std::string getDataOffsetSymbol(StringRef SymbolName);
std::string getDataOffsetSymbol(StringRef Name);
bool isDataOffsetSymbolName(StringRef Name);
std::string extractDataOffseteName(StringRef SymbolName);
bool isLoadImmutableSymbolName(StringRef Name);
std::string getImmutableId(StringRef Name);
} // namespace EVM
} // namespace llvm

enum class ReferenceSymbolType { Linker, Factory };

constexpr static unsigned subSymbolRelocSize = sizeof(uint32_t);

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
    report_fatal_error(Twine("lld: expected ") + sectionName +
                       " in object file");

  return cantFail(si->getContents());
}

static std::string getLinkerSubSymbolName(StringRef name, unsigned idx) {
  return EraVM::getSymbolIndexedName(
      EraVM::getLinkerSymbolName(EraVM::getSymbolHash(name)), idx);
}

static std::string getFactoryDepSubSymbolName(StringRef name, unsigned idx) {
  return EraVM::getSymbolIndexedName(
      EraVM::getFactoryDependencySymbolName(EraVM::getSymbolHash(name)), idx);
}

/// Returns true if the object file \p file contains any other undefined
/// linker/factory dependency symbols besides those passed in
/// \p linkerSymbolNames and \p factoryDepSymbolNames.
static bool hasUndefinedReferenceSymbols(
    ObjectFile &file, const char *const *linkerSymbolNames,
    uint64_t numLinkerSymbols, const char *const *factoryDepSymbolNames,
    uint64_t numFactoryDepSymbols) {
  StringSet<> symbolsToBeDefined;
  // Create a set of possible symbols from the 'linkerSymbolNames' array.
  for (unsigned symIdx = 0; symIdx < numLinkerSymbols; ++symIdx) {
    for (unsigned subSymIdx = 0;
         subSymIdx < LINKER_SYMBOL_SIZE / subSymbolRelocSize; ++subSymIdx) {
      std::string subSymName =
          getLinkerSubSymbolName(linkerSymbolNames[symIdx], subSymIdx);
      if (!symbolsToBeDefined.insert(subSymName).second)
        report_fatal_error(Twine("lld: duplicating reference symbol ") +
                           subSymName + "in object file");
    }
  }
  for (unsigned symIdx = 0; symIdx < numFactoryDepSymbols; ++symIdx) {
    for (unsigned subSymIdx = 0;
         subSymIdx < FACTORYDEPENDENCY_SYMBOL_SIZE / subSymbolRelocSize;
         ++subSymIdx) {
      std::string subSymName =
          getFactoryDepSubSymbolName(factoryDepSymbolNames[symIdx], subSymIdx);
      if (!symbolsToBeDefined.insert(subSymName).second)
        report_fatal_error(Twine("lld: duplicating reference symbol ") +
                           subSymName + "in object file");
    }
  }

  for (const SymbolRef &sym : file.symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other == ELF::STO_ERAVM_REFERENCE_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      StringRef symName = cantFail(sym.getName());
      if (!symbolsToBeDefined.contains(symName))
        return true;
    }
  }
  return false;
}

/// Returns a string with the symbol definitions passed in
/// \p linkerSymbolValues and \p factoryDependencySymbolNames.
/// For each name from the \p linkerSymbolNames array it creates
/// five symbol definitions.
/// For example, if the linkerSymbolNames[0] points to a string 'symbol_id',
/// it takes the linkerSymbolValues[0] value
/// (which is 20 byte array: 0xAAAAAAAABB.....EEEEEEEE) and creates five symbol
/// definitions:
///
///   __linker_symbol_id_0 = 0xAAAAAAAA
///   __linker_symbol_id_1 = 0xBBBBBBBB
///   __linker_symbol_id_2 = 0xCCCCCCCC
///   __linker_symbol_id_3 = 0xDDDDDDDD
///   __linker_symbol_id_4 = 0xEEEEEEEE
///
/// The is true for \p factoryDependencySymbolNames. The only difference is
/// that for a factory dependency symbol we read eight symbols.
static std::string createSymbolDefinitions(
    const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols, const char *const *factoryDependencySymbolNames,
    const char factoryDependencySymbolValues[][FACTORYDEPENDENCY_SYMBOL_SIZE],
    uint64_t numFactoryDependencySymbols) {
  auto getSymbolDef =
      [](StringRef symName, const char *symVal, size_t symSize,
         std::function<std::string(StringRef, unsigned)> subSymNameFunc)
      -> std::string {
    std::string symbolDefBuf;
    raw_string_ostream symbolDef(symbolDefBuf);
    SmallString<128> hexStrSymbolVal;
    toHex(ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(symVal), symSize),
          /*LowerCase*/ false, hexStrSymbolVal);
    for (unsigned idx = 0; idx < symSize / subSymbolRelocSize; ++idx) {
      symbolDef << subSymNameFunc(symName, idx);
      symbolDef << " = 0x";
      symbolDef << hexStrSymbolVal
                       .substr(2 * subSymbolRelocSize * idx,
                               2 * subSymbolRelocSize)
                       .str();
      symbolDef << ";\n";
    }
    return symbolDef.str();
  };

  std::string symbolsDefBuf;
  raw_string_ostream symbolsDef(symbolsDefBuf);
  for (uint64_t symNum = 0; symNum < numLinkerSymbols; ++symNum)
    symbolsDef << getSymbolDef(linkerSymbolNames[symNum],
                               linkerSymbolValues[symNum], LINKER_SYMBOL_SIZE,
                               &getLinkerSubSymbolName);

  for (uint64_t symNum = 0; symNum < numFactoryDependencySymbols; ++symNum)
    symbolsDef << getSymbolDef(factoryDependencySymbolNames[symNum],
                               factoryDependencySymbolValues[symNum],
                               FACTORYDEPENDENCY_SYMBOL_SIZE,
                               &getFactoryDepSubSymbolName);
  return symbolsDef.str();
}

/// Creates a linker script used to generate an relocatable ELF file. The
/// script contains only the linker symbol definitions.
/// Here is an example of the resulting linker script:
///
///   __linker_library_id_0 = 0x01010101;
///   __linker_library_id_1 = 0x02020202;
///   __linker_library_id_2 = 0x03030303;
///   __linker_library_id_3 = 0x04040404;
///   __linker_library_id_4 = 0x05050505;
///
static std::string createEraVMRelLinkerScript(
    const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols, const char *const *factoryDependencySymbolNames,
    const char factoryDependencySymbolValues[][FACTORYDEPENDENCY_SYMBOL_SIZE],
    uint64_t numFactoryDependencySymbols) {
  return createSymbolDefinitions(linkerSymbolNames, linkerSymbolValues,
                                 numLinkerSymbols, factoryDependencySymbolNames,
                                 factoryDependencySymbolValues,
                                 numFactoryDependencySymbols);
}

/// Creates a linker script used to generate an executable byte code. The
/// script describes both the byte code layout according to the
/// 'EraVM Binary Layout' specification and the linker symbol definitions.
/// Here is an example of the resulting linker script:
///
///   __linker_library_id_0 = 0x06060606;
///   __linker_library_id_1 = 0x07070707;
///   __linker_library_id_2 = 0x08080808;
///   __linker_library_id_3 = 0x09090909;
///   __linker_library_id_4 = 0x0A0B0C0D;
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
static std::string createEraVMExeLinkerScript(
    uint64_t metadataSize, const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols, const char *const *factoryDependencySymbolNames,
    const char factoryDependencySymbolValues[][FACTORYDEPENDENCY_SYMBOL_SIZE],
    uint64_t numFactoryDependencySymbols) {
  std::string linkerSymbolNamesStr = createSymbolDefinitions(
      linkerSymbolNames, linkerSymbolValues, numLinkerSymbols,
      factoryDependencySymbolNames, factoryDependencySymbolValues,
      numFactoryDependencySymbols);

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

  std::string scriptPart1 = "\
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
    /* Add padding before the metadata */\n";

  std::string scriptPart2 = "\
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
  }}";

  return linkerSymbolNamesStr + scriptPart1 + padding + scriptPart2;
}

/// Performs linkage of the ELF object file passed in \p inBuffer, as
/// described in the header. It works by creating a linker script
/// depending on the binary type to be produced (ELF relocatable vs byte code
/// with stripped ELF format) and passing it with the input file to the LLD.
LLVMBool LLVMLinkEraVM(
    LLVMMemoryBufferRef inBuffer, LLVMMemoryBufferRef *outBuffer,
    const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols, const char *const *factoryDependencySymbolNames,
    const char factoryDependencySymbolValues[][FACTORYDEPENDENCY_SYMBOL_SIZE],
    uint64_t numFactoryDependencySymbols, char **errorMessage) {
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

  bool shouldEmitRelocatable = hasUndefinedReferenceSymbols(
      *static_cast<ObjectFile *>(InBinary.get()), linkerSymbolNames,
      numLinkerSymbols, factoryDependencySymbolNames,
      numFactoryDependencySymbols);

  // Input ELF has undefined linker/factory symbols, but no one definition was
  // provided, which means the linker has nothing to do with the ELF file,
  // so just copy it to the out buffer.
  if (shouldEmitRelocatable && !numLinkerSymbols &&
      !numFactoryDependencySymbols) {
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
          ? createEraVMRelLinkerScript(
                linkerSymbolNames, linkerSymbolValues, numLinkerSymbols,
                factoryDependencySymbolNames, factoryDependencySymbolValues,
                numFactoryDependencySymbols)
          : createEraVMExeLinkerScript(
                MDSize, linkerSymbolNames, linkerSymbolValues, numLinkerSymbols,
                factoryDependencySymbolNames, factoryDependencySymbolValues,
                numFactoryDependencySymbols);

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

/// Returns an array of names of undefined reference symbols in the
/// ELF object.
static char **LLVMGetUndefinedSymbols(const ObjectFile *oFile,
                                      uint64_t *numSymbols,
                                      ReferenceSymbolType symbolType) {
  StringSet<> undefSymbols;
  StringSet<> undefSubSymbols;
  for (const SymbolRef &sym : oFile->symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other == ELF::STO_ERAVM_REFERENCE_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      StringRef subName = cantFail(sym.getName());
      if ((symbolType == ReferenceSymbolType::Linker &&
           EraVM::isLinkerSymbolName(subName)) ||
          (symbolType == ReferenceSymbolType::Factory &&
           EraVM::isFactoryDependencySymbolName(subName))) {
        undefSubSymbols.insert(subName);
        std::string symName = EraVM::getNonIndexedSymbolName(subName);
        undefSymbols.insert(symName);
      }
    }
  }

  *numSymbols = undefSymbols.size();
  if (!undefSymbols.size())
    return nullptr;

  char **undefSymbolNames = reinterpret_cast<char **>(
      std::malloc(undefSymbols.size() * sizeof(char *)));
  unsigned undefSymIdx = 0;
  const unsigned symbolSize = symbolType == ReferenceSymbolType::Linker
                                  ? LINKER_SYMBOL_SIZE
                                  : FACTORYDEPENDENCY_SYMBOL_SIZE;
  for (const StringSet<>::value_type &entry : undefSymbols) {
    StringRef symName = entry.first();
    // Check that 'undefSubSymbols' forms a set of groups each consisting of
    // five or eight sub-symbols depending on the symbol type.
    for (unsigned idx = 0; idx < symbolSize / subSymbolRelocSize; idx++) {
      std::string subSymName = EraVM::getSymbolIndexedName(symName, idx);
      if (!undefSubSymbols.contains(subSymName)) {
        report_fatal_error(Twine("lld: missing reference symbol ") +
                           subSymName);
      }
    }
    std::string secName = EraVM::getSymbolSectionName(symName);
    undefSymbolNames[undefSymIdx++] =
        strdup(getSectionContent(*oFile, secName).str().c_str());
  }

  // Sort the returned names in lexicographical order.
  std::sort(
      undefSymbolNames, undefSymbolNames + *numSymbols,
      [](const char *s1, const char *s2) { return std::strcmp(s1, s2) < 0; });

  return undefSymbolNames;
}

/// Returns an array of undefined linker/factory dependency symbol names
/// of the ELF object passed in \p inBuffer.
void LLVMGetUndefinedReferencesEraVM(LLVMMemoryBufferRef inBuffer,
                                     char ***linkerSymbols,
                                     uint64_t *numLinkerSymbols,
                                     char ***factoryDepSymbols,
                                     uint64_t *numFactoryDepSymbols) {
  if (linkerSymbols) {
    *numLinkerSymbols = 0;
    *linkerSymbols = nullptr;
  }

  if (factoryDepSymbols) {
    *numFactoryDepSymbols = 0;
    *factoryDepSymbols = nullptr;
  }

  if (!LLVMIsELFEraVM(inBuffer))
    return;

  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  const auto *oFile = static_cast<const ObjectFile *>(inBinary.get());

  if (linkerSymbols)
    *linkerSymbols = LLVMGetUndefinedSymbols(oFile, numLinkerSymbols,
                                             ReferenceSymbolType::Linker);
  if (factoryDepSymbols)
    *factoryDepSymbols = LLVMGetUndefinedSymbols(oFile, numFactoryDepSymbols,
                                                 ReferenceSymbolType::Factory);
}

/// Disposes an array with symbols returned by the
/// LLVMGetUndefinedReferences* functions.
void LLVMDisposeUndefinedReferences(char *symbolNames[], uint64_t numSymbols) {
  for (unsigned idx = 0; idx < numSymbols; ++idx)
    std::free(symbolNames[idx]);
  std::free(symbolNames);
}

//----------------------------------------------------------------------------//

/// Create linker script as described in the createEraVMRelLinkerScript.
static std::string createEVMLinkerSymbolsDefinitions(
    const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols) {
  return createSymbolDefinitions(linkerSymbolNames, linkerSymbolValues,
                                 numLinkerSymbols, nullptr, nullptr, 0);
}

/// Returns true if the \p inBuffer contains an ELF object file.
LLVMBool LLVMIsELFEVM(LLVMMemoryBufferRef inBuffer) {
  Expected<ELFObjectFile<ELF32BE>> inBinaryOrErr =
      ELFObjectFile<ELF32BE>::create(unwrap(inBuffer)->getMemBufferRef());
  if (!inBinaryOrErr) {
    handleAllErrors(inBinaryOrErr.takeError(), [](const ErrorInfoBase &EI) {});
    return false;
  }
  return inBinaryOrErr.get().getArch() == Triple::evm;
}

/// Returns an array of undefined linker symbol names
/// of the ELF object passed in \p inBuffer.
void LLVMGetUndefinedReferencesEVM(LLVMMemoryBufferRef inBuffer,
                                   char ***linkerSymbols,
                                   uint64_t *numLinkerSymbols) {
  if (linkerSymbols) {
    *numLinkerSymbols = 0;
    *linkerSymbols = nullptr;
  }

  if (!LLVMIsELFEVM(inBuffer))
    return;

  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  const auto *oFile = static_cast<const ObjectFile *>(inBinary.get());

  if (linkerSymbols)
    *linkerSymbols = LLVMGetUndefinedSymbols(oFile, numLinkerSymbols,
                                             ReferenceSymbolType::Linker);
}

/// Resolves undefined linker symbols in the ELF object file \inBuffer.
/// Returns ELF object file if there remain unresolved linker symbols.
/// Otherwise returns the bytecode.
LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef inBuffer,
                     LLVMMemoryBufferRef *outBuffer,
                     const char *const *linkerSymbolNames,
                     const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
                     uint64_t numLinkerSymbols, char **errorMessage) {
  SmallVector<MemoryBufferRef> localInMemBufRefs(2);
  localInMemBufRefs[0] = *unwrap(inBuffer);

  *outBuffer = nullptr;
  if (!LLVMIsELFEVM(inBuffer)) {
    *errorMessage = strdup("Input binary is not an EVM ELF file");
    return true;
  }

  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(localInMemBufRefs[0]));
  assert(inBinary->isObject());
  bool shouldEmitRelocatable = hasUndefinedReferenceSymbols(
      *static_cast<ObjectFile *>(inBinary.get()), linkerSymbolNames,
      numLinkerSymbols, nullptr, 0);

  std::string linkerScript = createEVMLinkerSymbolsDefinitions(
      linkerSymbolNames, linkerSymbolValues, numLinkerSymbols);

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

  bool ret = !s.retCode && s.canRunAgain;
  if (!ret) {
    *errorMessage = strdup(errorString.c_str());
    return true;
  }

  StringRef data = ostream.str();
  *outBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(),
                                                         data.size(), "deploy");

  return false;
}

static std::string createEVMAssembeScript(ArrayRef<LLVMMemoryBufferRef> memBufs,
                                          ArrayRef<const char *> bufIDs) {
  assert(memBufs.size() == bufIDs.size());

  auto getDataSizeName = [](StringRef name) {
    return EVM::getDataSizeSymbol(EVM::getLinkerSymbolHash(name));
  };

  // A set of all the defined symbols within the script.
  std::string definedSymbolsBuf;
  raw_string_ostream definedSymbols(definedSymbolsBuf);

  std::unique_ptr<Binary> topLevelBinary =
      cantFail(createBinary(unwrap(memBufs[0])->getMemBufferRef()));
  const auto *topLevelObjFile =
      static_cast<const ObjectFile *>(topLevelBinary.get());

  // Set of objects names that are referenced from the top level object.
  StringSet<> topLevelDataOffsetRefs;
  for (const SymbolRef &sym : topLevelObjFile->symbols()) {
    section_iterator symSec = cantFail(sym.getSection());
    if (symSec != topLevelObjFile->section_end())
      continue;

    StringRef symName = cantFail(sym.getName());
    if (EVM::isDataOffsetSymbolName(symName)) {
      std::string objName = EVM::extractDataOffseteName(symName);
      topLevelDataOffsetRefs.insert(objName);
    }
  }

  std::string textSectionBuf;
  raw_string_ostream textSection(textSectionBuf);

  // Define symbols whose values represent sizes of the dependencies.
  // Skip the topLevelObject, as its size denotes .text section size of
  // the resulting object file.
  for (unsigned idx = 1; idx < memBufs.size(); ++idx) {
    std::unique_ptr<Binary> binary =
        cantFail(createBinary(unwrap(memBufs[idx])->getMemBufferRef()));
    const auto *objFile = static_cast<const ObjectFile *>(binary.get());
    for (const SectionRef &sec : objFile->sections()) {
      if (!sec.isText())
        continue;

      StringRef secName = cantFail(sec.getName());
      if (secName == ".text") {
        std::string sym = getDataSizeName(bufIDs[idx]);
        definedSymbols << sym << ";\n";
        textSection << sym << " = " << sec.getSize() << ";\n";
        break;
      }
    }
  }

  //   __dataoffset_Id_1 = .;
  //   Id_1(.text);
  //   __dataoffset_Id_2 = .;
  //   Id_2(.text);
  //   ...
  //   __dataoffset_Id_N = .;
  //   Id_N(.text);
  for (unsigned idx = 0; idx < memBufs.size(); ++idx) {
    std::string bufIdHash = EVM::getLinkerSymbolHash(bufIDs[idx]);
    // Do not link the dependency if it's not referenced via
    // __dataoffset.
    if (idx > 0 && !topLevelDataOffsetRefs.count(bufIdHash))
      continue;

    std::string sym = EVM::getDataOffsetSymbol(bufIdHash);
    definedSymbols << sym << ";\n";
    textSection << sym << " = .;\n";
    textSection << bufIdHash << "(.text);\n";
  }

  // Define the symbol whose value is size of the .text section.
  std::string topDataSizeSym = getDataSizeName(bufIDs[0]);
  definedSymbols << topDataSizeSym << ";\n";
  textSection << topDataSizeSym << " = .;\n";

  std::string script = formatv("ENTRY(0);\n\
SECTIONS {\n\
  . = 0;\n\
  .text : SUBALIGN(1) {\n\
{0}\
  }\n\
}\n\
VERSION {\n\
  { local:\n\
{1}\
  };\n\
};\n\
",
                               textSection.str(), definedSymbols.str());

  return script;
}

/// Links the deploy and runtime ELF object files using the information about
//  dependencies.
LLVMBool LLVMAssembleEVM(LLVMMemoryBufferRef inBuffers[],
                         const char *inBuffersIDs[], uint64_t numInBuffers,
                         LLVMMemoryBufferRef *outBuffer, char **errorMessage) {
  assert(numInBuffers > 1);
  SmallVector<MemoryBufferRef> localInMemBufRefs(numInBuffers + 1);
  SmallVector<std::unique_ptr<MemoryBuffer>> localInMemBufs(numInBuffers + 1);

  // TODO: #740. Verify that the object files contain sections with original
  // inBuffersIDs, i.e. before taking hash.
  for (unsigned idx = 0; idx < numInBuffers; ++idx) {
    MemoryBufferRef ref = *unwrap(inBuffers[idx]);
    // We need to copy buffers to be able to change their names, as this matters
    // for the linker.
    localInMemBufs[idx] = MemoryBuffer::getMemBufferCopy(
        ref.getBuffer(), EVM::getLinkerSymbolHash(inBuffersIDs[idx]));
    localInMemBufRefs[idx] = localInMemBufs[idx]->getMemBufferRef();
  }

  std::string linkerScript = createEVMAssembeScript(
      ArrayRef(inBuffers, numInBuffers), ArrayRef(inBuffersIDs, numInBuffers));

  std::unique_ptr<MemoryBuffer> scriptBuf =
      MemoryBuffer::getMemBuffer(linkerScript, "script.x");
  localInMemBufRefs[numInBuffers] = scriptBuf->getMemBufferRef();

  SmallVector<const char *, 16> lldArgs;
  lldArgs.push_back("ld.lld");

  // Use remapping of file names (a linker feature) to replace file names with
  // indexes in the array of memory buffers.
  const std::string remapStr("--remap-inputs=");
  SmallVector<std::string> args;
  for (unsigned idx = 0; idx < numInBuffers; ++idx)
    args.emplace_back(remapStr + EVM::getLinkerSymbolHash(inBuffersIDs[idx]) +
                      "=" + std::to_string(idx));

  args.emplace_back("-T");
  args.emplace_back(std::to_string(numInBuffers));

  for (unsigned idx = 0; idx < numInBuffers; ++idx)
    args.emplace_back(std::to_string(idx));

  for (const std::string &arg : args)
    lldArgs.push_back(arg.c_str());

  lldArgs.push_back("-r");

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

  bool ret = !s.retCode && s.canRunAgain;
  if (!ret) {
    *errorMessage = strdup(errorString.c_str());
    return true;
  }

  StringRef data = ostream.str();
  *outBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data(), data.size(),
      EVM::getLinkerSymbolHash(inBuffersIDs[0]).c_str());

  return false;
}

/// Returns immutables and their offsets of the ELF object
/// file passed in \p inBuffer.
uint64_t LLVMGetImmutablesEVM(LLVMMemoryBufferRef inBuffer,
                              char ***immutableIDs,
                              uint64_t **immutableOffsets) {
  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  const auto *objFile = static_cast<const ObjectFile *>(inBinary.get());

  // Maps immutable IDs to their references in the object code.
  StringMap<SmallVector<uint64_t>> immutablesMap;
  for (const SymbolRef &sym : objFile->symbols()) {
    section_iterator symSec = cantFail(sym.getSection());
    if (symSec == objFile->section_end())
      continue;

    StringRef symName = cantFail(sym.getName());
    if (EVM::isLoadImmutableSymbolName(symName)) {
      std::string Id = EVM::getImmutableId(symName);
      uint64_t symOffset = cantFail(sym.getValue());
      // The symbol points to the beginning of a PUSH32 instruction.
      // We have to add 1 (opcode size) to get offset to the PUSH32
      // instruction operand.
      immutablesMap[Id].push_back(symOffset + 1);
    }
  }

  if (immutablesMap.empty()) {
    *immutableIDs = nullptr;
    *immutableOffsets = nullptr;
    return 0;
  }

  uint64_t numOfImmutables = 0;
  for (const auto &[id, offsets] : immutablesMap) {
    numOfImmutables += offsets.size();
  };

  *immutableIDs =
      reinterpret_cast<char **>(std::malloc(numOfImmutables * sizeof(char *)));
  *immutableOffsets = reinterpret_cast<uint64_t *>(
      std::malloc(numOfImmutables * sizeof(uint64_t)));

  unsigned idx = 0;
  for (const auto &[id, offsets] : immutablesMap) {
    for (uint64_t offset : offsets) {
      assert(idx < numOfImmutables);
      (*immutableIDs)[idx] = strdup(id.str().c_str());
      (*immutableOffsets)[idx++] = offset;
    }
  }

  return numOfImmutables;
}

/// Disposes immutable names and their offsets returned by the
/// LLVMGetImmutablesEVM.
void LLVMDisposeImmutablesEVM(char **immutableIDs, uint64_t *immutableOffsets,
                              uint64_t numOfImmutables) {
  for (unsigned idx = 0; idx < numOfImmutables; ++idx)
    std::free(immutableIDs[idx]);

  std::free(immutableIDs);
  std::free(immutableOffsets);
}
