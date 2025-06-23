//===---------- llvm/unittests/MC/AssemblerTest.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm-c/ErrorHandling.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "gtest/gtest.h"
#include <memory>

using namespace llvm;

static void on_std(uint64_t SpillRegionSize) {
  EXPECT_TRUE(SpillRegionSize == 32);
}

static void on_std_exit(uint64_t SpillRegionSize) {
  EXPECT_TRUE(SpillRegionSize == 64);
  exit(1);
}

class SpillRegionTest : public testing::Test {};

TEST_F(SpillRegionTest, Basic) {
  LLVMInstallEVMStackErrorHandler(on_std);
  EXPECT_DEATH(report_evm_stack_error("STD error", 32), "STD error");
  LLVMResetEVMStackErrorHandler();
  LLVMInstallEVMStackErrorHandler(on_std_exit);
  EXPECT_DEATH(report_evm_stack_error("STD error", 64), "");
}
