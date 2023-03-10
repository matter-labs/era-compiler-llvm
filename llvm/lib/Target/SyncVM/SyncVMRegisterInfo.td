//===-- SyncVMRegisterInfo.td - SyncVM Register defs -------*- tablegen -*-===//
//
//  Declarations that describe the SyncVM register file
//
//===----------------------------------------------------------------------===//

class SyncVMRegister<bits<5> num, string n> : Register<n> {
  field bits<5> Num = num;

  let Namespace = "SyncVM";
  let HWEncoding{4-0} = num;
}



//===----------------------------------------------------------------------===//
// General purpose registers
//===----------------------------------------------------------------------===//

def R0 : SyncVMRegister<0, "r0">;
def R1 : SyncVMRegister<1, "r1">;
def R2 : SyncVMRegister<2, "r2">;
def R3 : SyncVMRegister<3, "r3">;
def R4 : SyncVMRegister<4, "r4">;
def R5 : SyncVMRegister<5, "r5">;
def R6 : SyncVMRegister<6, "r6">;
def R7 : SyncVMRegister<7, "r7">;
def R8 : SyncVMRegister<8, "r8">;
def R9 : SyncVMRegister<9, "r9">;
def R10 : SyncVMRegister<10, "r10">;
def R11 : SyncVMRegister<11, "r11">;
def R12 : SyncVMRegister<12, "r12">;
def R13 : SyncVMRegister<13, "r13">;
def R14 : SyncVMRegister<14, "r14">;
def R15 : SyncVMRegister<15, "r15">;

def GR256 : RegisterClass<"SyncVM", [i256], 256, (add R0, R1, R2, R3, R4, R5, R6,
                                                      R7, R8, R9, R10, R11, R12,
                                                      R13, R14, R15)>;

def GRPTR : RegisterClass<"SyncVM", [fatptr], 256, (add R0, R1, R2, R3, R4, R5, R6,
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

def PC : SyncVMRegister<16, "pc">;
def SP : SyncVMRegister<17, "sp">;

// Flags pseudo register is to model flags. SyncVM doesn't privide explicit
// access to it, but it's needed to track impilicit def and use of flages in
// codegen.
def Flags : SyncVMRegister<18, "#BAD#">;

let isAllocatable = 0 in
def SPR256 : RegisterClass<"SyncVM", [i256], 256, (add PC, SP, Flags)>;
