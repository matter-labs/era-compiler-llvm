#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/ObjCopy/CommonConfig.h"
#include "llvm/ObjCopy/ConfigManager.h"
#include "llvm/ObjCopy/ObjCopy.h"
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
using namespace objcopy;

LLD_HAS_DRIVER_MEM_BUF(elf)

namespace llvm {
namespace EVM {
std::string getLinkerSymbolHash(StringRef SymName);
bool isLinkerSymbolName(StringRef Name);
std::string getLinkerSymbolName(StringRef SymName);
bool isSymbolSectionName(StringRef Name);
std::string getSymbolSectionName(StringRef Name);
std::string getSymbolIndexedName(StringRef Name, unsigned SubIdx);
std::string getDataSizeSymbol(StringRef SymbolName);
std::string getNonIndexedSymbolName(StringRef Name);
bool isDataOffsetSymbolName(StringRef Name);
std::string getDataOffsetSymbol(StringRef Name);
std::string extractDataOffseteName(StringRef SymbolName);
bool isLoadImmutableSymbolName(StringRef Name);
std::string getImmutableId(StringRef Name);
} // namespace EVM
} // namespace llvm

enum class ReferenceSymbolType { Linker };

constexpr static unsigned subSymbolRelocSize = sizeof(uint32_t);

static std::mutex lldMutex;

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
  return EVM::getSymbolIndexedName(
      EVM::getLinkerSymbolName(EVM::getLinkerSymbolHash(name)), idx);
}

/// Returns true if the object file \p file contains any other undefined
/// linker dependency symbols besides those passed in  \p linkerSymbolNames.
static bool hasUndefinedReferenceSymbols(ObjectFile &file,
                                         const char *const *linkerSymbolNames,
                                         uint64_t numLinkerSymbols) {
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

  for (const SymbolRef &sym : file.symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other == ELF::STO_EVM_REFERENCE_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      StringRef symName = cantFail(sym.getName());
      if (!symbolsToBeDefined.contains(symName))
        return true;
    }
  }
  return false;
}

