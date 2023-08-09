//===-- KECCAK.h - KECCAK implementation ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation the Keccak-f[1600] permutation approved in the FIPS 202
// standard, which is used for instantiation of the KECCAK-256 hash function.
//
// [FIPS PUB 202]
//   https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf
// [Keccak Reference]
//   https://keccak.team/files/Keccak-reference-3.0.pdf
// [Keccak Specifications Summary]
//   https://keccak.team/keccak_specs_summary.html
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_KECCAK_H
#define LLVM_SUPPORT_KECCAK_H

#include <array>
#include <cstdint>

namespace llvm {

template <typename T> class ArrayRef;
class StringRef;

namespace KECCAK {
/// Returns a raw 256-bit KECCAK-256 hash for the given data.
std::array<uint8_t, 32> KECCAK_256(ArrayRef<uint8_t> Data);

/// Returns a raw 256-bit KECCAK-256 hash for the given string.
std::array<uint8_t, 32> KECCAK_256(StringRef Str);
} // namespace KECCAK

} // namespace llvm

#endif // LLVM_SUPPORT_KECCAK_H
