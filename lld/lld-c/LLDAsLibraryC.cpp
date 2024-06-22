#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"

LLD_HAS_DRIVER_MEM_BUF(elf)

static std::string creteEraVMLinkerScript() {
  llvm::Twine script("                   \
MEMORY {                                 \
  code (rx)   : ORIGIN = 0,  LENGTH = 4K \
  stack (w!x) : ORIGIN = 4K, LENGTH = 4K \
}                                        \
                                         \
SECTIONS {                               \
  .code : {                              \
    *(.rodata)                           \
    *(.text)                             \
  } > code                               \
  .stack : {                             \
    *(.data)                             \
  } > stack                              \
}");

  return script.str();
}

LLVMBool LLVMLinkEraVM(LLVMMemoryBufferRef inMemBufs,
                       LLVMMemoryBufferRef *outMemBuf) {
  llvm::SmallVector<llvm::MemoryBufferRef> localInMemBufRefs(2);
  localInMemBufRefs[0] = *llvm::unwrap(inMemBufs);

  std::string linkerScript = creteEraVMLinkerScript();

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
  const lld::Result s =
      lld::lldMainMemBuf(localInMemBufRefs, &ostream, lldArgs, llvm::outs(),
                         llvm::errs(), {{lld::Gnu, &lld::elf::linkMemBuf}});

  bool Ret = !s.retCode && s.canRunAgain;
  if (!Ret)
    return Ret;

  llvm::StringRef data = ostream.str();

  *outMemBuf = LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(),
                                                         data.size(), "result");

  return Ret;
}