/// Returns a string with the symbol definitions passed in
/// \p linkerSymbolValues. For each name from the \p linkerSymbolNames array it
/// creates five symbol definitions. For example, if the linkerSymbolNames[0]
/// points to a string 'symbol_id', it takes the linkerSymbolValues[0] value
/// (which is 20 byte array: 0xAAAAAAAABB.....EEEEEEEE) and creates five symbol
/// definitions:
///
///   __linker_symbol_id_0 = 0xAAAAAAAA
///   __linker_symbol_id_1 = 0xBBBBBBBB
///   __linker_symbol_id_2 = 0xCCCCCCCC
///   __linker_symbol_id_3 = 0xDDDDDDDD
///   __linker_symbol_id_4 = 0xEEEEEEEE
///
static std::string
createSymbolDefinitions(const char *const *linkerSymbolNames,
                        const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
                        uint64_t numLinkerSymbols) {
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

  return symbolsDef.str();
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
    if ((other == ELF::STO_EVM_REFERENCE_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      StringRef subName = cantFail(sym.getName());
      if (symbolType == ReferenceSymbolType::Linker &&
           EVM::isLinkerSymbolName(subName)) {
        undefSubSymbols.insert(subName);
        std::string symName = EVM::getNonIndexedSymbolName(subName);
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
  for (const StringSet<>::value_type &entry : undefSymbols) {
    StringRef symName = entry.first();
    // Check that 'undefSubSymbols' forms a set of groups each consisting of
    // five or eight sub-symbols depending on the symbol type.
    for (unsigned idx = 0; idx < LINKER_SYMBOL_SIZE / subSymbolRelocSize; idx++) {
      std::string subSymName = EVM::getSymbolIndexedName(symName, idx);
      if (!undefSubSymbols.contains(subSymName)) {
        report_fatal_error(Twine("lld: missing reference symbol ") +
                           subSymName);
      }
    }
    std::string secName = EVM::getSymbolSectionName(symName);
    undefSymbolNames[undefSymIdx++] =
        strdup(getSectionContent(*oFile, secName).str().c_str());
  }

  // Sort the returned names in lexicographical order.
  std::sort(
      undefSymbolNames, undefSymbolNames + *numSymbols,
      [](const char *s1, const char *s2) { return std::strcmp(s1, s2) < 0; });

  return undefSymbolNames;
}

/// Disposes an array with symbols returned by the
/// LLVMGetUndefinedReferences* functions.
void LLVMDisposeUndefinedReferences(char *symbolNames[], uint64_t numSymbols) {
  for (unsigned idx = 0; idx < numSymbols; ++idx)
    std::free(symbolNames[idx]);
  std::free(symbolNames);
}

//----------------------------------------------------------------------------//

/// Create linker script with linker symbol definitions.
static std::string createEVMLinkerSymbolsDefinitions(
    const char *const *linkerSymbolNames,
    const char linkerSymbolValues[][LINKER_SYMBOL_SIZE],
    uint64_t numLinkerSymbols) {
  return createSymbolDefinitions(linkerSymbolNames, linkerSymbolValues,
                                 numLinkerSymbols);
}

/// Returns true if the \p inBuffer contains an EVM ELF object file.
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
/// from the ELF object provided in \p inBuffer.
void LLVMGetUndefinedReferencesEVM(LLVMMemoryBufferRef inBuffer,
                                   char ***linkerSymbols,
                                   uint64_t *numLinkerSymbols) {
  assert(linkerSymbols && numLinkerSymbols);
  *linkerSymbols = nullptr;
  *numLinkerSymbols = 0;
  if (!LLVMIsELFEVM(inBuffer))
    return;

  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  const auto *oFile = static_cast<const ObjectFile *>(inBinary.get());

  *linkerSymbols = LLVMGetUndefinedSymbols(oFile, numLinkerSymbols,
                                           ReferenceSymbolType::Linker);
}

/// Resolves undefined linker symbols in the ELF object file \p inBuffer.
/// Returns the ELF object file if any linker symbols remain unresolved;
/// otherwise, returns the bytecode.
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
  bool shouldEmitRelocatable =
      hasUndefinedReferenceSymbols(*static_cast<ObjectFile *>(inBinary.get()),
                                   linkerSymbolNames, numLinkerSymbols);

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

  // If all symbols are resolved, strip the ELF format and emit the final
  // bytecode. Otherwise, emit an ELF relocatable file.
  if (shouldEmitRelocatable)
    lldArgs.push_back("--relocatable");
  else
    lldArgs.push_back("--oformat=binary");

  SmallString<0> codeString;
  raw_svector_ostream ostream(codeString);
  SmallString<0> errorString;
  raw_svector_ostream errorOstream(errorString);

  // Lld-as-a-library is not thread-safe due to its global state,
  // so we need to protect it from concurrent access by multiple threads.
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
                                                         data.size(), "result");

  return false;
}

/// Finds symbol name sections and adds them to the \p namesMap.
/// A symbol name section has the following format:
///
///    .symbol_name__linker_symbol__$[0-9a-f]{64}$__
///
static void getSymbolNameSections(const ObjectFile *oFile, const char *fileID,
                                  StringMap<SmallVector<StringRef>> &namesMap) {
  [[maybe_unused]] SmallVector<StringRef> sectionNames;
  for (const SectionRef &sec : oFile->sections()) {
    StringRef secName = cantFail(sec.getName());
    if (EVM::isSymbolSectionName(secName)) {
      namesMap[secName].push_back(fileID);
#ifndef NDEBUG
      sectionNames.push_back(secName);
#endif // NDEBUG
    }
  }

#ifndef NDEBUG
  // Verify that undefined linker symbols exist for the name sections.
  StringMap<unsigned> expectedSymNames;
  for (StringRef secName : sectionNames) {
    StringRef refName = getSectionContent(*oFile, secName);
    for (unsigned idx = 0; idx < LINKER_SYMBOL_SIZE / subSymbolRelocSize;
         ++idx) {
      std::string symName = getLinkerSubSymbolName(refName, idx);
      expectedSymNames[symName]++;
    }
  }

  StringMap<unsigned> actualSymNames;
  for (const SymbolRef &sym : oFile->symbols()) {
    section_iterator symSec = cantFail(sym.getSection());
    // Undefined symbol has no related section.
    if (symSec != oFile->section_end())
      continue;

    StringRef symName = cantFail(sym.getName());
    if (EVM::isLinkerSymbolName(symName))
      actualSymNames[symName]++;
  }
  assert(actualSymNames == expectedSymNames);
#endif // NDEBUG
}

/// Creates a linker script to generate an 'assembly' ELF object file.
/// Here is an example of the script:
///
///   ENTRY(0);
///   SECTIONS {
///     . = 0;
///     .text : SUBALIGN(1) {
///       __datasize__$12c297efd8baf$__ = 268;
///       __dataoffset__$b5e66d52578d$__ = ABSOLUTE(.);
///       __$b5e66d52578d$__(.text);
///       __dataoffset__$12c297efd8baf$__ = ABSOLUTE(.);
///       __$12c297efd8baf$__(.text);
///       __$b5e66d52578d$__(.metadata)
///       __datasize__$b5e66d52578d$__ = ABSOLUTE(.);
///     }
///     /DISCARD/ : {
///     }
///  }
///  VERSION {
///  { local:
///    __datasize__$12c297efd8baf$__;
///    __dataoffset__$b5e66d52578d$__;
///    __dataoffset__$12c297efd8baf$__;
///    __datasize__$b5e66d52578d$__;
///  };
///  };

static std::string createEVMAssembeScript(ArrayRef<LLVMMemoryBufferRef> memBufs,
                                          ArrayRef<const char *> depIDs,
                                          const StringSet<> &depsToLink) {
  assert(memBufs.size() == depIDs.size());

  // Maps symbol name section to an array of object IDs that contain it.
  StringMap<SmallVector<StringRef>> symbolNameSectionsMap;

  auto getDataSizeName = [](StringRef name) {
    return EVM::getDataSizeSymbol(EVM::getLinkerSymbolHash(name));
  };

  // A set of all the defined symbols in the script that should be removed
  // from the symbol table. To achieve this, we set their visibility
  // to 'local'. These symbols will later be removed using the 'objcopy' API.
  std::string definedSymbolsBuf;
  raw_string_ostream definedSymbols(definedSymbolsBuf);

  std::unique_ptr<Binary> firstBinary =
      cantFail(createBinary(unwrap(memBufs[0])->getMemBufferRef()));
  const auto *firstObjFile = static_cast<const ObjectFile *>(firstBinary.get());
  getSymbolNameSections(firstObjFile, depIDs[0], symbolNameSectionsMap);

  std::string textSectionBuf;
  raw_string_ostream textSection(textSectionBuf);

  // A set of all the defined symbols in the script that should be removed
  // from the symbol table. To achieve this, we set their visibility to
  // 'local'. These symbols will later be removed using the 'objcopy' API.
  for (unsigned idx = 1; idx < memBufs.size(); ++idx) {
    std::unique_ptr<Binary> binary =
        cantFail(createBinary(unwrap(memBufs[idx])->getMemBufferRef()));
    const auto *objFile = static_cast<const ObjectFile *>(binary.get());
    std::string bufIdHash = EVM::getLinkerSymbolHash(depIDs[idx]);
    if (depsToLink.count(bufIdHash))
      getSymbolNameSections(objFile, depIDs[idx], symbolNameSectionsMap);

    for (const SectionRef &sec : objFile->sections()) {
      if (sec.isText()) {
        assert(cantFail(sec.getName()) == ".text");
        std::string sym = getDataSizeName(depIDs[idx]);
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
    std::string bufIdHash = EVM::getLinkerSymbolHash(depIDs[idx]);
    // Do not link the dependency if it's not referenced via
    // __dataoffset.
    if (idx != 0 && !depsToLink.count(bufIdHash))
      continue;

    std::string sym = EVM::getDataOffsetSymbol(bufIdHash);
    definedSymbols << sym << ";\n";
    textSection << sym << " = ABSOLUTE(.);\n";
    textSection << bufIdHash << "(.text);\n";
  }

  // Append matadata section of the first object (if any).
  textSection << EVM::getLinkerSymbolHash(depIDs[0]) << "(.metadata);\n";

  // Define a symbol whose value is the total size of the output
  // '.text' section.
  std::string firstDataSizeSym = getDataSizeName(depIDs[0]);
  definedSymbols << firstDataSizeSym << ";\n";
  textSection << firstDataSizeSym << " = ABSOLUTE(.);\n";

  // When assembling multiple ELF files that reference the same library,
  // the files will have identical (name and content) sections:
  //
  //   .symbol_name__linker_symbol__$[0-9a-f]{64}$__
  //
  // By default, the linker will concatenate the sections, which is not desired.
  // We need to retain only one section with the original content in the output
  // file. To achieve this, we discard all duplicate input sections and keep
  // just one.
  std::string discardSectionsBuf;
  raw_string_ostream discardSections(discardSectionsBuf);
  for (const auto &[secName, IDs] : symbolNameSectionsMap) {
    // We need to remove all symbol name sections, retaining just one
    // (regardless of which one). The sections to be removed should be placed
    // in the /DISCARD/ output section.
    assert(IDs.size() > 0);
    for (StringRef id : drop_begin(IDs)) {
      std::string idHash = EVM::getLinkerSymbolHash(id);
      discardSections << idHash << '(' << secName << ");\n";
    }
  }

  std::string script =
      formatv(R"(ENTRY(0);
SECTIONS {
  . = 0;
  .text : SUBALIGN(1) {
{0}
  }
/DISCARD/ : {
{2}
}
}
VERSION {
  { local:
{1}
  };
};)",
              textSection.str(), definedSymbols.str(), discardSections.str());

  return script;
}

/// Removes all local symbols from the object file, except for undefined
/// linker symbols and immutables from the first file.
/// This is done using the 'objcopy' API.
static LLVMMemoryBufferRef removeLocalSymbols(LLVMMemoryBufferRef inBuffer,
                                              StringSet<> firstFileImmutables) {
  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  auto *oFile = static_cast<ObjectFile *>(inBinary.get());

  StringSet<> undefReferenceSymbols;
  for (const SymbolRef &sym : oFile->symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other == ELF::STO_EVM_REFERENCE_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined)) {
      undefReferenceSymbols.insert(cantFail(sym.getName()));
    }
  }

  auto errCallback = [](Error E) {
    report_fatal_error(StringRef(toString(std::move(E))));
    return Error::success();
  };

  // Create an 'objcopy' configuration that removes all local
  // symbols while explicitly retaining the necessary ones.
  ConfigManager Config;
  Config.Common.DiscardMode = DiscardType::All;
  for (const StringSet<>::value_type &entry :
       llvm::concat<StringSet<>::value_type>(undefReferenceSymbols,
                                             firstFileImmutables)) {
    if (Error E = Config.Common.SymbolsToKeep.addMatcher(NameOrPattern::create(
            entry.first(), MatchStyle::Literal, errCallback)))
      report_fatal_error(StringRef(toString(std::move(E))));
  }

  SmallString<0> dataVector;
  raw_svector_ostream outStream(dataVector);

  if (Error Err = objcopy::executeObjcopyOnBinary(Config, *oFile, outStream))
    report_fatal_error(StringRef(toString(std::move(Err))));

  MemoryBufferRef buffer(StringRef(dataVector.data(), dataVector.size()), "");
  Expected<std::unique_ptr<Binary>> result = createBinary(buffer);

  // Check the copied file.
  if (!result)
    report_fatal_error(StringRef(toString(result.takeError())));

  StringRef data = outStream.str();
  StringRef bufName = unwrap(inBuffer)->getBufferIdentifier();
  return LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(), data.size(),
                                                   bufName.str().c_str());
}

