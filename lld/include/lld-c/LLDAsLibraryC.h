//==----- EVMFrameLowering.h - Define frame lowering for EVM --*- C++ -*----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header declares the C interface to the lld-as-a-library, which can be
// used to invoke LLD functionality.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_C_LLDASLIBRARYC_H
#define LLD_C_LLDASLIBRARYC_H

#include "llvm-c/ExternC.h"
#include "llvm-c/Types.h"

LLVM_C_EXTERN_C_BEGIN

/** Performs linkage a operation of an object code via lld-as-a-library.
    Input/output files are transferred via memory buffers. */

LLVMBool LLVMLinkMemoryBuffers(LLVMMemoryBufferRef *inMemBufs, size_t numInBufs,
                               LLVMMemoryBufferRef *outMemBuf,
                               const char **lldArgs, size_t numLldArgs);

LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef inMemBuf,
                     LLVMMemoryBufferRef *outMemBuf);
LLVM_C_EXTERN_C_END

#endif // LLD_C_LLDASLIBRARYC_H
