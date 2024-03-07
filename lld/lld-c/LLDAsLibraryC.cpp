#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MemoryBuffer.h"

LLD_HAS_DRIVER_MEM_BUF(elf)

static std::string creteEraVMLinkerScript(const char *hashBuf) {
  // The final bytecode should be padded such that its size be the
  // odd number of words, i.e 2 * (N + 1).
  std::string hash;
  std::string padding;
  if (hashBuf) {
    // Hash is the 32-byte array that will be append to the output section
    // using eight LONG() commands.
    for (unsigned idx = 0; idx < 8; ++idx) {
      // hashBuf has bytes in the 'right' order, but LONG(value) command
      // stores the value in the endianness specified in the ELF header.
      // EraVM is LE, which leads to swapped bytes in the object code.
      // To overcome this we read bytes from the buffer as if they were
      // in LE order.
      uint32_t val = llvm::support::endian::read32le(hashBuf + idx * 4);
      hash += (llvm::Twine("LONG(0x") + llvm::utohexstr(val, true, 8) + ");\n")
                  .str();
    }

    // Add padding such that the current size would be the even number of words.
    // The last word will be the hash.
    padding = ". = (. + 63) & ~63;";
  } else {
    // No hash, but anyway the final size should be the odd number of words.
    // If the size is already an odd number, we are done. Otherwise add
    // one zeroed word.
    padding = ". = ((. >> 5) | 1) << 5;\n";
  }

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
    . = (. + 31) & ~31;                                                     \n\
    *(.rodata)                                                              \n\
                                                                            \n\
    ASSERT(. % 32 == 0, \"size isn't multiple of 32\");                     \n\
                                                                            \n\
    /* Add padding and hash, such that the size be odd number of words */\n");

  llvm::Twine scriptPart2 = llvm::Twine("\
    ASSERT(. % 64 == 32, \"size isn't odd number of words\");               \n\
                                                                            \n\
    /* Check the total binary size is no more than (2^16 - 2) words. */     \n\
    ASSERT(. <= ((1 << 16) - 1) * 32, \"Binary size > (2^16 - 2) words\")   \n\
  } = 0                                                                     \n\
                                                                            \n\
  /* .data section itself that contains initializers of global variables,   \n\
     is not needed. */                                                      \n\
  /DISCARD/ : {                                                             \n\
    *(.data)                                                                \n\
  }}");

  return (scriptPart1 + padding + "\n" + hash + "\n" + scriptPart2).str();
}

LLVMBool LLVMLinkEraVM(LLVMMemoryBufferRef inBuffer,
                       LLVMMemoryBufferRef *outBuffer, const char *hash,
                       char **ErrorMessage) {
  llvm::SmallVector<llvm::MemoryBufferRef> localInMemBufRefs(2);
  localInMemBufRefs[0] = *llvm::unwrap(inBuffer);

  std::string linkerScript = creteEraVMLinkerScript(hash);

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
    *ErrorMessage = strdup(errorString.c_str());
    return true;
  }

  llvm::StringRef data = ostream.str();
  *outBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(),
                                                         data.size(), "result");

  return false;
}
