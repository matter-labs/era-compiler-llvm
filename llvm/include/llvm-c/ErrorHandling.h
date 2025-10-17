/*===-- llvm-c/ErrorHandling.h - Error Handling C Interface -------*- C -*-===*\
|*                                                                            *|
|* Part of the LLVM Project, under the Apache License v2.0 with LLVM          *|
|* Exceptions.                                                                *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* This file defines the C interface to LLVM's error handling mechanism.      *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifndef LLVM_C_ERRORHANDLING_H
#define LLVM_C_ERRORHANDLING_H

#include "llvm-c/ExternC.h"
// EVM local beging
#include <stdint.h>
// EVM local end

LLVM_C_EXTERN_C_BEGIN

/**
 * @addtogroup LLVMCError
 *
 * @{
 */

typedef void (*LLVMFatalErrorHandler)(const char *Reason);

// EVM local begin
typedef void (*LLVMStackErrorHandlerEVM)(uint64_t SpillRegionSize);
// EVM local end

/**
 * Install a fatal error handler. By default, if LLVM detects a fatal error, it
 * will call exit(1). This may not be appropriate in many contexts. For example,
 * doing exit(1) will bypass many crash reporting/tracing system tools. This
 * function allows you to install a callback that will be invoked prior to the
 * call to exit(1).
 */
void LLVMInstallFatalErrorHandler(LLVMFatalErrorHandler Handler);

/**
 * Reset the fatal error handler. This resets LLVM's fatal error handling
 * behavior to the default.
 */
void LLVMResetFatalErrorHandler(void);

/**
 * Enable LLVM's built-in stack trace code. This intercepts the OS's crash
 * signals and prints which component of LLVM you were in at the time if the
 * crash.
 */
void LLVMEnablePrettyStackTrace(void);

// EVM local begin
/**
 *  Register a handler that the stackification algorithm invokes when it
 *  requires a pre-allocated spill region.
 */
void LLVMInstallEVMStackErrorHandler(LLVMStackErrorHandlerEVM Handler);

/**
 * Reset the EVM stack error handler.
 */
void LLVMResetEVMStackErrorHandler(void);
// EVM local end

/**
 * @}
 */

LLVM_C_EXTERN_C_END

#endif
