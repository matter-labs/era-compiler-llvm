//===----- lib/Support/ABIBreak.cpp - EnableABIBreakingChecks -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Config/abi-breaking.h"

// Analyzer test code, do not merge
// core.UndefinedBinaryOperatorResult example from
// https://clang-analyzer.llvm.org/available_checks.html

void test_UndefinedBinaryOperatorResult() {
  int x;
  int y = x + 1; // warn: left operand is garbage
}
// Also found without --analyze, sometimes


/// cplusplus.NewDelete test
void test_NewDelete() {
  int *p = new int[1];
  delete[] (++p);
    // warn: argument to 'delete[]' is offset by 4 bytes
    // from the start of memory allocated by 'new[]'
}
// Not found without --analyze


#ifndef _MSC_VER
namespace llvm {

// One of these two variables will be referenced by a symbol defined in
// llvm-config.h. We provide a link-time (or load time for DSO) failure when
// there is a mismatch in the build configuration of the API client and LLVM.
#if LLVM_ENABLE_ABI_BREAKING_CHECKS
int EnableABIBreakingChecks;
#else
int DisableABIBreakingChecks;
#endif

} // end namespace llvm
#endif
