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

static std::mutex lldMutex;

/// This function generates a linker script for EVM architecture.
/// \p memBufs - array of input memory buffers with following structure:
///
///   memBufs[0] - deploy object code
///   memBufs[1] - deployed object code
///   --------------------------
///   memBufs[2] - 1-st sub-contract (final EVM bytecode)
///   ...
///   memBufs[N] - N-st sub-contract (final EVM bytecode)
///
/// Sub-contracts are optional. They should have the same ordering as in
/// the YUL layout.
///
/// \p bufIDs - array of string identifiers of the buffers. IDs correspond
/// to the object names in the YUL layout.
///
/// For example, the YUL object:
///
///   |--D_105_deploy --||--D_105_deployed --||-- B_40 --|
///
///   __datasize_B_40 = 1384;
///   SECTIONS {
///     . = 0;
///     .text : SUBALIGN(1) {
///       D_105(.text);
///       __dataoffset_D_105_deployed = .;
///       D_105_deployed(.text);
///       __datasize_D_105_deployed = . - __dataoffset_D_105_deployed;
///       __dataoffset_B_40 = .;
///       __datasize_D_105 = __dataoffset_B_40 + __datasize_B_40;
///       LONG(__dataoffset_D_105_deployed);
///     }
///
/// The dot '.' denotes current location in the resulting file.
/// The purpose of the script is to define datasize/dataoffset absolute symbols
/// that reflect the YUL layout.
static std::string creteEVMLinkerScript(ArrayRef<LLVMMemoryBufferRef> memBufs,
                                        ArrayRef<const char *> bufIDs) {
  assert(memBufs.size() == bufIDs.size());
  size_t numObjectsToLink = memBufs.size();
  StringRef dataSizePrefix("__datasize_");
  StringRef dataOffsetPrefix("__dataoffset_");

  // Define the script part related to the top-level contract.
  StringRef topName(bufIDs[0]);
  StringRef deployed(bufIDs[1]);

  // Contains the linker script part corresponding to the top-level contract.
  // For the example above, this contains:
  //   D_105(.text);
  //   __dataoffset_D_105_deployed = .;
  //   D_105_deployed(.text);
  //   __datasize_D_105_deployed = . - __dataoffset_D_105_deployed;
  std::string topLevel =
      (topName + "(.text);\n" + dataOffsetPrefix + deployed + " = .;\n" +
       deployed + "(.text);\n" + dataSizePrefix + deployed + " = . - " +
       dataOffsetPrefix + deployed + ";\n")
          .str();

  // Contains symbols whose values are the sizes of the dependent contracts.
  // For the example above, this contains:
  //   __datasize_B_40 = 1384;
  std::string symDatasizeDeps;

  // Contains symbols whose values are the offsets of the dependent contracts.
  // For the example above, this contains:
  //   __dataoffset_B_40 = .;
  std::string symDataOffsetDeps;
  if (numObjectsToLink > 2) {
    // Define datasize symbols for the dependent contracts. They start after
    // {deploy, deployed} pair of the top-level contract, i.e. at index 2.
    for (unsigned idx = 2; idx < numObjectsToLink; ++idx)
      symDatasizeDeps += (dataSizePrefix + bufIDs[idx] + " = " +
                          Twine(LLVMGetBufferSize(memBufs[idx])) + ";\n")
                             .str();

    symDataOffsetDeps = (dataOffsetPrefix + bufIDs[2] + " = .;\n").str();
    for (unsigned idx = 3; idx < numObjectsToLink; ++idx)
      symDataOffsetDeps +=
          (dataOffsetPrefix + bufIDs[idx] + " = " + dataOffsetPrefix +
           bufIDs[idx - 1] + " + " + dataSizePrefix + bufIDs[idx - 1] + ";\n")
              .str();
  }

  // Contains a symbol whose value is the total size of the top-level contract
  // with all the dependencies.
  std::string symDatasizeTop = (dataSizePrefix + topName + " = ").str();
  if (numObjectsToLink > 2)
    symDatasizeTop += (dataOffsetPrefix + bufIDs.back() + " + " +
                       dataSizePrefix + bufIDs.back() + ";\n")
                          .str();
  else
    symDatasizeTop += ".;\n";

  // Emit size of the deploy code offset as the 4-byte unsigned integer.
  // This is needed to determine which offset the deployed code starts at
  // in the linked binary.
  std::string deploySize =
      ("LONG(" + dataOffsetPrefix + deployed + ");\n").str();

  std::string script = formatv("{0}\n\
ENTRY(0);\n\
SECTIONS {\n\
  . = 0;\n\
  .code : SUBALIGN(1) {\n\
{1}\
{2}\
{3}\
{4}\
  }\n\
}\n\
",
                               symDatasizeDeps, topLevel, symDataOffsetDeps,
                               symDatasizeTop, deploySize);

  return script;
}

LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef inBuffers[],
                     const char *inBuffersIDs[], uint64_t numInBuffers,
                     LLVMMemoryBufferRef outBuffers[2], char **errorMessage) {
  assert(numInBuffers > 1);
  SmallVector<MemoryBufferRef> localInMemBufRefs(3);
  SmallVector<std::unique_ptr<MemoryBuffer>> localInMemBufs(3);
  for (unsigned idx = 0; idx < 2; ++idx) {
    MemoryBufferRef ref = *unwrap(inBuffers[idx]);
    localInMemBufs[idx] =
        MemoryBuffer::getMemBuffer(ref.getBuffer(), inBuffersIDs[idx],
                                   /*RequiresNullTerminator*/ false);
    localInMemBufRefs[idx] = localInMemBufs[idx]->getMemBufferRef();
  }

  std::string linkerScript = creteEVMLinkerScript(
      ArrayRef(inBuffers, numInBuffers), ArrayRef(inBuffersIDs, numInBuffers));
  std::unique_ptr<MemoryBuffer> scriptBuf =
      MemoryBuffer::getMemBuffer(linkerScript, "script.x");
  localInMemBufRefs[2] = scriptBuf->getMemBufferRef();

  SmallVector<const char *, 16> lldArgs;
  lldArgs.push_back("ld.lld");
  lldArgs.push_back("-T");
  lldArgs.push_back("script.x");

  // Use remapping of file names (a linker feature) to replace file names with
  // indexes in the array of memory buffers.
  const std::string remapStr("--remap-inputs=");
  std::string remapDeployStr = remapStr + inBuffersIDs[0] + "=0";
  lldArgs.push_back(remapDeployStr.c_str());

  std::string remapDeployedStr = remapStr + inBuffersIDs[1] + "=1";
  lldArgs.push_back(remapDeployedStr.c_str());

  lldArgs.push_back("--remap-inputs=script.x=2");

  // Deploy code
  lldArgs.push_back(inBuffersIDs[0]);
  // Deployed code
  lldArgs.push_back(inBuffersIDs[1]);

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
  // Linker script adds size of the deploy code as a 8-byte BE unsigned to the
  // end of .text section. Knowing this, we can extract final deploy and
  // deployed codes.
  assert(data.size() > 4);
  size_t deploySize = support::endian::read32be(data.data() + data.size() - 4);
  assert(deploySize < data.size());
  size_t deployedSize = data.size() - deploySize - 4;

  outBuffers[0] = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data(), deploySize, "deploy");
  outBuffers[1] = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data() + deploySize, deployedSize, "deployed");

  return false;
}
