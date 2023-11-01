//===-- InstrMapping.cpp - Instruction mapping test -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Coverage tests for EraVM's Tablegen InstrMapping implementations.
//
//===----------------------------------------------------------------------===//

#include "Utils.h"

#define EXPECT_MAP_BASE(OpcodeName, S1, S2)                                    \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##rrr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##rrs_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##srr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##srs_, S1, S2)

#define EXPECT_MAP_COMMUTABLE(OpcodeName, S1, S2)                              \
  EXPECT_MAP_BASE(OpcodeName, S1, S2);                                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##irr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##irs_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##crr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##crs_, S1, S2)

#define EXPECT_MAP_NONCOMMUTABLE(OpcodeName, S1, S2)                           \
  EXPECT_MAP_COMMUTABLE(OpcodeName, S1, S2);                                   \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##xrr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##xrs_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##yrr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##yrs_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##zrr_, S1, S2);                         \
  EXPECT_MAP_EQ_OPCODE_BASE(OpcodeName##zrs_, S1, S2)

#define EXPECT_MAP_ARITH2(Opcode, S1, S2)                                      \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##rrrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##irrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##irsr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##crrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##srrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##rrsr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##crsr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##srsr_, S1, S2)

#define EXPECT_MAP_ARITH2_EXTEND(Opcode, S1, S2)                               \
  EXPECT_MAP_ARITH2(Opcode, S1, S2);                                           \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##xrrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##yrrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##zrrr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##xrsr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##yrsr_, S1, S2);                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##zrsr_, S1, S2)

#define EXPECT_MAP_PTR(Opcode, S1, S2)                                         \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##rrr_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##srr_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##xrr_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##yrr_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##zrr_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##rrs_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##srs_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##xrs_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##yrs_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##zrs_, S1, S2)

#define EXPECT_MAP_OPERAND_IN_AM_PTR(Mnemonic, S1, S2)                         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rr_s, Mnemonic##S2##rr_s);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rr_v, Mnemonic##S2##rr_v);

#define EXPECT_MAP_OPERAND_IN_AM(Mnemonic, S1, S2)                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rr_s, Mnemonic##S2##rr_s);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rr_v, Mnemonic##S2##rr_v);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rs_s, Mnemonic##S2##rs_s);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rs_v, Mnemonic##S2##rs_v);

#define EXPECT_MAP_OPERAND_IN_AM2(Mnemonic, S1, S2)                            \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rrr_s, Mnemonic##S2##rrr_s);         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rrr_v, Mnemonic##S2##rrr_v);         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rsr_s, Mnemonic##S2##rsr_s);         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rsr_v, Mnemonic##S2##rsr_v);

#define EXPECT_MAP_OPERAND_OUT_AM_BASE(Mnemonic, S1, S2)                       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##rr##S1##_s, Mnemonic##rr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##rr##S1##_v, Mnemonic##rr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##ir##S1##_s, Mnemonic##ir##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##ir##S1##_v, Mnemonic##ir##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##cr##S1##_s, Mnemonic##cr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##cr##S1##_v, Mnemonic##cr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##sr##S1##_s, Mnemonic##sr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##sr##S1##_v, Mnemonic##sr##S2##_v);

#define EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(Mnemonic, S1, S2)              \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##xr##S1##_s, Mnemonic##xr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##xr##S1##_v, Mnemonic##xr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##yr##S1##_s, Mnemonic##yr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##yr##S1##_v, Mnemonic##yr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##zr##S1##_s, Mnemonic##zr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##zr##S1##_v, Mnemonic##zr##S2##_v);

#define EXPECT_MAP_OPERAND_OUT_AM_PTR(Mnemonic, S1, S2)                        \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##rr##S1##_s, Mnemonic##rr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##rr##S1##_v, Mnemonic##rr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##sr##S1##_s, Mnemonic##sr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##sr##S1##_v, Mnemonic##sr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##xr##S1##_s, Mnemonic##xr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##xr##S1##_v, Mnemonic##xr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##yr##S1##_s, Mnemonic##yr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##yr##S1##_v, Mnemonic##yr##S2##_v);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##zr##S1##_s, Mnemonic##zr##S2##_s);       \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##zr##S1##_v, Mnemonic##zr##S2##_v);

#define EXPECT_MAP_OPERAND_OUT_AM_BASE2(Mnemonic, S1, S2)                      \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##rr##S1##r_s, Mnemonic##rr##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##rr##S1##r_v, Mnemonic##rr##S2##r_v);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##ir##S1##r_s, Mnemonic##ir##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##ir##S1##r_v, Mnemonic##ir##S2##r_v);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##cr##S1##r_s, Mnemonic##cr##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##cr##S1##r_v, Mnemonic##cr##S2##r_v);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##sr##S1##r_s, Mnemonic##sr##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##sr##S1##r_v, Mnemonic##sr##S2##r_v);

#define EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE2(Mnemonic, S1, S2)             \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##xr##S1##r_s, Mnemonic##xr##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##xr##S1##r_v, Mnemonic##xr##S2##r_v);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##yr##S1##r_s, Mnemonic##yr##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##yr##S1##r_v, Mnemonic##yr##S2##r_v);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##zr##S1##r_s, Mnemonic##zr##S2##r_s);     \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##zr##S1##r_v, Mnemonic##zr##S2##r_v);

#define EXPECT_MAP_OPERAND_SWAP(Mnemonic, S1, S2)                              \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rr_s, Mnemonic##S2##rr_s);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rr_v, Mnemonic##S2##rr_v);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rs_s, Mnemonic##S2##rs_s);           \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rs_v, Mnemonic##S2##rs_v);

#define EXPECT_MAP_OPERAND_SWAP2(Mnemonic, S1, S2)                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rrr_s, Mnemonic##S2##rrr_s);         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rrr_v, Mnemonic##S2##rrr_v);         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rsr_s, Mnemonic##S2##rsr_s);         \
  EXPECT_MAP_EQ_OPCODE_BASE(Mnemonic##S1##rsr_v, Mnemonic##S2##rsr_v);

#define TEST_INSTR_MAPS(S1, S2)                                                \
  EXPECT_MAP_COMMUTABLE(EraVM::ADD, S1, S2);                                  \
  EXPECT_MAP_COMMUTABLE(EraVM::AND, S1, S2);                                  \
  EXPECT_MAP_COMMUTABLE(EraVM::OR, S1, S2);                                   \
  EXPECT_MAP_COMMUTABLE(EraVM::XOR, S1, S2);                                  \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::SUB, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::SHL, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::SHR, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::ROL, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::ROR, S1, S2);                               \
  EXPECT_MAP_PTR(EraVM::PTR_ADD, S1, S2);                                     \
  EXPECT_MAP_PTR(EraVM::PTR_PACK, S1, S2);                                    \
  EXPECT_MAP_PTR(EraVM::PTR_SHRINK, S1, S2);                                  \
  EXPECT_MAP_ARITH2(EraVM::MUL, S1, S2);                                      \
  EXPECT_MAP_ARITH2_EXTEND(EraVM::DIV, S1, S2)

#define TEST_IN_AM_MAPS_NONPTR(S2)                                             \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ADD, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ADD, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ADD, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ADD, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::AND, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::AND, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::AND, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::AND, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::OR, r, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::OR, i, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::OR, c, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::OR, s, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::XOR, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::XOR, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::XOR, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::XOR, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::MUL, r, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::MUL, i, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::MUL, c, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::MUL, s, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SUB, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHL, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::SHR, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROL, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(EraVM::ROR, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, r, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, i, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, x, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, c, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, y, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, s, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(EraVM::DIV, z, S2)

#define TEST_IN_AM_MAPS_PTR(S2)                                                \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_ADD, r, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_ADD, x, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_ADD, y, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_ADD, s, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_ADD, z, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_PACK, r, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_PACK, x, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_PACK, y, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_PACK, s, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_PACK, z, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_SHRINK, r, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_SHRINK, x, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_SHRINK, y, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_SHRINK, s, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(EraVM::PTR_SHRINK, z, S2);

#define TEST_OUT_AM_MAPS(S1, S2)                                               \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(EraVM::ADD, S1, S2);                         \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(EraVM::AND, S1, S2);                         \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(EraVM::OR, S1, S2);                          \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(EraVM::XOR, S1, S2);                         \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(EraVM::SUB, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(EraVM::SHL, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(EraVM::SHR, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(EraVM::ROL, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(EraVM::ROR, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_PTR(EraVM::PTR_ADD, S1, S2);                      \
  EXPECT_MAP_OPERAND_OUT_AM_PTR(EraVM::PTR_PACK, S1, S2);                     \
  EXPECT_MAP_OPERAND_OUT_AM_PTR(EraVM::PTR_SHRINK, S1, S2);                   \
  EXPECT_MAP_OPERAND_OUT_AM_BASE2(EraVM::MUL, S1, S2);                        \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE2(EraVM::DIV, S1, S2)

#define TEST_MAPS_SWAP_NONPTR(S1, S2)                                          \
  EXPECT_MAP_OPERAND_SWAP(EraVM::SUB, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(EraVM::SHL, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(EraVM::SHR, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(EraVM::ROL, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(EraVM::ROR, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP2(EraVM::DIV, S1, S2);

#define TEST_MAPS_SWAP_PTR(S1, S2)                                             \
  EXPECT_MAP_OPERAND_SWAP(EraVM::PTR_ADD, S1, S2);                            \
  EXPECT_MAP_OPERAND_SWAP(EraVM::PTR_PACK, S1, S2);                           \
  EXPECT_MAP_OPERAND_SWAP(EraVM::PTR_SHRINK, S1, S2);

TEST(GetFlagSettingOpcode, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode, Subfix1, Subfix2)                    \
  EXPECT_EQ(EraVM::getFlagSettingOpcode(Opcode##Subfix1), Opcode##Subfix2)

  TEST_INSTR_MAPS(s, v);
}

TEST(GetNonFlagSettingOpcode, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode, Subfix1, Subfix2)                    \
  EXPECT_EQ(EraVM::getNonFlagSettingOpcode(Opcode##Subfix1), Opcode##Subfix2)

  TEST_INSTR_MAPS(v, s);
}

TEST(GetPseudoMapOpcode, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode, Subfix1, Subfix2)                    \
  EXPECT_EQ(EraVM::getPseudoMapOpcode(Opcode##Subfix1), Opcode##Subfix2)

  TEST_INSTR_MAPS(p, s);
}

TEST(GetOperandAM_RR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithRRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(r);
  TEST_IN_AM_MAPS_PTR(r);
}

TEST(GetOperandAM_IR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithIRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(i);
  TEST_IN_AM_MAPS_PTR(x);
}

TEST(GetOperandAM_CR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithCRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(c);
  TEST_IN_AM_MAPS_PTR(y);
}

TEST(GetOperandAM_SR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithSRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(s);
  TEST_IN_AM_MAPS_PTR(s);
}

TEST(GetResultAM_RR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithRROutAddrMode(Opcode1), Opcode2)

  TEST_OUT_AM_MAPS(r, r);
  TEST_OUT_AM_MAPS(s, r);
}

TEST(SwapToX, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithInsSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(i, x);
  TEST_MAPS_SWAP_NONPTR(x, x);
}

TEST(SwapToI, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithInsNotSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(i, i);
  TEST_MAPS_SWAP_NONPTR(x, i);
}

TEST(SwapToY, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithInsSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(c, y);
  TEST_MAPS_SWAP_NONPTR(y, y);
}

TEST(SwapToC, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithInsNotSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(c, c);
  TEST_MAPS_SWAP_NONPTR(y, c);
}

TEST(SwapToZ, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithInsSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(s, z);
  TEST_MAPS_SWAP_NONPTR(z, z);
  TEST_MAPS_SWAP_PTR(s, z);
  TEST_MAPS_SWAP_PTR(z, z);
}

TEST(SwapToS, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(EraVM::getWithInsNotSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(s, s);
  TEST_MAPS_SWAP_NONPTR(z, s);
  TEST_MAPS_SWAP_PTR(s, s);
  TEST_MAPS_SWAP_PTR(z, s);
}
