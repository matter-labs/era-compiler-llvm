//===-- SyncVMRegisterInfo.td - SyncVM Register defs -------*- tablegen -*-===//
//
//  Declarations that describe the SyncVM register file
//
//===----------------------------------------------------------------------===//

class SyncVMRegister<bits<4> num, string n> : Register<n> {
  field bits<4> Num = num;

  let Namespace = "SyncVM";
  let HWEncoding{3-0} = num;
}



//===----------------------------------------------------------------------===//
// General purpose registers
//===----------------------------------------------------------------------===//

def R1 : SyncVMRegister<1, "r1">;
def R2 : SyncVMRegister<2, "r2">;
def R3 : SyncVMRegister<3, "r3">;
def R4 : SyncVMRegister<4, "r4">;
def R5 : SyncVMRegister<5, "r5">;
def R6 : SyncVMRegister<6, "r6">;

def GR256 : RegisterClass<"SyncVM", [i256], 256, (add R1, R2, R3, R4, R5, R6)>;

//===----------------------------------------------------------------------===//
// Special registers
//===----------------------------------------------------------------------===//

// Contains 0, might be used as a destination reg to dispose the result.
// Note: Before register allocation r0 must always be dead. Live intervals
// analysis would assume any read from R0 as a read of the value put to the
// register (insted of 0), thus assuming the register is live.
// It seems that the simplest way to utilize r0 for 0 materialization is to
// replace inst dst, rN, (loadconst 0) with inst dst, rN, r0 in a peephole pass.
def R0 : SyncVMRegister<0, "r0">;
def Z256 : RegisterClass<"SyncVM", [i256], 256, (add R0)>;

def PC : SyncVMRegister<6, "pc">;
def SP : SyncVMRegister<7, "sp">;

// Flags pseudo register is to model flags. SyncVM doesn't privide explicit
// access to it, but it's needed to track impilicit def and use of flages in
// codegen.
def Flags : SyncVMRegister<8, "#BAD#">;

def GR16 : RegisterClass<"SyncVM", [i16], 16, (add PC, SP, Flags)>;
