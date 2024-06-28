//==--------- Assembler.h - Assembler Public C Interface ------*- C++ -*----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header declares the C interface for the llvm-mc assembler.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_C_ASSEMBLER_H
#define LLVM_C_ASSEMBLER_H

#include "llvm-c/ExternC.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Types.h"

LLVM_C_EXTERN_C_BEGIN

/** Translates textual assembly to the object code.  */
LLVMBool LLVMAssembleEraVM(LLVMTargetMachineRef T, LLVMMemoryBufferRef InBuffer,
                           LLVMMemoryBufferRef *OutBuffer, char **ErrorMessage);

/** Checks that the object code complies to the EraVM limits. This function
 *  partially emulates the linker script logic to calculate the bytecode
 *  dimensions that include:
 *    - number of instructions (to be <= 2^16)
 *    - total binary size (to be <= (2^16 - 1) * 32)
 *  */
LLVMBool LLVMExceedsSizeLimitEraVM(LLVMMemoryBufferRef MemBuf,
                                   uint64_t MetadataSize);

LLVM_C_EXTERN_C_END

#endif // LLVM_C_ASSEMBLER_H
