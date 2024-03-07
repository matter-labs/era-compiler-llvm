#include "lld-c/LLDAsLibraryC.h"
#include "lld/Common/Driver.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm-c/Core.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"

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

/// This function generates a linker script for EVM architecture.
/// \p memBufs - array of input memory buffers with following structure:
///
///   memBufs[0] - deploy object code
///   memBufs[1] - deplyed object code
///   --------------------------
///   memBufs[2] - 1-st sub-contract (final EVM bytecode)
///   ...
///   memBufs[N] - N-st sub-contract (final EVM bytecode)
///
/// Sub contracts are optional. They should have the same ordering as in
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
///       __dataoffset_D_105 = .;
///       D_105_deploy(.text);
///       __dataoffset_D_105_deployed = .;
///       D_105_deployed(.text);
///       __datasize_D_105_deployed = . - __dataoffset_D_105_deployed;
///       __dataoffset_B_40 = .;
///       __datasize_D_105 = __dataoffset_B_40 + __datasize_B_40;
///       QUAD(__dataoffset_D_105_deployed);
///     }
///
/// The dot '.' denotes current location in the resulting file.
/// The purpose of the script is to define datasize/dataoffset absolute symbols
/// that reflect the YUL layour.
static std::string
creteEVMLinkerScript(llvm::ArrayRef<LLVMMemoryBufferRef> memBufs,
                     llvm::ArrayRef<const char *> bufIDs) {
  assert(memBufs.size() == bufIDs.size());
  llvm::Twine dzPrefix("__datasize_");
  llvm::Twine dfPrefix("__dataoffset_");

  // Define the script part related to top-level contract.
  llvm::StringRef deploy(bufIDs[0]);
  llvm::StringRef deployed(bufIDs[1]);

  size_t pos = deploy.find("_deploy");
  assert(pos != llvm::StringRef::npos);
  llvm::StringRef topName = deploy.take_front(pos);
  assert(topName == deployed.take_front(pos));

  llvm::Twine topLevel = dfPrefix + topName + " = .;\n" + deploy +
                         "(.text);\n" + dfPrefix + deployed + " = .;\n" +
                         deployed + "(.text);\n" + dzPrefix + deployed +
                         " = . - " + dfPrefix + deployed + ";\n";

  std::string symDatasizeDeps;
  std::string symDataOffsetDeps;
  if (memBufs.size() > 2) {
    // Define datasize symbols for the dependent contracts. They start after
    // {deploy, deployed} pair of the top-level contract,.i.e. at index 2.
    for (unsigned idx = 2; idx < bufIDs.size(); ++idx)
      symDatasizeDeps += (dzPrefix + bufIDs[idx] + " = " +
                          llvm::Twine(LLVMGetBufferSize(memBufs[idx])) + ";\n")
                             .str();

    // Define dataoffset symbols for the dependent contracts.
    symDataOffsetDeps = (dfPrefix + bufIDs[2] + " = .;\n").str();
    for (unsigned idx = 3; idx < bufIDs.size(); ++idx) {
      symDataOffsetDeps +=
          (dfPrefix + bufIDs[idx] + " = " + dfPrefix + bufIDs[idx - 1] + " + " +
           dzPrefix + bufIDs[idx - 1] + ";\n")
              .str();
    }
  }

  // Define datasize symbol for top-level contract.
  std::string symDatasizeTop = (dzPrefix + topName + " = ").str();
  if (memBufs.size() > 2)
    symDatasizeTop +=
        (dfPrefix + bufIDs.back() + " + " + dzPrefix + bufIDs.back() + ";\n")
            .str();
  else
    symDatasizeTop += ".;\n";

  // Emit size of the deploy code as an 8-byte usingned integer.
  llvm::Twine deploySize = "QUAD(" + dfPrefix + deployed + ");\n";

  llvm::Twine script = llvm::formatv(
      "{0}\n\
SECTIONS {\n\
  . = 0;\n\
  .text : SUBALIGN(1) {\n\
{1}\
{2}\
{3}\
{4}\
  }\n\
}\n\
",
      symDatasizeDeps, topLevel, symDataOffsetDeps, symDatasizeTop, deploySize);

  return script.str();
}

LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef inMemBufs[], const char *inMemBufIDs[],
                     size_t numInBufs, LLVMMemoryBufferRef outMemBuf[2]) {
  assert(numInBufs > 1);
  // Define local copies of the IN buffers. FIXME: we can avoid this and also
  // get rig of inMemBufIDs, but this requires that a proper name is set up in
  // each memory buffer.
  llvm::SmallVector<llvm::MemoryBufferRef> localInMemBufRefs(3);
  llvm::SmallVector<std::unique_ptr<llvm::MemoryBuffer>> localInMemBufs(3);
  for (unsigned idx = 0; idx < 2; ++idx) {
    llvm::MemoryBufferRef Ref = *llvm::unwrap(inMemBufs[idx]);
    localInMemBufs[idx] =
        llvm::MemoryBuffer::getMemBuffer(Ref.getBuffer(), inMemBufIDs[idx],
                                         /*RequiresNullTerminator*/ false);
    localInMemBufRefs[idx] = localInMemBufs[idx]->getMemBufferRef();
  }

  std::string linkerScript =
      creteEVMLinkerScript(llvm::ArrayRef(inMemBufs, numInBufs),
                           llvm::ArrayRef(inMemBufIDs, numInBufs));
  std::unique_ptr<llvm::MemoryBuffer> ScriptBuf =
      llvm::MemoryBuffer::getMemBuffer(linkerScript, "script.x");
  localInMemBufRefs[2] = ScriptBuf->getMemBufferRef();

  llvm::SmallVector<const char *, 16> lldArgs;
  lldArgs.push_back("ld.lld");

  lldArgs.push_back("-T");
  lldArgs.push_back("script.x");

  // Use remapping of file names (a linker feature) to replace file names with
  // indexes in the array of memory buffers.
  llvm::Twine remapStr("--remap-inputs=");
  std::string remapDeployStr = (remapStr + inMemBufIDs[0] + "=0").str();
  lldArgs.push_back(remapDeployStr.c_str());

  std::string remapDeployedStr = (remapStr + inMemBufIDs[1] + "=1").str();
  lldArgs.push_back(remapDeployedStr.c_str());

  lldArgs.push_back("--remap-inputs=script.x=2");

  // Deploy code
  lldArgs.push_back(inMemBufIDs[0]);
  // Deployed code
  lldArgs.push_back(inMemBufIDs[1]);

  lldArgs.push_back("--oformat=binary");

  llvm::SmallString<0> codeString;
  llvm::raw_svector_ostream ostream(codeString);
  const lld::Result s =
      lld::lldMainMemBuf(localInMemBufRefs, &ostream, lldArgs, llvm::outs(),
                         llvm::errs(), {{lld::Gnu, &lld::elf::linkMemBuf}});
  llvm::StringRef data = ostream.str();
  // Linker script adds size of the deploy code as a 8-byte BE unsigned to the
  // end of .text section. Knowing this, we can extract final deploy and
  // deployed codes.
  assert(data.size() > 8);
  size_t deploySize =
      llvm::support::endian::read64be(data.data() + data.size() - 8);
  assert(deploySize < data.size());
  size_t deployedSize = data.size() - deploySize - 8;

  outMemBuf[0] = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data(), deploySize, "deploy");
  outMemBuf[1] = LLVMCreateMemoryBufferWithMemoryRangeCopy(
      data.data() + deploySize, deployedSize, "deployed");

  return !s.retCode && s.canRunAgain;
}