/// Checks if the ELF file contains any undefined symbols, aside from
/// those used to represent library addresses.
static void getUndefinedNonRefSymbols(LLVMMemoryBufferRef inBuffer,
                                      SmallVectorImpl<StringRef> &undefSyms) {
  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  auto *oFile = static_cast<ObjectFile *>(inBinary.get());

  for (const SymbolRef &sym : oFile->symbols()) {
    uint32_t symFlags = cantFail(sym.getFlags());
    uint8_t other = ELFSymbolRef(sym).getOther();
    if ((other != ELF::STO_EVM_REFERENCE_SYMBOL) &&
        (symFlags & object::SymbolRef::SF_Undefined))
      undefSyms.push_back(cantFail(sym.getName()));
  }
}

/// Returns an array of offsets for the linker symbol relocations.
/// These are the symbol offsets in the generated bytecode.
uint64_t LLVMGetSymbolOffsetsEVM(LLVMMemoryBufferRef inBuffer,
                                 const char *symbolName,
                                 uint64_t **symbolOffsets) {
  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(unwrap(inBuffer)->getMemBufferRef()));
  auto *elfFile = static_cast<ELFObjectFileBase *>(inBinary.get());

  SmallVector<uint64_t> Offsets;
  // The relocation of a linker symbol is expressed as a sequence of
  // five consecutive 32-bit relocations. We need only the first one.
  std::string subSymName = getLinkerSubSymbolName(symbolName, 0);
  for (const ELFSectionRef relocsSec : elfFile->sections()) {
    // Find the relocation section. It should be .rela.text.
    if (relocsSec.relocations().empty() ||
        !cantFail(relocsSec.getRelocatedSection())->isText())
      continue;

    // Check relocations of the specified type.
    for (const ELFRelocationRef rel : relocsSec.relocations()) {
      if (rel.getType() != ELF::R_EVM_DATA)
        continue;

      elf_symbol_iterator sym = rel.getSymbol();
      uint32_t symFlags = cantFail(sym->getFlags());
      StringRef symName = cantFail(sym->getName());
      if ((sym->getOther() == llvm::ELF::STO_EVM_REFERENCE_SYMBOL) &&
          (symFlags & object::SymbolRef::SF_Undefined) &&
          (symName == subSymName))
        Offsets.push_back(rel.getOffset());
    }
  }

  *symbolOffsets = reinterpret_cast<uint64_t *>(
      std::malloc(Offsets.size() * sizeof(uint64_t)));

  sort(Offsets);
  copy(Offsets, *symbolOffsets);

  return Offsets.size();
}

