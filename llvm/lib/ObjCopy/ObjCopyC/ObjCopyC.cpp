//===-------- ObjCopyC.cpp - ObjCopy Public C Interface ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the C interface for the llv-objcopy functionality.
//
//===----------------------------------------------------------------------===//

#include "llvm-c/Core.h"
#include "llvm-c/ObjCopy.h"
#include "llvm-c/Object.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ObjCopy/ConfigManager.h"
#include "llvm/ObjCopy/ObjCopy.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"

#include <cstdint>
#include <string>

using namespace llvm;
using namespace object;
using namespace objcopy;

#ifndef NDEBUG
static void checkSectionData(ObjectFile &File, StringRef SectionName,
                             StringRef SectionData) {
  for (const object::SectionRef &Sec : File.sections()) {
    StringRef CurSecName = cantFail(Sec.getName());
    if (CurSecName == SectionName) {
      StringRef CurSecData = cantFail(Sec.getContents());
      assert(Sec.getSize() == SectionData.size());
      assert(memcmp(CurSecData.data(), SectionData.data(),
                    SectionData.size()) == 0);
    }
  }
}
#endif // NDEBUG

LLVMBool LLVMAddMetadataEraVM(LLVMMemoryBufferRef InBuffer,
                              const char *MetadataPtr, uint64_t MetadataSize,
                              LLVMMemoryBufferRef *OutBuffer,
                              char **ErrorMessage) {
  if (!MetadataSize) {
    *OutBuffer = nullptr;
    return false;
  }

  StringRef MDSectionName = ".eravm-metadata";

  std::unique_ptr<MemoryBuffer> MDSectionBuffer = MemoryBuffer::getMemBuffer(
      StringRef(MetadataPtr, MetadataSize), MDSectionName, false);

  Expected<std::unique_ptr<Binary>> InObjOrErr(
      createBinary(unwrap(InBuffer)->getMemBufferRef()));
  if (!InObjOrErr)
    llvm_unreachable("Cannot create Binary object from the memory buffer");

  ConfigManager Config;
  Config.Common.AddSection.emplace_back(MDSectionName,
                                        std::move(MDSectionBuffer));

  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  SectionFlagsUpdate SFU = {MDSectionName, SecReadonly | SecNoload};
  [[maybe_unused]] auto It =
      Config.Common.SetSectionFlags.try_emplace(SFU.Name, SFU);
  assert(It.second);

  SmallString<0> BufferString;
  raw_svector_ostream OutStream(BufferString);
  if (Error Err = objcopy::executeObjcopyOnBinary(Config, *InObjOrErr.get(),
                                                  OutStream)) {
    *ErrorMessage = strdup(toString(std::move(Err)).c_str());
    return true;
  }

  // Create output buffer and copy there the object code.
  llvm::StringRef Data = BufferString.str();
  *OutBuffer = LLVMCreateMemoryBufferWithMemoryRangeCopy(Data.data(),
                                                         Data.size(), "result");

#ifndef NDEBUG
  // Check that copied file has the new section.
  Expected<std::unique_ptr<Binary>> Result =
      createBinary(llvm::unwrap(*OutBuffer)->getMemBufferRef());
  assert(Result && (*Result)->isObject());
  checkSectionData(*static_cast<ObjectFile *>((*Result).get()), MDSectionName,
                   StringRef(MetadataPtr, MetadataSize));
#endif // NDEBUG

  return false;
}
