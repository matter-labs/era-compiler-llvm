//===- ValueTypes.td - ValueType definitions ---------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Value types - These values correspond to the register types defined in the
// MachineValueTypes.h file.  If you update anything here, you must update it
// there as well!
//
//===----------------------------------------------------------------------===//

class ValueType<int size, int value> {
  string Namespace = "MVT";
  int Size = size;
  int Value = value;
}

def OtherVT : ValueType<0,   1>;  // "Other" value
def i1      : ValueType<1,   2>;  // One bit boolean value
def i8      : ValueType<8,   3>;  // 8-bit integer value
def i16     : ValueType<16,  4>;  // 16-bit integer value
def i32     : ValueType<32,  5>;  // 32-bit integer value
def i64     : ValueType<64,  6>;  // 64-bit integer value
def i128    : ValueType<128, 7>;  // 128-bit integer value
def i256    : ValueType<256, 8>;  // 256-bit integer value

def bf16    : ValueType<16,   9>;  // 16-bit brain floating point value
def f16     : ValueType<16,  10>;  // 16-bit floating point value
def f32     : ValueType<32,  11>;  // 32-bit floating point value
def f64     : ValueType<64,  12>;  // 64-bit floating point value
def f80     : ValueType<80,  13>;  // 80-bit floating point value
def f128    : ValueType<128, 14>;  // 128-bit floating point value
def ppcf128 : ValueType<128, 15>;  // PPC 128-bit floating point value

def v1i1    : ValueType<1,    16>;  //    1 x i1 vector value
def v2i1    : ValueType<2,    17>;  //    2 x i1 vector value
def v4i1    : ValueType<4,    18>;  //    4 x i1 vector value
def v8i1    : ValueType<8,    19>;  //    8 x i1 vector value
def v16i1   : ValueType<16,   20>;  //   16 x i1 vector value
def v32i1   : ValueType<32,   21>;  //   32 x i1 vector value
def v64i1   : ValueType<64,   22>;  //   64 x i1 vector value
def v128i1  : ValueType<128,  23>;  //  128 x i1 vector value
def v256i1  : ValueType<256,  24>;  //  256 x i1 vector value
def v512i1  : ValueType<512,  25>;  //  512 x i1 vector value
def v1024i1 : ValueType<1024, 26>;  // 1024 x i1 vector value

def v1i8    : ValueType<8,    27>;  //    1 x i8 vector value
def v2i8    : ValueType<16,   28>;  //    2 x i8 vector value
def v4i8    : ValueType<32,   29>;  //    4 x i8 vector value
def v8i8    : ValueType<64,   30>;  //    8 x i8 vector value
def v16i8   : ValueType<128,  31>;  //   16 x i8 vector value
def v32i8   : ValueType<256,  32>;  //   32 x i8 vector value
def v64i8   : ValueType<512,  33>;  //   64 x i8 vector value
def v128i8  : ValueType<1024, 34>;  //  128 x i8 vector value
def v256i8  : ValueType<2048, 35>;  //  256 x i8 vector value
def v512i8  : ValueType<4096, 36>;  //  512 x i8 vector value
def v1024i8 : ValueType<8192, 37>;  // 1024 x i8 vector value

def v1i16   : ValueType<16,   38>;  //   1 x i16 vector value
def v2i16   : ValueType<32,   39>;  //   2 x i16 vector value
def v3i16   : ValueType<48,   40>;  //   3 x i16 vector value
def v4i16   : ValueType<64,   41>;  //   4 x i16 vector value
def v8i16   : ValueType<128,  42>;  //   8 x i16 vector value
def v16i16  : ValueType<256,  43>;  //  16 x i16 vector value
def v32i16  : ValueType<512,  44>;  //  32 x i16 vector value
def v64i16  : ValueType<1024, 45>;  //  64 x i16 vector value
def v128i16 : ValueType<2048, 46>;  // 128 x i16 vector value
def v256i16 : ValueType<4096, 47>;  // 256 x i16 vector value
def v512i16 : ValueType<8192, 48>;  // 512 x i16 vector value

