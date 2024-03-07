#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/Support/MemoryBuffer.h"

LLD_HAS_DRIVER_MEM_BUF(elf)

LLVMBool llvmLinkMemoryBuffers(LLVMMemoryBufferRef *inMemBufs, size_t numInBufs,
                               LLVMMemoryBufferRef *outMemBuf,
                               const char **lldArgs, size_t numLldArgs) {
  llvm::SmallVector<llvm::MemoryBufferRef> inData(numInBufs);
  for (unsigned idx = 0; idx < numInBufs; ++idx)
    inData[idx] = *llvm::unwrap(inMemBufs[idx]);

  llvm::SmallVector<const char *> args(numLldArgs);
  for (unsigned idx = 0; idx < numLldArgs; ++idx)
    args[idx] = lldArgs[idx];

  llvm::SmallString<0> codeString;
  llvm::raw_svector_ostream ostream(codeString);
  const lld::Result s =
      lld::lldMainMemBuf(inData, &ostream, args, llvm::outs(), llvm::errs(),
                         {{lld::Gnu, &lld::elf::linkMemBuf}});
  llvm::StringRef data = ostream.str();
  *outMemBuf =
      LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(), data.size(), "");

  return !s.retCode && s.canRunAgain;
}
