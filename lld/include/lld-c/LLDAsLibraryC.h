//==--------- LLDAsLibraryC.h - LLD Public C Interface --------*- C++ -*----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header declares the following EVM C interfaces:
//  - 'LLVMLinkEVM'
//    The interface to the LLD linker functionality (via lld-as-a-library)
//
//===----------------------------------------------------------------------===//

#ifndef LLD_C_LLDASLIBRARYC_H
#define LLD_C_LLDASLIBRARYC_H

#include "llvm-c/ExternC.h"
#include "llvm-c/Types.h"

LLVM_C_EXTERN_C_BEGIN

// Currently, the size of a linker symbol is limited to 20 bytes, as its the
// only usage is to represent Ethereum addresses which are of 160 bit width.
#define LINKER_SYMBOL_SIZE 20

/** Links the deploy and runtime ELF object files using the information about
 *  dependencies.
 *  \p inBuffers - array of input memory buffers with following structure:
 *
 *   inBuffers[0] - deploy ELF object code
 *   inBuffers[1] - deployed (runtime) ELF object code
 *   --------------------------
 *   inBuffers[2] - 1-st sub-contract (final EVM bytecode)
 *   ...
 *   inBuffers[N] - N-st sub-contract (final EVM bytecode)
 *
 *  Sub-contracts are optional. They should have the same ordering as in
 *  the YUL layout.
 *
 *  \p inBuffersIDs - array of string identifiers of the buffers. IDs correspond
 *  to the object names in the YUL layout.
 *  On success, outBuffers[0] will contain the deploy bytecode and outBuffers[1]
 *  the runtime bytecode.
 *  In case of an error the function returns 'true' and the error message is
 *  passes in \p errorMessage. The message should be disposed by
 *  'LLVMDisposeMessage'. */
LLVMBool LLVMLinkEVM(LLVMMemoryBufferRef *inBuffers, const char *inBuffersIDs[],
                     uint64_t numInBuffers, LLVMMemoryBufferRef outBuffers[2],
                     char **errorMessage);

LLVM_C_EXTERN_C_END

#endif // LLD_C_LLDASLIBRARYC_H