def v1i32    : ValueType<32,    49>;  //    1 x i32 vector value
def v2i32    : ValueType<64,    50>;  //    2 x i32 vector value
def v3i32    : ValueType<96,    51>;  //    3 x i32 vector value
def v4i32    : ValueType<128,   52>;  //    4 x i32 vector value
def v5i32    : ValueType<160,   53>;  //    5 x i32 vector value
def v6i32    : ValueType<192,   54>;  //    6 x f32 vector value
def v7i32    : ValueType<224,   55>;  //    7 x f32 vector value
def v8i32    : ValueType<256,   56>;  //    8 x i32 vector value
def v16i32   : ValueType<512,   57>;  //   16 x i32 vector value
def v32i32   : ValueType<1024,  58>;  //   32 x i32 vector value
def v64i32   : ValueType<2048,  59>;  //   64 x i32 vector value
def v128i32  : ValueType<4096,  60>;  //  128 x i32 vector value
def v256i32  : ValueType<8192,  61>;  //  256 x i32 vector value
def v512i32  : ValueType<16384, 62>;  //  512 x i32 vector value
def v1024i32 : ValueType<32768, 63>;  // 1024 x i32 vector value
def v2048i32 : ValueType<65536, 64>;  // 2048 x i32 vector value

def v1i64   : ValueType<64,    65>;  //   1 x i64 vector value
def v2i64   : ValueType<128,   66>;  //   2 x i64 vector value
def v3i64   : ValueType<192,   67>;  //   3 x i64 vector value
def v4i64   : ValueType<256,   68>;  //   4 x i64 vector value
def v8i64   : ValueType<512,   69>;  //   8 x i64 vector value
def v16i64  : ValueType<1024,  70>;  //  16 x i64 vector value
def v32i64  : ValueType<2048,  71>;  //  32 x i64 vector value
def v64i64  : ValueType<4096,  72>;  //  64 x i64 vector value
def v128i64 : ValueType<8192,  73>;  // 128 x i64 vector value
def v256i64 : ValueType<16384, 74>;  // 256 x i64 vector value

def v1i128 : ValueType<128, 75>;  //  1 x i128 vector value

def v1f16    : ValueType<16,     76>;  //    1 x f16 vector value
def v2f16    : ValueType<32,     77>;  //    2 x f16 vector value
def v3f16    : ValueType<48,     78>;  //    3 x f16 vector value
def v4f16    : ValueType<64,     79>;  //    4 x f16 vector value
def v8f16    : ValueType<128,    80>;  //    8 x f16 vector value
def v16f16   : ValueType<256,    81>;  //   16 x f16 vector value
def v32f16   : ValueType<512,    82>;  //   32 x f16 vector value
def v64f16   : ValueType<1024,   83>;  //   64 x f16 vector value
def v128f16  : ValueType<2048,   84>;  //  128 x f16 vector value
def v256f16  : ValueType<4096,   85>;  //  256 x f16 vector value
def v512f16  : ValueType<8192,   86>;  //  512 x f16 vector value

def v2bf16   : ValueType<32,     87>;  //    2 x bf16 vector value
def v3bf16   : ValueType<48,     88>;  //    3 x bf16 vector value
def v4bf16   : ValueType<64,     89>;  //    4 x bf16 vector value
def v8bf16   : ValueType<128,    90>;  //    8 x bf16 vector value
def v16bf16  : ValueType<256,    91>;  //   16 x bf16 vector value
def v32bf16  : ValueType<512,    92>;  //   32 x bf16 vector value
def v64bf16  : ValueType<1024,   93>;  //   64 x bf16 vector value
def v128bf16 : ValueType<2048,   94>;  //  128 x bf16 vector value

