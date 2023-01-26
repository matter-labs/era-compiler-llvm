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
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##xrs_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##yrs_, S1, S2);                             \
  EXPECT_MAP_EQ_OPCODE_BASE(Opcode##zrs_, S1, S2)

#define TEST_INSTR_MAPS(S1, S2)                                                \
  EXPECT_MAP_COMMUTABLE(EraVM::ADD, S1, S2);                                   \
  EXPECT_MAP_COMMUTABLE(EraVM::AND, S1, S2);                                   \
  EXPECT_MAP_COMMUTABLE(EraVM::OR, S1, S2);                                    \
  EXPECT_MAP_COMMUTABLE(EraVM::XOR, S1, S2);                                   \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::SUB, S1, S2);                                \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::SHL, S1, S2);                                \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::SHR, S1, S2);                                \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::ROL, S1, S2);                                \
  EXPECT_MAP_NONCOMMUTABLE(EraVM::ROR, S1, S2);                                \
  EXPECT_MAP_PTR(EraVM::PTR_ADD, S1, S2);                                      \
  EXPECT_MAP_PTR(EraVM::PTR_PACK, S1, S2);                                     \
  EXPECT_MAP_PTR(EraVM::PTR_SHRINK, S1, S2);                                   \
  EXPECT_MAP_ARITH2(EraVM::MUL, S1, S2);                                       \
  EXPECT_MAP_ARITH2_EXTEND(EraVM::DIV, S1, S2)

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
