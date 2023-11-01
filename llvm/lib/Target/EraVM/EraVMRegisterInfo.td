//===-- EraVMRegisterInfo.td - EraVM Register defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares that describes the EraVM register file.
//
//===----------------------------------------------------------------------===//

class EraVMRegister<bits<5> num, string n> : Register<n> {
  field bits<5> Num = num;

  let Namespace = "EraVM";
  let HWEncoding{4-0} = num;
}



//===----------------------------------------------------------------------===//
// General purpose registers
//===----------------------------------------------------------------------===//

def R0 : EraVMRegister<0, "r0">;
def R1 : EraVMRegister<1, "r1">;
def R2 : EraVMRegister<2, "r2">;
def R3 : EraVMRegister<3, "r3">;
def R4 : EraVMRegister<4, "r4">;
def R5 : EraVMRegister<5, "r5">;
def R6 : EraVMRegister<6, "r6">;
def R7 : EraVMRegister<7, "r7">;
def R8 : EraVMRegister<8, "r8">;
def R9 : EraVMRegister<9, "r9">;
def R10 : EraVMRegister<10, "r10">;
def R11 : EraVMRegister<11, "r11">;
def R12 : EraVMRegister<12, "r12">;
def R13 : EraVMRegister<13, "r13">;
def R14 : EraVMRegister<14, "r14">;
def R15 : EraVMRegister<15, "r15">;

def GR256 : RegisterClass<"EraVM", [i256], 256, (add R0, R1, R2, R3, R4, R5, R6,
                                                      R7, R8, R9, R10, R11, R12,
                                                      R13, R14, R15)>;

def GRPTR : RegisterClass<"EraVM", [fatptr], 256, (add R0, R1, R2, R3, R4, R5, R6,
                                                      R7, R8, R9, R10, R11, R12,
                                                      R13, R14, R15)>;

//===----------------------------------------------------------------------===//
// Special registers
//===----------------------------------------------------------------------===//

// Contains 0, might be used as a destination reg to dispose the result.
// Note: Before register allocation r0 must always be dead. Live intervals
// analysis would assume any read from R0 as a read of the value put to the
// register (insted of 0), thus assuming the register is live.
// It seems that the simplest way to utilize r0 for 0 materialization is to
// replace inst dst, rN, (loadconst 0) with inst dst, rN, r0 in a peephole pass.

def PC : EraVMRegister<16, "pc">;
def SP : EraVMRegister<17, "sp">;

// Flags pseudo register is to model flags. EraVM doesn't privide explicit
// access to it, but it's needed to track impilicit def and use of flages in
// codegen.
def Flags : EraVMRegister<18, "#BAD#">;

let isAllocatable = 0 in
def SPR256 : RegisterClass<"EraVM", [i256], 256, (add PC, SP, Flags)>;

// Registers that could be used for stack addressing.
def GRStack : RegisterClass<"EraVM", [i256], 256, (add R0, SP)>;