def v1f32    : ValueType<32,     95>;  //    1 x f32 vector value
def v2f32    : ValueType<64,     96>;  //    2 x f32 vector value
def v3f32    : ValueType<96,     97>;  //    3 x f32 vector value
def v4f32    : ValueType<128,    98>;  //    4 x f32 vector value
def v5f32    : ValueType<160,    99>;  //    5 x f32 vector value
def v6f32    : ValueType<192,   100>;  //    6 x f32 vector value
def v7f32    : ValueType<224,   101>;  //    7 x f32 vector value
def v8f32    : ValueType<256,   102>;  //    8 x f32 vector value
def v16f32   : ValueType<512,   103>;  //   16 x f32 vector value
def v32f32   : ValueType<1024,  104>;  //   32 x f32 vector value
def v64f32   : ValueType<2048,  105>;  //   64 x f32 vector value
def v128f32  : ValueType<4096,  106>;  //  128 x f32 vector value
def v256f32  : ValueType<8192,  107>;  //  256 x f32 vector value
def v512f32  : ValueType<16384, 108>;  //  512 x f32 vector value
def v1024f32 : ValueType<32768, 109>;  // 1024 x f32 vector value
def v2048f32 : ValueType<65536, 110>;  // 2048 x f32 vector value

def v1f64    : ValueType<64,    111>;  //    1 x f64 vector value
def v2f64    : ValueType<128,   112>;  //    2 x f64 vector value
def v3f64    : ValueType<192,   113>;  //    3 x f64 vector value
def v4f64    : ValueType<256,   114>;  //    4 x f64 vector value
def v8f64    : ValueType<512,   115>;  //    8 x f64 vector value
def v16f64   : ValueType<1024,  116>;  //   16 x f64 vector value
def v32f64   : ValueType<2048,  117>;  //   32 x f64 vector value
def v64f64   : ValueType<4096,  118>;  //   64 x f64 vector value
def v128f64  : ValueType<8192,  119>;  //  128 x f64 vector value
def v256f64  : ValueType<16384, 120>;  //  256 x f64 vector value

def nxv1i1  : ValueType<1,  121>;  // n x  1 x i1  vector value
def nxv2i1  : ValueType<2,  122>;  // n x  2 x i1  vector value
def nxv4i1  : ValueType<4,  123>;  // n x  4 x i1  vector value
def nxv8i1  : ValueType<8,  124>;  // n x  8 x i1  vector value
def nxv16i1 : ValueType<16, 125>;  // n x 16 x i1  vector value
def nxv32i1 : ValueType<32, 126>;  // n x 32 x i1  vector value
def nxv64i1 : ValueType<64, 127>;  // n x 64 x i1  vector value

def nxv1i8  : ValueType<8,   128>;  // n x  1 x i8  vector value
def nxv2i8  : ValueType<16,  129>;  // n x  2 x i8  vector value
def nxv4i8  : ValueType<32,  130>;  // n x  4 x i8  vector value
def nxv8i8  : ValueType<64,  131>;  // n x  8 x i8  vector value
def nxv16i8 : ValueType<128, 132>;  // n x 16 x i8  vector value
def nxv32i8 : ValueType<256, 133>;  // n x 32 x i8  vector value
def nxv64i8 : ValueType<512, 134>;  // n x 64 x i8  vector value

def nxv1i16  : ValueType<16,  135>;  // n x  1 x i16 vector value
def nxv2i16  : ValueType<32,  136>;  // n x  2 x i16 vector value
def nxv4i16  : ValueType<64,  137>;  // n x  4 x i16 vector value
def nxv8i16  : ValueType<128, 138>;  // n x  8 x i16 vector value
def nxv16i16 : ValueType<256, 139>;  // n x 16 x i16 vector value
def nxv32i16 : ValueType<512, 140>;  // n x 32 x i16 vector value

def nxv1i32  : ValueType<32,   141>;  // n x  1 x i32 vector value
def nxv2i32  : ValueType<64,   142>;  // n x  2 x i32 vector value
def nxv4i32  : ValueType<128,  143>;  // n x  4 x i32 vector value
def nxv8i32  : ValueType<256,  144>;  // n x  8 x i32 vector value
def nxv16i32 : ValueType<512,  145>;  // n x 16 x i32 vector value
def nxv32i32 : ValueType<1024, 146>;  // n x 32 x i32 vector value

