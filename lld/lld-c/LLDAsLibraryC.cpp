#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"

#include <iostream>
LLD_HAS_DRIVER_MEM_BUF(elf)

LLVMBool LLVMLinkMemoryBuffers(LLVMMemoryBufferRef *inMemBufs, size_t numInBufs,
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

LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef inMemBuf,
                     LLVMMemoryBufferRef *outMemBuf) {
  llvm::SmallVector<llvm::MemoryBufferRef> localInMemBufRefs(1);
  localInMemBufRefs[0] = *llvm::unwrap(inMemBuf);

  llvm::SmallVector<const char *, 8> lldArgs;
  lldArgs.push_back("ld.lld");

  lldArgs.push_back("0");

  lldArgs.push_back("--oformat=binary");

  llvm::SmallString<0> codeString;
  llvm::raw_svector_ostream ostream(codeString);
  const lld::Result s =
      lld::lldMainMemBuf(localInMemBufRefs, &ostream, lldArgs, llvm::outs(),
                         llvm::errs(), {{lld::Gnu, &lld::elf::linkMemBuf}});
  bool isOK = !s.retCode && s.canRunAgain;
  if (!isOK)
    return isOK;

  llvm::StringRef data = ostream.str();
  *outMemBuf = LLVMCreateMemoryBufferWithMemoryRangeCopy(data.data(),
                                                         data.size(), "result");
  return isOK;
}
