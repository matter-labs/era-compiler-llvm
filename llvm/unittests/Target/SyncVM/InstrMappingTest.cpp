//===------------ llvm/unittests/Target/SyncVM/InstrMapping.cpp -----------===//
//
// Coverage tests for SyncVM's Tablegen InstrMapping implementations.
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
  EXPECT_MAP_COMMUTABLE(SyncVM::ADD, S1, S2);                                  \
  EXPECT_MAP_COMMUTABLE(SyncVM::AND, S1, S2);                                  \
  EXPECT_MAP_COMMUTABLE(SyncVM::OR, S1, S2);                                   \
  EXPECT_MAP_COMMUTABLE(SyncVM::XOR, S1, S2);                                  \
  EXPECT_MAP_NONCOMMUTABLE(SyncVM::SUB, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(SyncVM::SHL, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(SyncVM::SHR, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(SyncVM::ROL, S1, S2);                               \
  EXPECT_MAP_NONCOMMUTABLE(SyncVM::ROR, S1, S2);                               \
  EXPECT_MAP_PTR(SyncVM::PTR_ADD, S1, S2);                                     \
  EXPECT_MAP_PTR(SyncVM::PTR_PACK, S1, S2);                                    \
  EXPECT_MAP_PTR(SyncVM::PTR_SHRINK, S1, S2);                                  \
  EXPECT_MAP_ARITH2(SyncVM::MUL, S1, S2);                                      \
  EXPECT_MAP_ARITH2_EXTEND(SyncVM::DIV, S1, S2)

#define TEST_IN_AM_MAPS_NONPTR(S2)                                             \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ADD, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ADD, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ADD, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ADD, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::AND, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::AND, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::AND, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::AND, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::OR, r, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::OR, i, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::OR, c, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::OR, s, S2);                                 \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::XOR, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::XOR, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::XOR, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::XOR, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::MUL, r, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::MUL, i, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::MUL, c, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::MUL, s, S2);                               \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SUB, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHL, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::SHR, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROL, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, r, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, i, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, x, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, c, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, y, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, s, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM(SyncVM::ROR, z, S2);                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, r, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, i, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, x, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, c, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, y, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, s, S2)                                \
  EXPECT_MAP_OPERAND_IN_AM2(SyncVM::DIV, z, S2)

#define TEST_IN_AM_MAPS_PTR(S2)                                                \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_ADD, r, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_ADD, x, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_ADD, y, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_ADD, s, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_ADD, z, S2);                        \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_PACK, r, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_PACK, x, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_PACK, y, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_PACK, s, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_PACK, z, S2);                       \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_SHRINK, r, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_SHRINK, x, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_SHRINK, y, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_SHRINK, s, S2);                     \
  EXPECT_MAP_OPERAND_IN_AM_PTR(SyncVM::PTR_SHRINK, z, S2);

#define TEST_OUT_AM_MAPS(S1, S2)                                               \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(SyncVM::ADD, S1, S2);                         \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(SyncVM::AND, S1, S2);                         \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(SyncVM::OR, S1, S2);                          \
  EXPECT_MAP_OPERAND_OUT_AM_BASE(SyncVM::XOR, S1, S2);                         \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(SyncVM::SUB, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(SyncVM::SHL, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(SyncVM::SHR, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(SyncVM::ROL, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE(SyncVM::ROR, S1, S2);                \
  EXPECT_MAP_OPERAND_OUT_AM_PTR(SyncVM::PTR_ADD, S1, S2);                      \
  EXPECT_MAP_OPERAND_OUT_AM_PTR(SyncVM::PTR_PACK, S1, S2);                     \
  EXPECT_MAP_OPERAND_OUT_AM_PTR(SyncVM::PTR_SHRINK, S1, S2);                   \
  EXPECT_MAP_OPERAND_OUT_AM_BASE2(SyncVM::MUL, S1, S2);                        \
  EXPECT_MAP_OPERAND_OUT_AM_NONCOMMUTABLE2(SyncVM::DIV, S1, S2)

#define TEST_MAPS_SWAP_NONPTR(S1, S2)                                          \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::SUB, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::SHL, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::SHR, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::ROL, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::ROR, S1, S2);                                \
  EXPECT_MAP_OPERAND_SWAP2(SyncVM::DIV, S1, S2);

#define TEST_MAPS_SWAP_PTR(S1, S2)                                             \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::PTR_ADD, S1, S2);                            \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::PTR_PACK, S1, S2);                           \
  EXPECT_MAP_OPERAND_SWAP(SyncVM::PTR_SHRINK, S1, S2);

TEST(GetFlagSettingOpcode, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode, Subfix1, Subfix2)                    \
  EXPECT_EQ(SyncVM::getFlagSettingOpcode(Opcode##Subfix1), Opcode##Subfix2)

  TEST_INSTR_MAPS(s, v);
}

TEST(GetNonFlagSettingOpcode, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode, Subfix1, Subfix2)                    \
  EXPECT_EQ(SyncVM::getNonFlagSettingOpcode(Opcode##Subfix1), Opcode##Subfix2)

  TEST_INSTR_MAPS(v, s);
}

TEST(GetPseudoMapOpcode, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode, Subfix1, Subfix2)                    \
  EXPECT_EQ(SyncVM::getPseudoMapOpcode(Opcode##Subfix1), Opcode##Subfix2)

  TEST_INSTR_MAPS(p, s);
}

TEST(GetOperandAM_RR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithRRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(r);
  TEST_IN_AM_MAPS_PTR(r);
}

TEST(GetOperandAM_IR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithIRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(i);
  TEST_IN_AM_MAPS_PTR(x);
}

TEST(GetOperandAM_CR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithCRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(c);
  TEST_IN_AM_MAPS_PTR(y);
}

TEST(GetOperandAM_SR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithSRInAddrMode(Opcode1), Opcode2)

  TEST_IN_AM_MAPS_NONPTR(s);
  TEST_IN_AM_MAPS_PTR(s);
}

TEST(GetResultAM_RR, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithRROutAddrMode(Opcode1), Opcode2)

  TEST_OUT_AM_MAPS(r, r);
  TEST_OUT_AM_MAPS(s, r);
}

TEST(SwapToX, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithInsSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(i, x);
  TEST_MAPS_SWAP_NONPTR(x, x);
}

TEST(SwapToI, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithInsNotSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(i, i);
  TEST_MAPS_SWAP_NONPTR(x, i);
}

TEST(SwapToY, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithInsSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(c, y);
  TEST_MAPS_SWAP_NONPTR(y, y);
}

TEST(SwapToC, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithInsNotSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(c, c);
  TEST_MAPS_SWAP_NONPTR(y, c);
}

TEST(SwapToZ, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithInsSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(s, z);
  TEST_MAPS_SWAP_NONPTR(z, z);
  TEST_MAPS_SWAP_PTR(s, z);
  TEST_MAPS_SWAP_PTR(z, z);
}

TEST(SwapToS, TheTest) {
#undef EXPECT_MAP_EQ_OPCODE_BASE
#define EXPECT_MAP_EQ_OPCODE_BASE(Opcode1, Opcode2)                            \
  EXPECT_EQ(SyncVM::getWithInsNotSwapped(Opcode1), Opcode2)

  TEST_MAPS_SWAP_NONPTR(s, s);
  TEST_MAPS_SWAP_NONPTR(z, s);
  TEST_MAPS_SWAP_PTR(s, s);
  TEST_MAPS_SWAP_PTR(z, s);
}