def nxv1i64  : ValueType<64,   147>;  // n x  1 x i64 vector value
def nxv2i64  : ValueType<128,  148>;  // n x  2 x i64 vector value
def nxv4i64  : ValueType<256,  149>;  // n x  4 x i64 vector value
def nxv8i64  : ValueType<512,  150>;  // n x  8 x i64 vector value
def nxv16i64 : ValueType<1024, 151>;  // n x 16 x i64 vector value
def nxv32i64 : ValueType<2048, 152>;  // n x 32 x i64 vector value

def nxv1f16  : ValueType<16,  153>;  // n x  1 x  f16 vector value
def nxv2f16  : ValueType<32,  154>;  // n x  2 x  f16 vector value
def nxv4f16  : ValueType<64,  155>;  // n x  4 x  f16 vector value
def nxv8f16  : ValueType<128, 156>;  // n x  8 x  f16 vector value
def nxv16f16 : ValueType<256, 157>;  // n x 16 x  f16 vector value
def nxv32f16 : ValueType<512, 158>;  // n x 32 x  f16 vector value

def nxv1bf16 : ValueType<16,  159>;  // n x  1 x bf16 vector value
def nxv2bf16 : ValueType<32,  160>;  // n x  2 x bf16 vector value
def nxv4bf16 : ValueType<64,  161>;  // n x  4 x bf16 vector value
def nxv8bf16 : ValueType<128, 162>;  // n x  8 x bf16 vector value

def nxv1f32  : ValueType<32,  163>;  // n x  1 x  f32 vector value
def nxv2f32  : ValueType<64,  164>;  // n x  2 x  f32 vector value
def nxv4f32  : ValueType<128, 165>;  // n x  4 x  f32 vector value
def nxv8f32  : ValueType<256, 166>;  // n x  8 x  f32 vector value
def nxv16f32 : ValueType<512, 167>;  // n x 16 x  f32 vector value

def nxv1f64  : ValueType<64,  168>;  // n x  1 x  f64 vector value
def nxv2f64  : ValueType<128, 169>;  // n x  2 x  f64 vector value
def nxv4f64  : ValueType<256, 170>;  // n x  4 x  f64 vector value
def nxv8f64  : ValueType<512, 171>;  // n x  8 x  f64 vector value

def x86mmx    : ValueType<64,   172>;  // X86 MMX value
def FlagVT    : ValueType<0,    173>;  // Pre-RA sched glue
def isVoid    : ValueType<0,    174>;  // Produces no value
def untyped   : ValueType<8,    175>;  // Produces an untyped value
def funcref   : ValueType<0,    176>;  // WebAssembly's funcref type
def externref : ValueType<0,    177>;  // WebAssembly's externref type
def x86amx    : ValueType<8192, 178>;  // X86 AMX value
def i64x8     : ValueType<512,  179>;  // 8 Consecutive GPRs (AArch64)


def token      : ValueType<0, 248>;  // TokenTy
def MetadataVT : ValueType<0, 249>;  // Metadata

// Pseudo valuetype mapped to the current pointer size to any address space.
// Should only be used in TableGen.
def iPTRAny    : ValueType<0, 250>;

// Pseudo valuetype to represent "vector of any size"
def vAny       : ValueType<0, 251>;

// Pseudo valuetype to represent "float of any format"
def fAny       : ValueType<0, 252>;

// Pseudo valuetype to represent "integer of any bit width"
def iAny       : ValueType<0, 253>;

// Pseudo valuetype mapped to the current pointer size.
def iPTR       : ValueType<0, 254>;

// Pseudo valuetype to represent "any type of any size".
def Any        : ValueType<0, 255>;

/// This class is for targets that want to use pointer types in patterns
/// with the GlobalISelEmitter.  Targets must define their own pointer
/// derived from this class.  The scalar argument should be an
/// integer type with the same bit size as the ponter.
/// e.g. def p0 : PtrValueType <i64, 0>;

class PtrValueType <ValueType scalar, int addrspace> :
    ValueType<scalar.Size, scalar.Value> {
  int AddrSpace = addrspace;
}