/// Performs the following steps, which are based on Ethereum's
/// Assembly::assemble() logic:
///  - Concatenates the .text sections of input ELF files referenced
///    by __dataoffset* symbols from the first file.
///  - Resolves undefined __dataoffset* and __datasize* symbols.
///  - Gathers all undefined linker symbols (library references) from
///    all files.
///  - Ensures that the first file does not load and set
///    immutables simultaneously.
///
/// \p codeSegment, 0 means the first file has a deploy code,
///                 1 - runtime code;
/// \p inBuffers - relocatable ELF files to be assembled
/// \p inBuffersIDs - their IDs
/// \p outBuffer - resulting relocatable object file
LLVMBool LLVMAssembleEVM(uint64_t codeSegment,
                         const LLVMMemoryBufferRef inBuffers[],
                         const char *const inBuffersIDs[],
                         uint64_t inBuffersNum, LLVMMemoryBufferRef *outBuffer,
                         char **errorMessage) {
  SmallVector<MemoryBufferRef> localInMemBufRefs(inBuffersNum + 1);
  SmallVector<std::unique_ptr<MemoryBuffer>> localInMemBufs(inBuffersNum + 1);

  // TODO: #740. Verify that the object files contain sections with the original
  // inBuffersIDs, i.e. before the hash is applied.
  for (unsigned idx = 0; idx < inBuffersNum; ++idx) {
    MemoryBufferRef ref = *unwrap(inBuffers[idx]);
    // We need to copy the buffers to change their names, as the linking
    // process depends on them.
    localInMemBufs[idx] = MemoryBuffer::getMemBufferCopy(
        ref.getBuffer(), EVM::getLinkerSymbolHash(inBuffersIDs[idx]));
    localInMemBufRefs[idx] = localInMemBufs[idx]->getMemBufferRef();
  }

  std::unique_ptr<Binary> firstBinary =
      cantFail(createBinary(localInMemBufRefs[0]));
  const auto *firstObjFile = static_cast<const ObjectFile *>(firstBinary.get());

  // Retrieve the object names referenced by the first file.
  // Retrieve the immutable symbols from the first file.
  StringSet<> firstLoadImmutables;
  StringSet<> firstDataOffsetRefs;
  for (const SymbolRef &sym : firstObjFile->symbols()) {
    StringRef symName = cantFail(sym.getName());
    if (EVM::isLoadImmutableSymbolName(symName))
      firstLoadImmutables.insert(symName);
    else if (EVM::isDataOffsetSymbolName(symName)) {
      std::string objName = EVM::extractDataOffseteName(symName);
      firstDataOffsetRefs.insert(objName);
    }
  }

  // Check if the first (idx == 1) dependency file contains loadimmutable
  // symbols.
  // A 'codeSegment' value of 0 indicates that the first file contains a
  // deploy code, which implies that the first dependency is the corresponding
  // runtime code.
  bool depHasLoadImmutable = false;
  if (codeSegment == 0 && inBuffersNum > 1) {
    std::unique_ptr<Binary> binary =
        cantFail(createBinary(localInMemBufRefs[1]));
    const auto *oFile = static_cast<const ObjectFile *>(binary.get());

    for (const SymbolRef &sym : oFile->symbols()) {
      section_iterator symSec = cantFail(sym.getSection());
      if (symSec == oFile->section_end())
        continue;

      StringRef symName = cantFail(sym.getName());
      if (EVM::isLoadImmutableSymbolName(symName))
        depHasLoadImmutable = true;
    }
  }

  if (firstLoadImmutables.size() > 0 && depHasLoadImmutable)
    report_fatal_error("lld: assembly both sets up and loads immutables");

  std::string linkerScript = createEVMAssembeScript(
      ArrayRef(inBuffers, inBuffersNum), ArrayRef(inBuffersIDs, inBuffersNum),
      firstDataOffsetRefs);

  std::unique_ptr<MemoryBuffer> scriptBuf =
      MemoryBuffer::getMemBuffer(linkerScript, "script.x");
  localInMemBufRefs[inBuffersNum] = scriptBuf->getMemBufferRef();

  SmallVector<const char *, 16> lldArgs;
  lldArgs.push_back("ld.lld");

  // Use file name remapping (a linker feature) to replace file names with
  // indexes in the array of memory buffers.
  const std::string remapStr("--remap-inputs=");
  SmallVector<std::string> args;
  for (unsigned idx = 0; idx < inBuffersNum; ++idx) {
    std::string idHash = EVM::getLinkerSymbolHash(inBuffersIDs[idx]);
    if (idx > 0 && !firstDataOffsetRefs.count(idHash))
      continue;

    args.emplace_back(remapStr + idHash + "=" + std::to_string(idx));
    args.emplace_back(std::to_string(idx));
  }

  args.emplace_back("-T");
  args.emplace_back(std::to_string(inBuffersNum));

  for (const std::string &arg : args)
    lldArgs.push_back(arg.c_str());

  lldArgs.push_back("--evm-assembly");

  SmallString<0> codeString;
  raw_svector_ostream ostream(codeString);
  SmallString<0> errorString;
  raw_svector_ostream errorOstream(errorString);

  // Lld-as-a-library is not thread-safe due to its global state,
  // so we need to protect it from concurrent access by multiple threads.
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
  LLVMMemoryBufferRef Tmp = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data(), data.size(),
      EVM::getLinkerSymbolHash(inBuffersIDs[0]).c_str());

  SmallVector<StringRef, 16> undefSyms;
  getUndefinedNonRefSymbols(Tmp, undefSyms);
  if (!undefSyms.empty()) {
    std::string storage;
    raw_string_ostream errMsg(storage);
    for (StringRef name : undefSyms)
      errMsg << "non-ref undefined symbol: " << name << '\n';

    *errorMessage = strdup(errMsg.str().c_str());
    return true;
  }

  *outBuffer = removeLocalSymbols(Tmp, firstLoadImmutables);
  LLVMDisposeMemoryBuffer(Tmp);

  return false;
}

/// Returns the immutable symbol names and their offsets from the ELF
/// object file provided in \p inBuffer.
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

/// Disposes of the immutable names and their offsets returned by
/// 'LLVMGetImmutablesEVM'.
void LLVMDisposeImmutablesEVM(char **immutableIDs, uint64_t *immutableOffsets,
                              uint64_t numOfImmutables) {
  for (unsigned idx = 0; idx < numOfImmutables; ++idx)
    std::free(immutableIDs[idx]);

  std::free(immutableIDs);
  std::free(immutableOffsets);
}

void LLVMDisposeSymbolOffsetsEVM(uint64_t *offsets) { std::free(offsets); }
