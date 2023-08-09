//===-- KECCAKTest.cpp - KECCAK tests ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements unit tests for the KECCAK functions.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/KECCAK.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

static std::string toHex(ArrayRef<uint8_t> Input) {
  static const char *const LUT = "0123456789abcdef";
  const size_t Length = Input.size();

  std::string Output;
  Output.reserve(2 * Length);
  for (size_t i = 0; i < Length; ++i) {
    const unsigned char c = Input[i];
    Output.push_back(LUT[c >> 4]);
    Output.push_back(LUT[c & 15]);
  }
  return Output;
}

/// Tests an arbitrary set of bytes passed as \p Input.
void TestKECCAKSum(ArrayRef<uint8_t> Input, StringRef Final) {
  auto hash = KECCAK::KECCAK_256(Input);
  auto hashStr = toHex(hash);
  EXPECT_EQ(hashStr, Final);
}

using KV = std::pair<const char *, const char *>;

TEST(KECCAKTest, KECCAK) {
  const std::array<KV, 6> testvectors{
      KV{"",
         "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"},
      KV{"a",
         "3ac225168df54212a25c1c01fd35bebfea408fdac2e31ddd6f80a4bbf9a5f1cb"},
      KV{"abc",
         "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45"},
      KV{"abcdbcdecdefdefgefghfghighijhijk",
         "4b50e45e85ca4a0a9c089890faf83098c75b04fe0e0f9c5488effd1643711033"},
      KV{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "45d3b367a6904e6e8d502ee04999a7c27647f91fa845d456525fd352ae3d7371"},
      KV{"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklm"
         "nopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
         "f519747ed599024f3882238e5ab43960132572b7345fbeb9a90769dafd21ad67"}};

  for (auto input_expected : testvectors) {
    const auto *const str = std::get<0>(input_expected);
    const auto *const expected = std::get<1>(input_expected);
    TestKECCAKSum({reinterpret_cast<const uint8_t *>(str), strlen(str)},
                  expected);
  }
}

} // end anonymous namespace
