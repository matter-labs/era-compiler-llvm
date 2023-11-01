//===-- KECCAK.cpp - Private copy of the KECCAK implementation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This code was taken from public domain
// (https://github.com/XKCP/XKCP/blob/master/Standalone/CompactFIPS202/
//  C/Keccak-readable-and-compact.c), and modified by wrapping it in a
// C++ interface for LLVM, changing formatting, and removing
// unnecessary comments.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/KECCAK.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Host.h"
#include <cstring>

using namespace llvm;

namespace {

#if defined(BYTE_ORDER) && defined(LITTLE_ENDIAN) && BYTE_ORDER == LITTLE_ENDIAN
#define KECCAK_LITTLE_ENDIAN
#endif

#ifndef KECCAK_LITTLE_ENDIAN
///
/// Function to load a 64-bit value using the little-endian (LE) convention.
/// On a LE platform, this could be greatly simplified using a cast.
///
uint64_t load64(const uint8_t *x) {
  int i;
  uint64_t u = 0;
  for (i = 7; i >= 0; --i) {
    u <<= 8;
    u |= x[i];
  }
  return u;
}

///
/// Function to store a 64-bit value using the little-endian (LE) convention.
/// On a LE platform, this could be greatly simplified using a cast.
///
void store64(uint8_t *x, uint64_t u) {
  unsigned int i;
  for (i = 0; i < 8; ++i) {
    x[i] = u;
    u >>= 8;
  }
}

///
/// Function to XOR into a 64-bit value using the little-endian (LE) convention.
/// On a LE platform, this could be greatly simplified using a cast.
///
void xor64(uint8_t *x, uint64_t u) {
  unsigned int i;
  for (i = 0; i < 8; ++i) {
    x[i] ^= u;
    u >>= 8;
  }
}
#endif

typedef uint64_t tKeccakLane;

///
/// A readable and compact implementation of the Keccak-f[1600] permutation.
///
#define ROL64(a, offset)                                                       \
  ((((uint64_t)a) << offset) ^ (((uint64_t)a) >> (64 - offset)))
#define i(x, y) ((x) + 5 * (y))

#ifdef KECCAK_LITTLE_ENDIAN
#define readLane(x, y) (((tKeccakLane *)state)[i(x, y)])
#define writeLane(x, y, lane) (((tKeccakLane *)state)[i(x, y)]) = (lane)
#define XORLane(x, y, lane) (((tKeccakLane *)state)[i(x, y)]) ^= (lane)
#else
#define readLane(x, y) load64((uint8_t *)state + sizeof(tKeccakLane) * i(x, y))
#define writeLane(x, y, lane)                                                  \
  store64((uint8_t *)state + sizeof(tKeccakLane) * i(x, y), lane)
#define XORLane(x, y, lane)                                                    \
  xor64((uint8_t *)state + sizeof(tKeccakLane) * i(x, y), lane)
#endif

///
/// Function that computes the linear feedback shift register (LFSR) used to
/// define the round constants (see [Keccak Reference, Section 1.2]).
///
int LFSR86540(uint8_t *LFSR) {
  const int result = ((*LFSR) & 0x01) != 0;
  if (((*LFSR) & 0x80) != 0)
    // Primitive polynomial over GF(2): x^8+x^6+x^5+x^4+1
    (*LFSR) = ((*LFSR) << 1) ^ 0x71;
  else
    (*LFSR) <<= 1;
  return result;
}

///
/// Function that computes the Keccak-f[1600] permutation on the given state.
///
void KeccakF1600_StatePermute(void *state) {
  unsigned int round, x, y, j, t;
  uint8_t LFSRstate = 0x01;

  for (round = 0; round < 24; round++) {
    {
      // θ step (see [Keccak Reference, Section 2.3.2])
      tKeccakLane C[5], D;
      // Compute the parity of the columns
      for (x = 0; x < 5; x++)
        C[x] = readLane(x, 0) ^ readLane(x, 1) ^ readLane(x, 2) ^
               readLane(x, 3) ^ readLane(x, 4);
      for (x = 0; x < 5; x++) {
        /* Compute the θ effect for a given column */
        D = C[(x + 4) % 5] ^ ROL64(C[(x + 1) % 5], 1);
        /* Add the θ effect to the whole column */
        for (y = 0; y < 5; y++)
          XORLane(x, y, D);
      }
    }

    {
      // ρ and π steps (see [Keccak Reference, Sections 2.3.3 and 2.3.4])
      tKeccakLane current, temp;
      // Start at coordinates (1 0)
      x = 1;
      y = 0;
      current = readLane(x, y);
      // Iterate over ((0 1)(2 3))^t * (1 0) for 0 ≤ t ≤ 23
      for (t = 0; t < 24; t++) {
        // Compute the rotation constant r = (t+1)(t+2)/2
        const unsigned int r = ((t + 1) * (t + 2) / 2) % 64;
        // Compute ((0 1)(2 3)) * (x y)
        const unsigned int Y = (2 * x + 3 * y) % 5;
        x = y;
        y = Y;
        // Swap current and state(x,y), and rotate
        temp = readLane(x, y);
        writeLane(x, y, ROL64(current, r));
        current = temp;
      }
    }

    {
      // χ step (see [Keccak Reference, Section 2.3.1])
      tKeccakLane temp[5];
      for (y = 0; y < 5; y++) {
        // Take a copy of the plane
        for (x = 0; x < 5; x++)
          temp[x] = readLane(x, y);
        // Compute χ on the plane
        for (x = 0; x < 5; x++)
          writeLane(x, y, temp[x] ^ ((~temp[(x + 1) % 5]) & temp[(x + 2) % 5]));
      }
    }

    {
      // ι step (see [Keccak Reference, Section 2.3.5])
      for (j = 0; j < 7; j++) {
        const unsigned int bitPosition = (1 << j) - 1; // 2^j-1
        if (LFSR86540(&LFSRstate))
          XORLane(0, 0, (tKeccakLane)1 << bitPosition);
      }
    }
  }
}

///
/// Function to compute the Keccak[r, c] sponge function over a given input
/// using the Keccak-f[1600] permutation.
///
/// @param  rate
///   The value of the rate r.
/// @param  capacity
///   The value of the capacity c.
/// @param  input
///   Pointer to the input message.
/// @param  inputByteLen
///   The number of input bytes provided in the input message.
/// @param  delimitedSuffix
///   Bits that will be automatically appended to the end of the input message,
///   as in domain separation. This is a byte containing from 0 to 7 bits.
///   These <i>n</i> bits must be in the least significant bit positions and
///   must be delimited with a bit 1 at position <i>n</i>
///   (counting from 0=LSB to 7=MSB) and followed by bits 0 from position
///   <i>n</i>+1 to position 7.
///   Some examples:
///     - If no bits are to be appended, then @a delimitedSuffix must be 0x01.
///     - If the 2-bit sequence 0,1 is to be appended (as for SHA3-*),
///       @a delimitedSuffix must be 0x06.
///     - If the 4-bit sequence 1,1,1,1 is to be appended (as for SHAKE*),
///       @a delimitedSuffix must be 0x1F.
///     - If the 7-bit sequence 1,1,0,1,0,0,0 is to be absorbed,
///       @a delimitedSuffix must be 0x8B.
/// @param  output
///   Pointer to the buffer where to store the output.
/// @param  outputByteLen
///   The number of output bytes desired.
/// @pre
///   One must have r+c=1600 and the rate a multiple of 8 bits in this
///   implementation.
///
#define MIN(a, b) ((a) < (b) ? (a) : (b))
void KeccakSponge(unsigned int rate, unsigned int capacity,
                  const unsigned char *input,
                  unsigned long long int inputByteLen,
                  unsigned char delimitedSuffix, unsigned char *output,
                  unsigned long long int outputByteLen) {
  uint8_t state[200];
  const unsigned int rateInBytes = rate / 8;
  unsigned int blockSize = 0;
  unsigned int i;

  if (((rate + capacity) != 1600) || ((rate % 8) != 0))
    return;

  // Initialize the state
  std::memset(state, 0, sizeof(state));

  // Absorb all the input blocks
  while (inputByteLen > 0) {
    blockSize = MIN(inputByteLen, rateInBytes);
    for (i = 0; i < blockSize; i++)
      state[i] ^= input[i];
    input += blockSize;
    inputByteLen -= blockSize;

    if (blockSize == rateInBytes) {
      KeccakF1600_StatePermute(state);
      blockSize = 0;
    }
  }

  // Do the padding and switch to the squeezing phase
  // Absorb the last few bits and add the first bit of padding
  // (which coincides with the delimiter in delimitedSuffix)
  state[blockSize] ^= delimitedSuffix;
  // If the first bit of padding is at position rate-1, we need a whole new
  // block for the second bit of padding
  if (((delimitedSuffix & 0x80) != 0) && (blockSize == (rateInBytes - 1)))
    KeccakF1600_StatePermute(state);
  // Add the second bit of padding
  state[rateInBytes - 1] ^= 0x80;
  // Switch to the squeezing phase
  KeccakF1600_StatePermute(state);

  // Squeeze out all the output blocks
  while (outputByteLen > 0) {
    blockSize = MIN(outputByteLen, rateInBytes);
    std::memcpy(output, state, blockSize);
    output += blockSize;
    outputByteLen -= blockSize;

    if (outputByteLen > 0)
      KeccakF1600_StatePermute(state);
  }
}

} // end anonymous namespace

///
/// Function to compute KECCAK-256 hash on the input message. The output length
/// is fixed to 32 bytes.
///
std::array<uint8_t, 32> KECCAK::KECCAK_256(ArrayRef<uint8_t> Data) {
  std::array<uint8_t, 32> Result;
  KeccakSponge(1088, 512, Data.data(), Data.size(), 0x01, Result.data(), 32);
  return Result;
}

///
/// Function to compute KECCAK-256 hash on the input string message. The output
/// length is fixed to 32 bytes.
///
std::array<uint8_t, 32> KECCAK::KECCAK_256(StringRef Str) {
  return KECCAK_256(
      ArrayRef(reinterpret_cast<const uint8_t *>(Str.data()), Str.size()));
}
