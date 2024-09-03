//==--------- ObjCopy.h - ObjCopy Public C Interface ----------*- C++ -*----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header declares the C interface for the llvm-objcopy functionality.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_C_OBJCOPY_H
#define LLVM_C_OBJCOPY_H

#include "llvm-c/ExternC.h"
#include "llvm-c/Types.h"

LLVM_C_EXTERN_C_BEGIN

/** Creates new section '.eravm-metadata' in the input object file \p InBuffer
 *  and puts there the data pointed to by the \p MetadataPtr. The result is
 *  returned via \p OutBuffer. In case of an error the function returns 'true'
 *  and an error message is passes via \p ErrorMessage. The message should be
 *  disposed by LLVMDisposeMessage. **/
LLVMBool LLVMAddMetadataEraVM(LLVMMemoryBufferRef InBuffer,
                              const char *MetadataPtr, uint64_t MetadataSize,
                              LLVMMemoryBufferRef *OutBuffer,
                              char **ErrorMessage);
LLVM_C_EXTERN_C_END

#endif // LLVM_C_OBJCOPY_H
