#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string.h>
#include <string>

using namespace llvm;
using namespace object;

LLD_HAS_DRIVER_MEM_BUF(elf)

static uint64_t getSectionSize(ObjectFile &file, StringRef sectionName) {
  for (const object::SectionRef &sec : file.sections()) {
    StringRef curSecName = cantFail(sec.getName());
    if (curSecName == sectionName)
      return sec.getSize();
  }
  return 0;
}

static std::string creteEraVMLinkerScript(uint64_t metadataSize) {
  // The final bytecode should be padded such that its size be the
  // odd number of words, i.e 2 * (N + 1).
  // Add padding before the metadata section such that the final
  // bytecode size to be the even number of words. The metadata
  // may have arbitrary size.
  uint64_t alignedMDSize = llvm::alignTo(metadataSize, llvm::Align(32), 0);
  // Create the resulting padding expression which is
  //  . = ((((. + alignedMDSize) >> 5) | 1 ) << 5) - metadataSize - .
  std::string padding =
      llvm::formatv(". = ((((. + {0}) >> 5) | 1 ) << 5) - {1};\n",
                    std::to_string(alignedMDSize), std::to_string(metadataSize))
          .str();

  llvm::Twine scriptPart1 = llvm::Twine("\
SECTIONS {                                                                  \n\
  .code : SUBALIGN(8) {                                                     \n\
    *(.text)                                                                \n\
                                                                            \n\
    ASSERT((32 - (31 & .)) % 8 == 0, \"padding isn't multiple of 8\");      \n\
                                                                            \n\
    /* Check the code size is no more than 2^16 instructions. */            \n\
    ASSERT(. <= (1 << 16) * 8, \"number of instructions > 2^16\")           \n\
                                                                            \n\
    /* Align the .rodata to 32 bytes. */                                    \n\
    . = ALIGN(32);                                                          \n\
    *(.rodata)                                                              \n\
                                                                            \n\
    ASSERT(. % 32 == 0, \"size isn't multiple of 32\");                     \n\
                                                                            \n\
    /* Add padding */\n");

  llvm::Twine scriptPart2 = llvm::Twine("\
    *(.eravm-metadata)                                                      \n\
                                                                            \n\
    ASSERT(. % 64 == 32, \"size isn't odd number of words\");               \n\
                                                                            \n\
    /* Check the total binary size is not more than (2^16 - 2) words. */    \n\
    ASSERT(. <= ((1 << 16) - 1) * 32, \"Binary size > (2^16 - 2) words\")   \n\
  } = 0                                                                     \n\
                                                                            \n\
  /* .data section itself that contains initializers of global variables,   \n\
     is not needed. */                                                      \n\
  /DISCARD/ : {                                                             \n\
    *(.data)                                                                \n\
  }}");

  return (scriptPart1 + padding + scriptPart2).str();
}

LLVMBool LLVMLinkEraVM(LLVMMemoryBufferRef inBuffer,
                       LLVMMemoryBufferRef *outBuffer, char **errorMessage) {
  llvm::SmallVector<llvm::MemoryBufferRef> localInMemBufRefs(2);
  localInMemBufRefs[0] = *llvm::unwrap(inBuffer);

  std::unique_ptr<Binary> inBinary =
      cantFail(createBinary(llvm::unwrap(inBuffer)->getMemBufferRef()));
  assert(inBinary->isObject());
  uint64_t mdSize = getSectionSize(*static_cast<ObjectFile *>(inBinary.get()),
                                   ".eravm-metadata");
  std::string linkerScript = creteEraVMLinkerScript(mdSize);

  std::unique_ptr<llvm::MemoryBuffer> linkerScriptBuf =
      llvm::MemoryBuffer::getMemBuffer(linkerScript, "1");

  localInMemBufRefs[1] = linkerScriptBuf->getMemBufferRef();

  llvm::SmallVector<const char *, 16> lldArgs;
  lldArgs.push_back("ld.lld");

  // Push the name of the linker script - '1'.
  lldArgs.push_back("-T");
  lldArgs.push_back("1");

  // Push the name of the input object file - '0'.
  lldArgs.push_back("0");

  // Strip out the ELF format.
  lldArgs.push_back("--oformat=binary");

  llvm::SmallString<0> codeString;
  llvm::raw_svector_ostream ostream(codeString);
  llvm::SmallString<0> errorString;
  llvm::raw_svector_ostream errorOstream(errorString);
  const lld::Result s =
      lld::lldMainMemBuf(localInMemBufRefs, &ostream, lldArgs, llvm::outs(),
                         errorOstream, {{lld::Gnu, &lld::elf::linkMemBuf}});

  bool Ret = !s.retCode && s.canRunAgain;
  // For unification with other LLVM C-API functions, return 'true' in case of
  // an error.
  if (!Ret) {
    *errorMessage = strdup(errorString.c_str());
    return true;
  }

  llvm::StringRef data = ostream.str();
  *outBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(),
                                                         data.size(), "result");

  return false;
}
